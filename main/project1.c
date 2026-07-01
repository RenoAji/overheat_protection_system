#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "freertos/semphr.h"
#include "dht.h"
#include "driver/ledc.h"
static const char *TAG = "DHT";


#define BUZZER_FX_MODE          LEDC_LOW_SPEED_MODE
#define BUZZER_CHANNEL          LEDC_CHANNEL_0
#define BUZZER_TIMER            LEDC_TIMER_0
#define BUZZER_GPIO             21
#define LEDC_DUTY_RES           LEDC_TIMER_13_BIT // Set duty resolution to 13 bits
    #define LEDC_FREQUENCY          (1000)
#define LEDC_CLK_SRC            LEDC_AUTO_CLK

QueueHandle_t temp_queue;

#define BLINK_GPIO CONFIG_BLINK_GPIO 
#define DHT_GPIO 18
#define DHT_SENSOR_TYPE DHT_TYPE_DHT11


// PWM configuration for buzzer
static void buzzer_pwm_config(void)
{
    // Prepare and then apply the LEDC PWM timer configuration
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = BUZZER_FX_MODE,
        .duty_resolution  = LEDC_DUTY_RES,
        .timer_num        = BUZZER_TIMER,
        .freq_hz          = LEDC_FREQUENCY,  // Set output frequency at 4 kHz
        .clk_cfg          = LEDC_CLK_SRC,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // Prepare and then apply the LEDC PWM channel configuration
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = BUZZER_FX_MODE,
        .channel        = BUZZER_CHANNEL,
        .timer_sel      = BUZZER_TIMER,
        .gpio_num       = BUZZER_GPIO,
        .duty           = 0, // Set duty to 0%
        .hpoint         = 0,
#if CONFIG_PM_ENABLE
        .sleep_mode     = LEDC_SLEEP_MODE_KEEP_ALIVE,
#endif
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
}

// ISR
static void IRAM_ATTR emergency_handler(void *arg)
{
    static uint32_t last_interrupt_time = 0;
    uint32_t current_time = esp_log_timestamp();


    if (current_time - last_interrupt_time > 150) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;

        float triggered_temp_value = 100.0; 

        xQueueSendFromISR(temp_queue, &triggered_temp_value, &xHigherPriorityTaskWoken);

        if (xHigherPriorityTaskWoken == pdTRUE) {

            portYIELD_FROM_ISR();
        }
    }
    last_interrupt_time = current_time;
}

static void IRAM_ATTR reset_emergency_handler(void *arg)
{
    static uint32_t last_interrupt_time = 0;
    uint32_t current_time = esp_log_timestamp();

    if (current_time - last_interrupt_time > 150) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;

        float reset_temp_value = -100.0f; 

        xQueueSendFromISR(temp_queue, &reset_temp_value, &xHigherPriorityTaskWoken);

        if (xHigherPriorityTaskWoken == pdTRUE) {

            portYIELD_FROM_ISR();
        }
    }
    last_interrupt_time = current_time;
}

void sensor_task(void *pvParameters)
{
    gpio_set_pull_mode(DHT_GPIO, GPIO_PULLUP_ONLY);
    while(1) {
        float temperature = 0.0f;
        float humidity = 0.0f;

        esp_err_t result = dht_read_float_data(DHT_SENSOR_TYPE, DHT_GPIO, &humidity, &temperature);

        if (result == ESP_OK)
        {
            xQueueSend(temp_queue, &temperature, 0);
        }else{
            ESP_LOGE(TAG, "Could not read data from DHT sensor. Error code: %d", result);
        }
        
        
        vTaskDelay(pdMS_TO_TICKS(2000)); 
    }
    
}

void consumer(void *pvParameters){
    float received_value;
    gpio_reset_pin(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);

    int system_is_latched_in_emergency = 0;

    while (1)
    {
        
        if(xQueueReceive(temp_queue, &received_value, portMAX_DELAY) == pdPASS){
            if (received_value >= 40.0f)
            {
                system_is_latched_in_emergency = 1;
                ESP_LOGW(TAG, "Temperature is too high! System is in emergency state.");
                gpio_set_level(BLINK_GPIO, 1);
            }
            if (received_value == -100.0f)
            {
                system_is_latched_in_emergency = 0;
                ESP_LOGI(TAG, "Emergency reset signal received.");
            }
            

            if (system_is_latched_in_emergency)
            {
                ESP_LOGW(TAG, "System is in emergency state. Temperature: %.2f°C", received_value);
                ledc_set_duty(BUZZER_FX_MODE, BUZZER_CHANNEL, 4096); // Set duty to 50%
                ledc_update_duty(BUZZER_FX_MODE, BUZZER_CHANNEL);
                for(int i = 0; i < 5; i++) {
                    gpio_set_level(BLINK_GPIO, 1);
                    vTaskDelay(pdMS_TO_TICKS(100));
                    gpio_set_level(BLINK_GPIO, 0);
                    vTaskDelay(pdMS_TO_TICKS(100));
                }
                
            }
            else
            {
                ESP_LOGI(TAG, "Temperature: %.2f°C", received_value);
                gpio_set_level(BLINK_GPIO, 0);
                ledc_set_duty(BUZZER_FX_MODE, BUZZER_CHANNEL, 0); // Set duty to 0%
                ledc_update_duty(BUZZER_FX_MODE, BUZZER_CHANNEL);
            }
        }
    }
    
}

void app_main(void){
    temp_queue = xQueueCreate(5, sizeof(float));

    gpio_reset_pin(4);
    gpio_set_direction(4, GPIO_MODE_INPUT);

    // Set the pull-up resistor for GPIO 4 to ensure a stable high state when the button is not pressed
    gpio_set_pull_mode(4, GPIO_PULLUP_ONLY);

    // Set the interrupt type for GPIO 4 to trigger on the falling edge (button press)
    // GPIO_INTR_NEGEDGE -> trigger when voltage goes from high to low (button press)
    gpio_set_intr_type(4, GPIO_INTR_NEGEDGE);

    // Install the ISR service with default configuration
    gpio_install_isr_service(0);

    // Arguments: (Pin Number, The ISR Function Name, Optional arguments to pass to the ISR)
    // if pin 4 is pressed, the ISR will be triggered and the semaphore will be given
    gpio_isr_handler_add(4, emergency_handler, NULL);


    gpio_reset_pin(19);
    gpio_set_direction(19, GPIO_MODE_INPUT);

    // Set the pull-up resistor for GPIO 19 to ensure a stable high state when the button is not pressed
    gpio_set_pull_mode(19, GPIO_PULLUP_ONLY);

    // Set the interrupt type for GPIO 19 to trigger on the falling edge (button press)
    // GPIO_INTR_NEGEDGE -> trigger when voltage goes from high to low (button press)
    gpio_set_intr_type(19, GPIO_INTR_NEGEDGE);

    // Arguments: (Pin Number, The ISR Function Name, Optional arguments to pass to the ISR)
    // if pin 19 is pressed, the ISR will be triggered and the semaphore will be given
    gpio_isr_handler_add(19, reset_emergency_handler, NULL);

    //buzzer pwm config
    buzzer_pwm_config();


    xTaskCreatePinnedToCore(sensor_task, "producer", 4096, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(consumer, "consumer", 4096, NULL, 2, NULL, 0);
}
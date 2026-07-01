| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C5 | ESP32-C6 | ESP32-C61 | ESP32-H2 | ESP32-H21 | ESP32-H4 | ESP32-P4 | ESP32-S2 | ESP32-S3 | ESP32-S31 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | --------- | -------- | --------- | -------- | -------- | -------- | -------- | --------- |

# Overheat Protection System

This project reads temperature data from a DHT11 sensor and enters an emergency state when the temperature reaches a configured threshold. In that state, it drives an indicator LED and a buzzer using LEDC PWM. A reset button clears the latch and returns the system to normal operation.

## Features

- Periodic temperature sampling from a DHT11 sensor on GPIO 18.
- Emergency latch when the temperature reaches 40°C or higher.
- Buzzer output on GPIO 21 driven by LEDC PWM.
- Status LED on the GPIO configured by `CONFIG_BLINK_GPIO`.
- Interrupt-driven emergency trigger button on GPIO 4.
- Interrupt-driven emergency reset button on GPIO 19.

## Hardware Required

- An ESP32-family development board supported by ESP-IDF.
- A DHT11 temperature and humidity sensor.
- A buzzer connected to GPIO 21.
- An indicator LED connected to the GPIO selected by `CONFIG_BLINK_GPIO`.
- Two momentary push buttons:
  - GPIO 4 to trigger the emergency state.
  - GPIO 19 to clear the emergency state.

## Wiring Summary

- DHT11 data pin: GPIO 18
- Buzzer: GPIO 21
- Emergency button: GPIO 4, active low with internal pull-up
- Reset button: GPIO 19, active low with internal pull-up
- Status LED: configured through menuconfig with `CONFIG_BLINK_GPIO`

## Configuration

Open the project configuration menu with:

```sh
idf.py menuconfig
```

Make sure the GPIO assigned to `Blink GPIO` matches the LED you want to use as the status indicator.

If needed, update the target chip first:

```sh
idf.py set-target <chip_name>
```

## Build and Flash

Build, flash, and monitor the project with:

```sh
idf.py -p PORT flash monitor
```

To exit the serial monitor, press `Ctrl-]`.

## Runtime Behavior

The firmware creates a queue shared by the sensor task, the emergency button ISR, and the reset button ISR.

- The sensor task reads temperature and humidity every 2 seconds.
- If the temperature is 40°C or higher, the system latches into emergency mode.
- In emergency mode, the indicator LED blinks and the buzzer is enabled.
- Pressing the reset button sends a reset signal that clears the latch.

## Project Structure

- [main/project1.c](main/project1.c) contains the application logic.
- [main/CMakeLists.txt](main/CMakeLists.txt) registers the firmware source file.
- [main/idf_component.yml](main/idf_component.yml) declares the DHT and LED strip dependencies.

## Notes

The project currently uses the DHT11 sensor type defined in code. If you replace the sensor with another DHT variant, update the sensor type constant in [main/project1.c](main/project1.c).

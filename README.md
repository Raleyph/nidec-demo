# Nidec Motor Controller Demo

A minimal ESP-IDF firmware project for controlling Nidec 24H055M020 through a pulse-frequency modulation (PFM) signal using a **Seeed Studio XIAO ESP32-C6**.

The firmware generates a 50% duty-cycle square wave with the ESP32-C6 LEDC peripheral and provides a USB serial console for starting, adjusting, and stopping the motor with smooth frequency ramps.

## Features

- PFM output from **1,000 to 16,000 Hz**
- Fixed **50% duty cycle** while the output is enabled
- Smooth frequency transitions in steps of up to **100 Hz**
- Configurable ramp duration through the motor API
- Serial console with `freq`, `stop`, and `help` commands

## Hardware

| Item | Configuration |
| --- | --- |
| Development board | Seeed Studio XIAO ESP32-C6 |
| Motor | Nidec 24H055M020  |
| PFM output | GPIO2 |
| PWM peripheral | LEDC, low-speed mode, timer 0, channel 0 |
| Duty resolution | 10-bit |
| Active duty cycle | 50% (512/1024) |
| Frequency range | 1–16 kHz |
| Initial frequency | 1 kHz |
| Console | USB Serial/JTAG |
| Serial monitor speed | 115200 baud |

Connect **GPIO2** to the motor controller's compatible PFM/control input and connect the appropriate signal ground reference. The GPIO is a logic-level control signal, **not a motor power output**. Verify the controller's input voltage (usually 12V), electrical requirements, and grounding before connecting hardware.

## Getting started

### PlatformIO

The included `platformio.ini` selects the `seeed_xiao_esp32c6` board and the ESP-IDF framework.

From the project directory:

```bash
pio run
pio run --target upload
pio device monitor -b 115200
```

Select the appropriate upload/monitor port if PlatformIO does not detect it automatically.

### ESP-IDF

Alternatively, use a configured ESP-IDF development environment:

```bash
idf.py set-target esp32c6
idf.py build
idf.py -p <PORT> flash monitor
```

The project includes `sdkconfig.defaults` for USB Serial/JTAG console output.

## Serial console

On startup, the firmware prints:

```text
Motor console
Type 'help' to list commands

>
```

| Command | Description |
| --- | --- |
| `help` | List registered commands |
| `freq <1000-16000>` | Start the output or ramp to the requested frequency (Hz) |
| `stop` | Ramp down to 1 kHz and disable the output |

Example:

```text
> freq 4000
> freq 8000
> stop
```

Both console commands use a nominal **1,500 ms ramp duration**. Starting from a stopped state begins at 1 kHz before ramping to the requested frequency. Stopping ramps back to 1 kHz before disabling the signal. The actual duration can differ due to FreeRTOS tick granularity and scheduling.

## Motor control API

Declared in `src/motor.h`:

```c
esp_err_t motor_init(void);
esp_err_t motor_set_frequency(uint32_t frequency_hz);
esp_err_t motor_ramp_to_frequency(uint32_t target_frequency_hz,
                                  uint32_t ramp_time_ms);
esp_err_t motor_ramp_stop(uint32_t ramp_time_ms);
esp_err_t motor_stop_immediately(void);
bool motor_is_running(void);
uint32_t motor_get_frequency(void);
```

`motor_set_frequency()` updates the configured frequency but does not itself enable a stopped output. Ramp durations must be greater than zero, and target frequencies must be within the supported range.

## Current limitations

- The console's `freq` and `stop` handlers **do not propagate** motor API errors to the console.
- Console argument parsing uses `strtoul()` without validating the entire input string.
- Frequency ramping is implemented with `vTaskDelay()` inside the calling task. **Ramps are blocking**: the console cannot process another command until the current ramp completes.
- The firmware has no motor feedback, speed measurement, fault detection, or closed-loop regulation.
- There is no dedicated hardware enable/brake output or emergency-stop console command.
- Motor compatibility and physical wiring must be verified for the specific controller.
- The motor module also exposes `motor_stop_immediately()`, which disables the output without a ramp, but this operation is **not registered as a console command**.

## License

Released under the [MIT License](LICENSE).

Copyright © 2025 Raleyph

Designed by [Raleyph](https://github.com/Raleyph) with the support of VOLVI

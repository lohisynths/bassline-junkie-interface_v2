# Bassline Junkie Interface

Zephyr firmware for the STM32 Nucleo-F411RE that combines:

- onboard status LED blinking through the board `led0` alias
- PCA9685-based LED output control through the `LEDS` class
- CD4067 input scanning through an out-of-tree Zephyr driver
- serial logging over the ST-LINK virtual COM port

## Modules

- `LEDS`: wraps the configured PCA9685 controllers and exposes channel-based LED control
- `cd4067`: out-of-tree Zephyr module providing the CD4067 GPIO multiplexer driver
- `main.cpp`: initializes the board LED, scans all CD4067 channels, and runs a simple LED chase pattern across all PCA9685 channels

## Runtime Overview

- The application configures the onboard `led0` GPIO as a heartbeat indicator.
- The `LEDS` class verifies all configured PCA9685 devices, clears them, and advances a single lit output in a chase loop.
- The CD4067 driver selects each of the 16 mux channels and samples the shared `SIG` input.
- The current mux state is logged periodically as a 16-bit active-channel mask.
- Status and error messages are emitted over the ST-LINK virtual serial port.

## Developer Notes

- The application entrypoint is `src/main.cpp`.
- PWM LED control is implemented in `src/LEDS.cpp` and declared in `src/LEDS.h`.
- The out-of-tree CD4067 module is discovered through `../cd4067` from the app `CMakeLists.txt`.
- Hardware description for the PCA9685 devices and the CD4067 instance lives in `app.overlay`.

For build, flash, documentation generation, and current hardware wiring, see
the repository `README.md`.

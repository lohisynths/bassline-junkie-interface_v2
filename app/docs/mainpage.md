# Bassline Junkie Interface

Zephyr firmware for the STM32 Nucleo-F411RE that combines:

- onboard status LED blinking through the board `led0` alias
- discrete encoder GPIO input sampling through the `GPIO` class
- PCA9685-based LED output control through the `LEDS` class
- CD4067 input scanning through the `MUX` class and an out-of-tree Zephyr driver
- aggregated input caching through the `InputController` class
- serial logging over the ST-LINK virtual COM port

## Modules

- `GPIO`: wraps the configured discrete GPIO inputs and exposes per-pin and bitmask reads
- `InputController`: owns the `MUX` and `GPIO` facades and exposes one flat cached input-state table
- `LEDS`: wraps the configured PCA9685 controllers and exposes channel-based LED control
- `MUX`: wraps the configured CD4067 devices, scans their inputs, and logs one active-channel mask per mux
- `cd4067`: out-of-tree Zephyr module providing the CD4067 GPIO multiplexer driver
- `main.cpp`: initializes the board LED, initializes the `InputController` and `LEDS` classes, and runs a simple LED chase pattern

## Runtime Overview

- The application configures the onboard `led0` GPIO as a heartbeat indicator.
- The `GPIO` class configures six active-low discrete encoder inputs and can return their combined active-state bitmask.
- The `MUX` class manages four configured CD4067 instances that share the same select lines.
- Each mux uses one dedicated `SIG` input:
  `MUX0` on `PA0`, `MUX1` on `PA1`, `MUX2` on `PA4`, and `MUX3` on `PB0`.
- The `MUX` class scans each configured CD4067 by selecting all 16 channels and sampling its `SIG` input.
- The `InputController` class reads all mux masks plus the discrete GPIO mask into one cached array.
- The `LEDS` class verifies all configured PCA9685 devices, clears them, and advances a single lit output in a chase loop.
- The main loop logs the cached input masks by iterating one flat array with `InputController::state_count` and `InputController::state()`.
- Status and error messages are emitted over the ST-LINK virtual serial port.

## Developer Notes

- The application entrypoint is `src/main.cpp`.
- Discrete GPIO input handling is implemented in `src/GPIO.cpp` and declared in `src/GPIO.h`.
- Input aggregation is implemented in `src/InputController.cpp` and declared in `src/InputController.h`.
- PWM LED control is implemented in `src/LEDS.cpp` and declared in `src/LEDS.h`.
- CD4067 aggregation is implemented in `src/MUX.cpp` and declared in `src/MUX.h`.
- The out-of-tree CD4067 module is discovered through `../cd4067` from the app `CMakeLists.txt`.
- Hardware description for the PCA9685 devices, CD4067 instances, and discrete GPIO inputs lives in `app.overlay`.

For build, flash, documentation generation, and current hardware wiring, see
the repository `README.md`.

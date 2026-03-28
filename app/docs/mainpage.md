# Bassline Junkie Interface

Zephyr firmware for the STM32 Nucleo-F411RE that combines:

- onboard status LED blinking through the board `led0` alias
- discrete GPIO input sampling through the `GPIO` class
- PCA9685-based LED output control through the `LEDSController` class
- CD4067 input scanning through the `MUX` class and an out-of-tree Zephyr driver
- aggregated input caching through the `InputController` class
- button decoding from cached input bits through the `Button` class
- quadrature decoding from cached CD4067 channels through the `Encoder` class
- reusable knob composition through the `Knob` class
- serial logging over the ST-LINK virtual COM port

## Modules

- `Button`: binds to one cached input state through `Config`, samples one configured active-low channel as a button input, reports per-update state changes through `button_msg`, exposes explicit LED control through `set_led_val()`, and reports the current button state through `get_state()`
- `Encoder`: binds to one cached mux state, samples two configured channels as quadrature phase A/B, and reports per-update delta plus accumulated position
- `GPIO`: wraps the configured discrete GPIO inputs and exposes per-pin and bitmask reads
- `InputController`: owns the `MUX` and `GPIO` facades, exposes one flat cached input-state table plus `input_count`, can log current mux and GPIO states for debugging, and can report cached input-bit transitions between successive updates
- `Knob`: owns one internal `Encoder`, reads one configured active-low button bit directly from cached input state, binds the knob UI to one contiguous LED range, maintains one `0..127` value from encoder movement, renders that value on the LED segment, and exposes the knob button state
- `LEDSController`: wraps the configured PCA9685 controllers and exposes channel-based LED control
- `MUX`: wraps the configured CD4067 devices, scans their inputs, and logs one active-channel mask per mux in hex or binary form
- `utils`: provides shared helpers such as 16-bit mask-to-binary-string formatting used by debug logging
- `cd4067`: out-of-tree Zephyr module providing the CD4067 GPIO multiplexer driver
- `main.cpp`: initializes the board LED, starts an input thread that constructs `InputController`, `LEDSController`, `Button` and `Knob` arrays as plain locals, compares current and previous button state to log transitions, logs cached input-bit transitions after each input refresh, and runs four knob indicators across the first 40 LEDs

## Runtime Overview

- The application configures the onboard `led0` GPIO as a heartbeat indicator, toggles it once per second, and logs `Heartbeat: LED blink running` every 10 seconds.
- The `GPIO` class configures the discrete active-low GPIO inputs, can return their combined active-state bitmask, and can log that mask in hex or binary form.
- The `MUX` class manages four configured CD4067 instances that share the same select lines.
- Each mux uses one dedicated `SIG` input:
  `MUX0` on `PA0`, `MUX1` on `PA1`, `MUX2` on `PA4`, and `MUX3` on `PB0`.
- The `MUX` class scans each configured CD4067 by selecting all 16 channels and sampling its `SIG` input.
- The `InputController` class reads all mux masks plus the discrete GPIO mask into one cached array, exposes `input_count` for clients that index that table, delegates debug state logging to `MUX::log_state()` and `GPIO::log_state()`, and exposes `log_mux_changes()` to report which cached input bits changed between successive updates.
- The `Button` class binds to one cached input state through `Config`, uses one configured active-low channel as a button source, exposes `button_msg` change flags through `update(msg)`, exposes `set_led_val()` for explicit LED updates, and reports the current button state through `get_state()`.
- The current application configures four standalone buttons through `button_configs[]`; the current entries use mux index `0`, channels `12`, `13`, `14`, and `15`, with LED channels `40`, `41`, `42`, and `43`.
- The `Encoder` class binds to one cached mux state, uses two configured channels as quadrature phase A/B, and converts valid AB transitions into signed movement.
- The current application configures four encoders on mux index `0` with phase pairs `1/2`, `4/5`, `7/8`, and `10/11`.
- The `LEDSController` class verifies all configured PCA9685 devices and exposes channel-based brightness control across all PCA9685 outputs.
- Shared utility code in `utils.cpp` formats 16-bit input masks as fixed-width binary strings for debug output.
- Each `Knob` owns its current encoder helper, reads one configured active-low button bit from the cached input table, binds the knob UI to LED channels `0..9`, `10..19`, `20..29`, or `30..39`, maintains one internal value in the range `0..127` from encoder deltas, projects that value onto the LED segment without wraparound, and exposes the current knob-button state through `get_state()`.
- A dedicated input thread constructs `InputController`, `LEDSController`, `Button` and `Knob` arrays as plain local objects, then refreshes the cached inputs, calls `log_mux_changes()` to report input transitions, updates each standalone button, sets its LED according to the current button state when a transition occurs, logs its transitions, updates each knob, compares the current and previous knob-button state to log `Knob N button pressed` / `released` transitions, and logs the current knob value when movement changes that value.
- Status and error messages are emitted over the ST-LINK virtual serial port.

## Developer Notes

- Button decoding is implemented in `src/Button.cpp` and declared in `src/Button.h`.
- Quadrature decoding is implemented in `src/Encoder.cpp` and declared in `src/Encoder.h`.
- Knob composition is implemented in `src/Knob.cpp` and declared in `src/Knob.h`.
- The application entrypoint is `src/main.cpp`.
- Discrete GPIO input handling is implemented in `src/GPIO.cpp` and declared in `src/GPIO.h`.
- Input aggregation is implemented in `src/InputController.cpp` and declared in `src/InputController.h`.
- PWM LED control is implemented in `src/LEDS.cpp` and declared in `src/LEDS.h`.
- CD4067 aggregation is implemented in `src/MUX.cpp` and declared in `src/MUX.h`.
- Shared binary-mask formatting helpers are implemented in `src/utils.cpp` and declared in `src/utils.h`.
- The out-of-tree CD4067 module is discovered through `../cd4067` from the app `CMakeLists.txt`.
- Hardware description for the PCA9685 devices, CD4067 instances, and discrete GPIO inputs lives in `app.overlay`.

For build, flash, documentation generation, and current hardware wiring, see
the repository `README.md`.

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
- ADSR control-surface composition through the `ADSR` block class
- FLT control-surface composition through the `FLT` block class
- preset display composition through the `LED_DISP` block class
- LFO control-surface composition through the `LFO` block class
- MOD control-surface composition through the `MOD` block class
- OSC control-surface composition through the `OSC` block class
- flash-backed preset persistence through the `PresetStore` class and `PresetSnapshot` model
- serial logging over the ST-LINK virtual COM port at `1000000` baud

## Modules

- `Button`: binds to one cached input state through `Config`, samples one configured active-low channel as a button input, reports per-update state changes through `button_msg`, exposes explicit LED control through `set_led_val()`, and reports the current button state through `get_state()`
- `ADSR`: owns the current standalone button set and knob set, stores their configuration tables plus three banked knob-value sets, initializes them against shared `InputController` and `LEDSController` objects, applies selector LED updates, and emits the current transition logs
- `FLT`: owns three radio buttons and three knobs, stores their configuration tables, initializes them against shared `InputController` and `LEDSController` objects, applies radio-button LED updates, and emits the current transition logs
- `LED_DISP`: owns one knob plus one three-digit active-low seven-segment display, stores the preset-selector knob configuration plus the legacy digit segment patterns, initializes itself against shared `InputController`, `LEDSController`, `PresetStore`, and block objects, renders blank-leading preset numbers on the display, and handles short-press load plus long-press save gestures
- `LFO`: owns three selector buttons, five radio buttons, and one knob, stores their configuration tables plus three banked knob-value sets and per-bank radio-button selections, initializes them against shared `InputController` and `LEDSController` objects, applies selector LED updates, and emits the current transition logs
- `MOD`: owns six selector buttons and one knob, stores their configuration tables plus six banked knob-value sets, initializes them against shared `InputController` and `LEDSController` objects, applies selector LED updates, and emits the current transition logs
- `OSC`: owns three selector buttons and five knobs, stores their configuration tables plus three banked knob-value sets, initializes them against shared `InputController` and `LEDSController` objects, applies selector LED updates, and emits the current transition logs
- `Encoder`: binds to one cached mux state, samples two configured channels as quadrature phase A/B, and reports per-update delta plus accumulated position
- `GPIO`: wraps the configured discrete GPIO inputs and exposes per-pin and bitmask reads
- `InputController`: owns the `MUX` and `GPIO` facades, exposes one flat cached input-state table plus `input_count`, and provides optional state-dump and input-transition debug helpers
- `Knob`: owns one internal `Encoder`, reads one configured active-low button bit directly from cached input state, binds the knob UI to one contiguous LED range, maintains one `0..127` value from encoder movement, supports explicit value recall through `set_value()`, renders that value on the LED segment, and exposes the knob button state
- `LEDSController`: wraps the configured PCA9685 controllers and exposes channel-based LED control
- `MUX`: wraps the configured CD4067 devices, scans their inputs, and logs one active-channel mask per mux in hex or binary form
- `PresetSnapshot`: provides the durable schema for the `ADSR`, `FLT`, `LFO`, `MOD`, and `OSC` block states stored in one preset slot while leaving bank selectors live
- `PresetStore`: owns the flash-backed 128-slot preset image, validates it with CRC32, returns default state for unsaved slots, and rewrites the dedicated preset partition on save
- `utils`: provides shared helpers such as 16-bit mask-to-binary-string formatting used by debug logging
- `cd4067`: out-of-tree Zephyr module providing the CD4067 GPIO multiplexer driver
- `main.cpp`: initializes the board LED, starts an input thread that constructs `InputController`, `LEDSController`, one `ADSR` block, one `FLT` block, one `LFO` block, one `MOD` block, one `OSC` block, one `PresetStore`, and one `LED_DISP` block as plain locals, and runs the block update loop alongside the heartbeat LED

## Runtime Overview

- The application uses the onboard `led0` as a heartbeat and emits status logs over the ST-LINK virtual serial port at `1000000` baud.
- A dedicated input thread builds `InputController`, `LEDSController`, the `ADSR`, `FLT`, `LFO`, `MOD`, and `OSC` control blocks, the `PresetStore`, and the `LED_DISP` preset selector, then repeatedly refreshes inputs and updates all blocks.
- `InputController` merges the CD4067 mux scans and discrete GPIO reads into one cached state table. `Button`, `Encoder`, and `Knob` build on that cache to provide reusable input primitives.
- `ADSR`, `LFO`, `MOD`, and `OSC` expose banked controls. Bank switches recall stored knob state and update the related LEDs.
- `FLT` provides a non-banked radio-button group with three knobs.
- `PresetStore` reserves one flash partition for 128 preset slots and returns the all-zero default surface when a slot has never been saved.
- `LED_DISP` provides one knob plus an active-low three-digit seven-segment display that shows the selected preset number in the range `0..127`.
- Preset `0` is auto-loaded on boot. Short display-knob presses load the selected preset, and long display-knob presses save the current full-surface state into that slot.
- Preset loads restore bank contents but do not force a different selected bank in `ADSR`, `LFO`, `MOD`, or `OSC`.
- `LEDSController` drives the PCA9685 outputs used for selector LEDs, knob segments, and the display.
- Runtime logs cover heartbeat activity, button transitions, radio selections, bank changes, knob movement, and preset save/load actions.

## Developer Notes

- ADSR block composition is implemented in `src/blocks/ADSR.cpp` and declared in `src/blocks/ADSR.h`.
- FLT block composition is implemented in `src/blocks/FLT.cpp` and declared in `src/blocks/FLT.h`.
- LED display block composition is implemented in `src/blocks/LED_DISP.cpp` and declared in `src/blocks/LED_DISP.h`.
- LFO block composition is implemented in `src/blocks/LFO.cpp` and declared in `src/blocks/LFO.h`.
- MOD block composition is implemented in `src/blocks/MOD.cpp` and declared in `src/blocks/MOD.h`.
- OSC block composition is implemented in `src/blocks/OSC.cpp` and declared in `src/blocks/OSC.h`.
- Button decoding is implemented in `src/Button.cpp` and declared in `src/Button.h`.
- Quadrature decoding is implemented in `src/Encoder.cpp` and declared in `src/Encoder.h`.
- Knob composition is implemented in `src/Knob.cpp` and declared in `src/Knob.h`.
- The application entrypoint is `src/main.cpp`.
- Discrete GPIO input handling is implemented in `src/GPIO.cpp` and declared in `src/GPIO.h`.
- Input aggregation is implemented in `src/InputController.cpp` and declared in `src/InputController.h`.
- PWM LED control is implemented in `src/LEDS.cpp` and declared in `src/LEDS.h`.
- Flash-backed preset persistence is implemented in `src/PresetStore.cpp` and declared in `src/PresetStore.h`.
- The durable preset schema is declared in `src/PresetSnapshot.h`.
- CD4067 aggregation is implemented in `src/MUX.cpp` and declared in `src/MUX.h`.
- Shared binary-mask formatting helpers are implemented in `src/utils.cpp` and declared in `src/utils.h`.
- The out-of-tree CD4067 module is discovered through `../cd4067` from the app `CMakeLists.txt`.
- The app `CMakeLists.txt` adds `src` to the target include path so nested sources can include project headers without relative `../` prefixes.
- Hardware description for the PCA9685 devices, CD4067 instances, discrete GPIO inputs, and the dedicated `code_partition` plus `preset_partition` lives in `app.overlay`.

For build, flash, documentation generation, and current hardware wiring, see
the repository `README.md`.

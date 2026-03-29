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
- polling-based app transport through the `UART` class on `USART1`
- MIDI channel-message helpers through the `MIDI` class layered on `UART`
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
- `LED_DISP`: owns one knob plus one three-digit active-low seven-segment display, stores the preset-selector knob configuration plus the legacy digit segment patterns, initializes itself against shared `InputController`, `LEDSController`, `PresetStore`, and block objects, renders blank-leading preset numbers on the display, uses a reduced-resolution encoder step for preset browsing, saves on hold timeout, and handles temporary blink feedback for save confirmation and browse-timeout restore
- `LFO`: owns three selector buttons, five radio buttons, and one knob, stores their configuration tables plus three banked knob-value sets and per-bank radio-button selections, initializes them against shared `InputController` and `LEDSController` objects, applies selector LED updates, and emits the current transition logs
- `MOD`: owns six selector buttons and one knob, stores their configuration tables plus six selector groups with seventeen virtual target banks each, remembers one linked target offset per group, tracks long-press preview state for every selector button, initializes them against shared `InputController`, `LEDSController`, and optional `MIDI` objects, applies selector LED updates, emits MOD MIDI Control Change messages, and emits the current transition logs
- `OSC`: owns three selector buttons and five knobs, stores their configuration tables plus three banked knob-value sets, initializes them against shared `InputController` and `LEDSController` objects, applies selector LED updates, and emits the current transition logs
- `Encoder`: binds to one cached mux state, samples two configured channels as quadrature phase A/B, and reports per-update delta plus accumulated position
- `GPIO`: wraps the configured discrete GPIO inputs and exposes per-pin and bitmask reads
- `InputController`: owns the `MUX` and `GPIO` facades, exposes one flat cached input-state table plus `input_count`, and provides optional state-dump and input-transition debug helpers
- `Knob`: owns one internal `Encoder`, reads one configured active-low button bit directly from cached input state, binds the knob UI to one contiguous LED range, maintains one `0..127` value from encoder movement, supports explicit value recall through `set_value()`, supports temporary LED-only preview rendering, can divide raw encoder edges before exposing one visible step, renders that value on the LED segment, and exposes the knob button state
- `LEDSController`: wraps the configured PCA9685 controllers and exposes channel-based LED control
- `MUX`: wraps the configured CD4067 devices, scans their inputs, and logs one active-channel mask per mux in hex or binary form
- `PresetSnapshot`: provides the durable schema for the `ADSR`, `FLT`, `LFO`, `MOD`, and `OSC` block states stored in one preset slot while leaving bank selectors live, including the expanded MOD virtual-bank state
- `PresetStore`: owns the flash-backed 128-slot preset log, validates it with CRC32, returns default state for unsaved slots, treats older snapshot formats as incompatible, and appends one flash record on save
- `UART`: wraps the app-owned `USART1` device on `PA9`/`PA10`, exposes polling writes plus non-blocking reads, and keeps the Zephyr console on `USART2`
- `MIDI`: binds to one initialized `UART` transport and emits Note On, Note Off, and Control Change channel messages
- `utils`: provides shared helpers such as 16-bit mask-to-binary-string formatting used by debug logging
- `cd4067`: out-of-tree Zephyr module providing the CD4067 GPIO multiplexer driver
- `main.cpp`: initializes the board LED, starts an input thread that constructs `InputController`, `LEDSController`, one `ADSR` block, one `FLT` block, one `LFO` block, one `MOD` block, one `OSC` block, one `PresetStore`, and one `LED_DISP` block as plain locals, and runs the block update loop alongside the heartbeat LED

## Runtime Overview

- The application uses the onboard `led0` as a heartbeat and emits status logs over the ST-LINK virtual serial port at `1000000` baud.
- The application also owns a separate polling UART transport on `USART1` using `PA9` for TX and `PA10` for RX at `115200` baud.
- `main.cpp` initializes `UART` during boot and sends `Bassline Junkie Interface UART1 ready` on `USART1`.
- A dedicated input thread builds `InputController`, `LEDSController`, the `ADSR`, `FLT`, `LFO`, `MOD`, and `OSC` control blocks, the `PresetStore`, and the `LED_DISP` preset selector, then repeatedly refreshes inputs and updates all blocks.
- `InputController` merges the CD4067 mux scans and discrete GPIO reads into one cached state table. `Button`, `Encoder`, and `Knob` build on that cache to provide reusable input primitives.
- `ADSR`, `LFO`, and `OSC` expose banked controls. `MOD` exposes six selector groups, each of which recalls one remembered virtual target bank when the group changes.
- ADSR knob value changes emit MIDI Control Change messages on channel `0`, and the banked ADSR latch emits `0` or `127`.
- FLT knob `0`, FLT knob `1`, and the FLT radio selection emit MIDI Control Change messages on channel `0`; FLT knob `2` is ignored by MIDI.
- LFO banked radio selection and the LFO knob emit MIDI Control Change messages on channel `0`; the fifth LFO radio button is ignored by MIDI.
- OSC knob value changes also emit MIDI Control Change messages on channel `0`, using CC `0..14` across the three OSC banks.
- MOD knob value changes emit MIDI Control Change messages on channel `1`, using CC `0..101` across the 102 MOD virtual banks, and MOD bank recalls also resend the currently active virtual-bank value.
- While the MOD knob is held, newly pressed OSC knobs map onto MOD target offsets `0..14`, FLT knob `0` maps to offset `15`, FLT knob `1` maps to offset `16`, and FLT knob `2` is ignored.
- Each MOD selector group stores one remembered target offset and one MOD knob value for every virtual bank `((group * 17) + target_offset)`.
- Preset loads also resend the full MOD MIDI Control Change snapshot, including all 102 MOD virtual banks.
- Long-pressing any MOD selector button for `1000 ms` temporarily previews that group's MOD values on the OSC knob LEDs for the current OSC bank and on FLT knobs `0` and `1`; the preview changes LEDs only and does not mutate stored values.
- Preset loads also resend the LFO MIDI Control Change snapshot.
- Preset loads also resend the FLT MIDI Control Change snapshot.
- Preset loads also resend the full ADSR MIDI Control Change snapshot, including all three ADSR banks.
- Preset loads also resend the full OSC MIDI Control Change snapshot, including all three OSC banks.
- `FLT` provides a non-banked radio-button group with three knobs.
- `PresetStore` reserves one flash partition for 128 preset slots, stores saves as an append-only log, and returns the all-zero default surface when a slot has never been saved.
- `LED_DISP` provides one knob plus an active-low three-digit seven-segment display that shows the selected preset number in the range `0..127`.
- The preset encoder uses a coarser step size than the other knobs so casual button presses are less likely to move the selected slot.
- Preset `0` is auto-loaded on boot. Short display-knob presses load the selected preset on release, and holding the display knob for `1000 ms` saves the current full-surface state immediately on timeout.
- After a hold-triggered save, `LED_DISP` blinks briefly as confirmation.
- Browsing away from the active preset without pressing load or save for `5000 ms` makes the display blink, then restore the active preset number.
- Preset loads restore bank contents but do not force a different selected selector group or bank in `ADSR`, `LFO`, `MOD`, or `OSC`.
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
- App-owned UART transport is implemented in `src/UART.cpp` and declared in `src/UART.h`.
- MIDI transport helpers are implemented in `src/MIDI.cpp` and declared in `src/MIDI.h`.
- The durable preset schema is declared in `src/PresetSnapshot.h`.
- CD4067 aggregation is implemented in `src/MUX.cpp` and declared in `src/MUX.h`.
- Shared binary-mask formatting helpers are implemented in `src/utils.cpp` and declared in `src/utils.h`.
- The out-of-tree CD4067 module is discovered through `../cd4067` from the app `CMakeLists.txt`.
- The app `CMakeLists.txt` adds `src` to the target include path so nested sources can include project headers without relative `../` prefixes.
- Hardware description for the PCA9685 devices, CD4067 instances, discrete GPIO inputs, and the dedicated `code_partition` plus `preset_partition` lives in `app.overlay`.

For build, flash, documentation generation, and current hardware wiring, see
the repository `README.md`.

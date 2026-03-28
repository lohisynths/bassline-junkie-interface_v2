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
- LFO control-surface composition through the `LFO` block class
- MOD control-surface composition through the `MOD` block class
- OSC control-surface composition through the `OSC` block class
- serial logging over the ST-LINK virtual COM port

## Modules

- `Button`: binds to one cached input state through `Config`, samples one configured active-low channel as a button input, reports per-update state changes through `button_msg`, exposes explicit LED control through `set_led_val()`, and reports the current button state through `get_state()`
- `ADSR`: owns the current standalone button set and knob set, stores their configuration tables plus three banked knob-value sets, initializes them against shared `InputController` and `LEDSController` objects, applies selector LED updates, and emits the current transition logs
- `FLT`: owns three radio buttons and three knobs, stores their configuration tables, initializes them against shared `InputController` and `LEDSController` objects, applies radio-button LED updates, and emits the current transition logs
- `LFO`: owns three selector buttons, five radio buttons, and one knob, stores their configuration tables plus three banked knob-value sets and per-bank radio-button selections, initializes them against shared `InputController` and `LEDSController` objects, applies selector LED updates, and emits the current transition logs
- `MOD`: owns six selector buttons and one knob, stores their configuration tables plus six banked knob-value sets, initializes them against shared `InputController` and `LEDSController` objects, applies selector LED updates, and emits the current transition logs
- `OSC`: owns three selector buttons and five knobs, stores their configuration tables plus three banked knob-value sets, initializes them against shared `InputController` and `LEDSController` objects, applies selector LED updates, and emits the current transition logs
- `Encoder`: binds to one cached mux state, samples two configured channels as quadrature phase A/B, and reports per-update delta plus accumulated position
- `GPIO`: wraps the configured discrete GPIO inputs and exposes per-pin and bitmask reads
- `InputController`: owns the `MUX` and `GPIO` facades, exposes one flat cached input-state table plus `input_count`, and provides optional state-dump and input-transition debug helpers
- `Knob`: owns one internal `Encoder`, reads one configured active-low button bit directly from cached input state, binds the knob UI to one contiguous LED range, maintains one `0..127` value from encoder movement, supports explicit value recall through `set_value()`, renders that value on the LED segment, and exposes the knob button state
- `LEDSController`: wraps the configured PCA9685 controllers and exposes channel-based LED control
- `MUX`: wraps the configured CD4067 devices, scans their inputs, and logs one active-channel mask per mux in hex or binary form
- `utils`: provides shared helpers such as 16-bit mask-to-binary-string formatting used by debug logging
- `cd4067`: out-of-tree Zephyr module providing the CD4067 GPIO multiplexer driver
- `main.cpp`: initializes the board LED, starts an input thread that constructs `InputController`, `LEDSController`, one `ADSR` block, one `FLT` block, one `LFO` block, one `MOD` block, and one `OSC` block as plain locals, and runs the block update loop alongside the heartbeat LED

## Runtime Overview

- The application configures the onboard `led0` GPIO as a heartbeat indicator, toggles it once per second, and logs `Heartbeat: LED blink running` every 10 seconds.
- The `GPIO` class configures the discrete active-low GPIO inputs, can return their combined active-state bitmask, and can log that mask in hex or binary form.
- The `MUX` class manages four configured CD4067 instances that share the same select lines.
- Each mux uses one dedicated `SIG` input:
  `MUX0` on `PA0`, `MUX1` on `PA1`, `MUX2` on `PA4`, and `MUX3` on `PB0`.
- The `MUX` class scans each configured CD4067 by selecting all 16 channels and sampling its `SIG` input.
- The `InputController` class reads all mux masks plus the discrete GPIO mask into one cached array, exposes `input_count` for clients that index that table, delegates debug state logging to `MUX::log_state()` and `GPIO::log_state()`, and exposes `log_mux_changes()` as an optional helper to report which cached input bits changed between successive updates.
- The `Button` class binds to one cached input state through `Config`, uses one configured active-low channel as a button source, exposes `button_msg` change flags through `update(msg)`, exposes `set_led_val()` for explicit LED updates, and reports the current button state through `get_state()`.
- The `ADSR` block stores the current control-surface configuration tables in one place, owns the runtime `Button` and `Knob` instances for that block, and maintains three banked knob-value sets.
- The current `ADSR` button entries use mux index `0`, channels `15`, `14`, `13`, and `12`, with LED channels `43`, `42`, `41`, and `40`.
- ADSR buttons `0`, `1`, and `2` act as latched selector buttons for knob banks `0`, `1`, and `2`.
- ADSR button `3` is a latched toggle whose stored state is independent in each bank.
- The `FLT` block stores its own control-surface configuration tables, owns three radio `Button` instances plus three `Knob` instances, and does not maintain banked state.
- The current `FLT` button entries use mux index `3`, channels `15`, `14`, and `13`, with LED channels `174`, `173`, and `172`.
- FLT buttons act as radio buttons with one active selection reflected on the LEDs at a time.
- The current `FLT` knob entries use mux index `3` with button/encoder bindings `4/5/6`, `7/8/9`, and `10/11/12`, and LED channels `144..155`, `160..169`, and no LED segment for the third knob.
- The `LFO` block stores its own control-surface configuration tables, owns three selector `Button` instances, five radio `Button` instances, and one `Knob` instance, and maintains three banked knob values plus one radio-button selection per bank independently from `ADSR`, `MOD`, and `OSC`.
- The current `LFO` button entries use mux index `3`, channel `0`, and mux index `2`, channels `15`, `14`, `13`, `12`, `11`, `10`, and `9`, with LED channels `158`, `157`, `156`, `142`, `141`, `140`, `139`, and `138`.
- LFO buttons `0..2` act as latched selector buttons for knob banks `0..2`.
- LFO buttons `3..7` act as radio buttons with one stored selection per bank.
- The `MOD` block stores its own control-surface configuration tables, owns six selector `Button` instances plus one `Knob` instance, and maintains six banked knob values independently from `ADSR` and `OSC`.
- The current `MOD` button entries use mux index `2`, channels `3`, `4`, `5`, `6`, `7`, and `8`, with LED channels `122`, `123`, `124`, `125`, `126`, and `127`.
- MOD buttons `0..5` act as latched selector buttons for knob banks `0..5`.
- The `OSC` block stores its own control-surface configuration tables, owns three selector `Button` instances plus five `Knob` instances, and maintains three banked knob-value sets independently from `ADSR`.
- The current `OSC` button entries use mux index `3`, channels `3`, `2`, and `1`, with LED channels `110`, `109`, and `108`.
- OSC buttons `0`, `1`, and `2` act as latched selector buttons for knob banks `0`, `1`, and `2`.
- The `Encoder` class binds to one cached mux state, uses two configured channels as quadrature phase A/B, and converts valid AB transitions into signed movement.
- The current `ADSR` knob entries use mux index `0` with encoder phase pairs `1/2`, `4/5`, `7/8`, and `10/11`.
- The current `FLT` knob entries use mux index `3` with encoder phase pairs `5/6`, `8/9`, and `11/12`.
- The current `LFO` knob entry uses cached input state index `4` with encoder phase pair `1/2`, button bit `0`, and LED channels `128..137`.
- The current `MOD` knob entry uses mux index `2` with encoder phase pair `1/2`, button bit `0`, and LED channels `112..121`.
- The current `OSC` knob entries use mux index `1` with encoder phase pairs `13/14`, `10/11`, `7/8`, `4/5`, and `1/2`.
- The `LEDSController` class verifies all configured PCA9685 devices and exposes channel-based brightness control across all PCA9685 outputs.
- Shared utility code in `utils.cpp` formats 16-bit input masks as fixed-width binary strings for debug output.
- Each `Knob` owns its current encoder helper, reads one configured active-low button bit from the cached input table, binds the knob UI to LED channels `0..9`, `10..19`, `20..29`, or `30..39`, maintains one internal value in the range `0..127` from encoder deltas, supports immediate value recall from `ADSR`, projects that value onto the LED segment without wraparound, and exposes the current knob-button state through `get_state()`.
- The current `FLT` knob indicators use LED channels `144..155`, `160..169`, and no LED segment for the third knob.
- The current `LFO` knob indicator uses LED channels `128..137`.
- The current `MOD` knob indicator uses LED channels `112..121`.
- The current `OSC` knob indicators use LED channels `96..105`, `78..87`, `68..77`, `58..67`, and `48..57`.
- A dedicated input thread constructs `InputController`, `LEDSController`, one `ADSR` block, one `FLT` block, one `LFO` block, one `MOD` block, and one `OSC` block as plain local objects, then refreshes the cached inputs and calls all five block update routines. The `ADSR` block keeps bank `0` selected on boot, lights only the active selector button LED plus the current bank's latched button-3 LED when enabled, recalls four stored knob values whenever buttons `0..2` change the active bank, recalls the active bank's latched button-3 state on bank switch, toggles that stored button-3 state on button-3 press, updates each knob, compares the current and previous knob-button state to log `Knob N button pressed` / `released` transitions, and logs knob movement as `Bank N knob M position=V` when movement changes the current value in the active bank.
- The `FLT` block also keeps radio button `0` selected on boot, lights only the active radio-button LED, updates its three knobs, compares the current and previous knob-button state to log `FLT knob N button pressed` / `released` transitions, logs radio-button selection as `FLT radio button N selected`, and logs knob movement as `FLT knob N position=V` when movement changes the current value.
- The `LFO` block also keeps bank `0` selected on boot, lights only the active selector button LED plus the currently selected radio-button LED for the active bank, recalls one stored knob value whenever buttons `0..2` change the active bank, updates its knob, compares the current and previous knob-button state to log `LFO knob 0 button pressed` / `released` transitions, logs radio-button selection as `LFO bank N radio M selected`, and logs knob movement as `LFO bank N knob 0 position=V` when movement changes the current value in the active LFO bank.
- The `MOD` block also keeps bank `0` selected on boot, lights only the active selector button LED, recalls one stored knob value whenever buttons `0..5` change the active bank, updates its knob, compares the current and previous knob-button state to log `MOD knob 0 button pressed` / `released` transitions, and logs knob movement as `MOD bank N knob 0 position=V` when movement changes the current value in the active MOD bank.
- The `OSC` block also keeps bank `0` selected on boot, lights only the active selector button LED, recalls five stored knob values whenever buttons `0..2` change the active bank, updates each knob, compares the current and previous knob-button state to log `OSC knob N button pressed` / `released` transitions, and logs knob movement as `OSC bank N knob M position=V` when movement changes the current value in the active bank.
- Status and error messages are emitted over the ST-LINK virtual serial port.

## Developer Notes

- ADSR block composition is implemented in `src/blocks/ADSR.cpp` and declared in `src/blocks/ADSR.h`.
- FLT block composition is implemented in `src/blocks/FLT.cpp` and declared in `src/blocks/FLT.h`.
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
- CD4067 aggregation is implemented in `src/MUX.cpp` and declared in `src/MUX.h`.
- Shared binary-mask formatting helpers are implemented in `src/utils.cpp` and declared in `src/utils.h`.
- The out-of-tree CD4067 module is discovered through `../cd4067` from the app `CMakeLists.txt`.
- The app `CMakeLists.txt` adds `src` to the target include path so nested sources can include project headers without relative `../` prefixes.
- Hardware description for the PCA9685 devices, CD4067 instances, and discrete GPIO inputs lives in `app.overlay`.

For build, flash, documentation generation, and current hardware wiring, see
the repository `README.md`.

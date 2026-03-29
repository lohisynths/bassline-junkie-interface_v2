# Bassline Junkie Interface

Zephyr firmware for the ST Nucleo-F411RE board.

The application blinks the onboard LD2 LED through the standard Zephyr `led0`
alias, logs status over the ST-LINK virtual serial port at `1000000` baud, and drives multiple
PCA9685 PWM controllers through the `LEDSController` class. It also samples multiple
CD4067 GPIO multiplexers through the `MUX` class, which is backed by an
out-of-tree Zephyr driver located in `cd4067/`. Discrete GPIO inputs are
sampled through the `GPIO` class. The `InputController` class combines the mux
and GPIO sources into one cached state table. The `Button` class decodes one
button from a selected cached input bit, exposes per-update change flags
through `button_msg`, uses a `Config` binding for its input bit and LED
channel, and exposes explicit LED control through `set_led_val()`, the
`Encoder` class decodes a
quadrature encoder from two selected CD4067 channels inside that cached input
state, and the `Knob` class owns one internal `Encoder`, samples one raw
active-low button bit directly from the cached input state, and drives one LED
segment as a reusable knob UI. The `ADSR` block class groups four standalone
buttons and four knobs into one reusable control-surface unit, owns their
configuration tables, drives their LEDs, and emits transition logs. The `OSC`
block class groups five knobs and three selector buttons into a second
banked control-surface unit. The `MOD` block class groups one knob and six
selector buttons into a third banked control-surface unit. The `LFO` block
groups one knob, three bank-selector buttons, and five radio buttons into a
fourth control-surface unit. The `FLT` block groups three radio buttons and
three standalone knobs into a fifth control-surface unit. The `LED_DISP`
block class groups one standalone knob with one three-digit seven-segment LED
display into a preset selector and preset save/load UI. The new `PresetStore`
class keeps 128 preset snapshots in a dedicated internal-flash partition, and
the `PresetSnapshot` model captures the durable state of the `ADSR`, `FLT`,
`LFO`, `MOD`, and `OSC` blocks while leaving the bank-selector buttons live.
The `UART` class owns one app-facing polling transport on `USART1` and keeps
the existing Zephyr console on the ST-LINK virtual serial port.
The `MIDI` class builds on top of that `UART` transport and emits basic MIDI
channel messages such as Note On, Note Off, and Control Change.
The current runtime pattern maps one encoder
onto each LED-backed segment, exposes latched knob-value banks where required,
maintains one clamped knob value in the range `0..127` per knob, lights one
LED in each segment according to the active value when LEDs are assigned,
drives one active-low three-digit display from the dedicated preset knob,
auto-loads preset `0` on boot, uses a reduced-resolution preset encoder to
avoid accidental slot changes while pressing, and uses the preset knob push
button for short-press load plus timeout-triggered long-press save. The input
thread constructs its
`InputController`, `LEDSController`, `ADSR`, `FLT`, `LED_DISP`, `LFO`, `MOD`,
`OSC`, and `PresetStore` objects as plain local objects, and each `Knob` owns
its internal `Encoder` helper while reading its configured button bit
directly. The button input is treated as active-low, so a raw mux bit value of
`0` means pressed and `1` means released.

The current CD4067 wiring described in `app/app.overlay` is:

- `S0` -> `PB5`
- `S1` -> `PB4`
- `S2` -> `PB10`
- `S3` -> `PA8`

The four configured mux `SIG` inputs are:

- `MUX0` -> `PA0`
- `MUX1` -> `PA1`
- `MUX2` -> `PA4`
- `MUX3` -> `PB0`

The configured discrete GPIO inputs are:

- `ENC10SW` -> `PB15`
- `ENC10A` -> `PB1`
- `ENC10B` -> `PB2`
- `ENC13SW` -> `PH1`
- `ENC13B` -> `PC2`
- `ENC13A` -> `PC3`
- `ENC14SW` -> `PC0`
- `ENC14B` -> `PC1`
- `ENC14A` -> `PH0`

The configured app-owned UART wiring is:

- `UART1_TX` -> `PA9`
- `UART1_RX` -> `PA10`

The serial transports are:

- `USART1` on `PA9`/`PA10`: app-owned external UART transport at `115200` baud
- `USART2` on the ST-LINK virtual COM port: Zephyr console and logs on `ttyACM0` at `1000000` baud

## Requirements

- A working Zephyr installation
- Zephyr Python virtual environment at `~/zephyrproject/.venv`
- Zephyr SDK `0.17.4`
- STM32CubeProgrammer installed
- Board target: `nucleo_f411re`
- Doxygen, if you want to generate API documentation locally

## Shell Setup

Before building or flashing, prepare the shell explicitly:

```bash
source ~/zephyrproject/.venv/bin/activate
export ZEPHYR_TOOLCHAIN_VARIANT=zephyr
export ZEPHYR_BASE=~/zephyrproject/zephyr
export ZEPHYR_SDK_INSTALL_DIR=~/Downloads/zephyr-sdk-0.17.4
export PATH="$HOME/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin:$PATH"
```

These exports are used here because they worked reliably for both `west build`
and `west flash` on this machine.

## Project Layout

```text
.
├── README.md
├── app
│   ├── CMakeLists.txt
│   ├── Doxyfile
│   ├── app.overlay
│   ├── docs
│   ├── prj.conf
│   └── src
└── cd4067
    ├── drivers
    ├── dts
    ├── include
    └── zephyr
```

The main application sources are:

- `app/src/blocks/ADSR.h` and `app/src/blocks/ADSR.cpp`: reusable block that owns the current standalone button set and knob set, including their config tables, three banked knob-value sets, selector LED updates, and transition logging
- `app/src/blocks/FLT.h` and `app/src/blocks/FLT.cpp`: reusable block that owns three radio buttons and three standalone knobs, including their config tables, radio-selection LED updates, and transition logging
- `app/src/blocks/LED_DISP.h` and `app/src/blocks/LED_DISP.cpp`: reusable block that owns the preset-selector knob plus one active-low three-digit seven-segment display, including the knob config table, timeout-triggered preset save/load gesture handling, temporary blink feedback, browse timeout restore, full-surface snapshot capture/application, blank-leading decimal rendering, and transition logging
- `app/src/blocks/LFO.h` and `app/src/blocks/LFO.cpp`: reusable block that owns one knob plus three bank-selector buttons and five radio buttons, including their config tables, three banked knob-value sets, per-bank radio selection, selector LED updates, and transition logging
- `app/src/blocks/MOD.h` and `app/src/blocks/MOD.cpp`: reusable block that owns one knob plus six bank-selector buttons, including their config tables, six banked knob-value sets, selector LED updates, and transition logging
- `app/src/blocks/OSC.h` and `app/src/blocks/OSC.cpp`: reusable block that owns five knobs plus three bank-selector buttons, including their config tables, three banked knob-value sets, selector LED updates, and transition logging
- `app/src/Button.h` and `app/src/Button.cpp`: button decoder with `Config` binding, `button_msg` change reporting, and explicit LED control through `set_led_val()`
- `app/src/Encoder.h` and `app/src/Encoder.cpp`: quadrature decoder bound to one cached mux state and two CD4067 channels
- `app/src/Knob.h` and `app/src/Knob.cpp`: reusable knob UI that owns one encoder, reads one raw active-low button bit, drives one contiguous LED segment, supports explicit value recall through `set_value()`, and can divide raw encoder edges before exposing one visible value step
- `app/src/PresetSnapshot.h`: durable preset schema for the `ADSR`, `FLT`, `LFO`, `MOD`, and `OSC` blocks
- `app/src/PresetStore.h` and `app/src/PresetStore.cpp`: flash-backed preset storage helper that validates one versioned preset log with CRC32, exposes 128 slots, returns default state for unsaved slots, and appends one flash record on each save
- `app/src/UART.h` and `app/src/UART.cpp`: polling-based wrapper around the app-owned `USART1` transport on `PA9`/`PA10`, including buffer writes plus non-blocking reads
- `app/src/MIDI.h` and `app/src/MIDI.cpp`: MIDI channel-message helper layered on top of `UART`, including Note On, Note Off, and Control Change message encoding
- `app/src/main.cpp`: entrypoint, input-thread setup, `ADSR`, `FLT`, `LED_DISP`, `LFO`, `MOD`, `OSC`, and `PresetStore` wiring, and top-level runtime loop
- `app/src/GPIO.h` and `app/src/GPIO.cpp`: discrete GPIO input initialization and bitmask reads
- `app/src/InputController.h` and `app/src/InputController.cpp`: aggregate input reads across all mux and GPIO sources, expose `input_count`, and provide optional debug logging helpers for input transitions and state dumps
- `app/src/LEDS.h` and `app/src/LEDS.cpp`: PCA9685 LED control through `LEDSController`
- `app/src/MUX.h` and `app/src/MUX.cpp`: CD4067 mux aggregation and scanning
- `app/src/utils.h` and `app/src/utils.cpp`: shared utility helpers including binary mask formatting for debug logs

## Build

From the repository root, after the shell setup above:

```bash
west build -p always -b nucleo_f411re -d build/app app -- -G"Unix Makefiles"
```

`-p always` forces a pristine rebuild so stale CMake or board configuration does
not leak into the next build.
The `-d build/app` option keeps the generated files under `build/app/`.
The final `app` argument tells `west` to use the application located in the
`app/` directory. The app `CMakeLists.txt` registers `../cd4067` as an
out-of-tree Zephyr module automatically and adds `app/src` to the target
include path so nested sources such as `app/src/blocks/ADSR.h` can include
project headers without `../` prefixes. The app config now enables
`CONFIG_USE_DT_CODE_PARTITION`, so the linker is constrained to the `384 KB`
`code_partition` declared in `app/app.overlay`, while the final `128 KB`
sector is reserved for flash-backed presets in `preset_partition`.

Successful builds produce artifacts under `build/app/zephyr/`, including
`zephyr.elf`, `zephyr.hex`, and `zephyr.bin`.

## API Documentation

This repository includes a Doxygen configuration and module documentation for
the application code and the CD4067 integration.

Generate the docs from the repository root with:

```bash
doxygen app/Doxyfile
```

The generated HTML entry point is:

```text
app/docs/doxygen/html/index.html
```

The Doxygen landing page focuses on code structure and module responsibilities.
Use this README as the canonical source for environment setup, build, flash,
and hardware wiring information. The generated API docs include the `Button`,
`Encoder`, `GPIO`, `InputController`, `Knob`, `LEDSController`, `MUX`,
`PresetStore`, `UART`, `MIDI`, `ADSR`, `FLT`, `LED_DISP`, `LFO`, `MOD`, and `OSC` classes,
the shared `PresetSnapshot` schema, the shared `utils` helpers, plus the
CD4067 driver interface.

## Flash

The generated runner configuration uses `stm32cubeprogrammer` by default.
After the shell setup above, flash with:

```bash
west flash -d build/app
```

This uses the artifacts already generated under `build/app/`.

Verified on this machine with:

- Zephyr `4.3.0`
- `west` `v1.5.0`
- STM32CubeProgrammer `v2.22.0`
- Board: `NUCLEO-F411RE`
- Flash command: `west flash -d build/app`

## Expected Behavior

When the application is flashed and running on the board:

- the onboard LD2 LED toggles every 1 s
- the app-owned `USART1` transport on `PA9`/`PA10` emits `Bassline Junkie Interface UART1 ready` at `115200` baud during boot
- the firmware continuously scans the configured CD4067 muxes and discrete GPIO inputs into one cached input-state table
- the input thread constructs the `InputController`, `LEDSController`, and the `ADSR`, `FLT`, `LFO`, `MOD`, `OSC`, `PresetStore`, and `LED_DISP` control blocks before entering its polling loop
- the control surface exposes:
    - banked knob/button state in `ADSR`, `LFO`, `MOD`, and `OSC`
    - radio-button selection in `FLT` and in the per-bank `LFO` radio group
    - one active-low three-digit seven-segment display driven by `LED_DISP`
    - 128 flash-backed presets selected on the `LED_DISP` encoder
- OSC knob changes emit MIDI Control Change messages on channel `0`, with CC numbers `0..4` for bank `0`, `5..9` for bank `1`, and `10..14` for bank `2`
- loading a preset also emits the full OSC MIDI snapshot on channel `0`, sending all CC numbers `0..14` from the loaded banked values
- ADSR knob changes emit MIDI Control Change messages on channel `0`, with knob CCs `14..17`, `19..22`, and `24..27` across banks `0..2`, while the latched button uses CC `18`, `23`, and `28` with values `0` or `127`
- loading a preset also emits the full ADSR MIDI snapshot on channel `0`, sending all ADSR CC numbers `14..28` from the loaded banked values
- FLT knob `0` and knob `1` emit MIDI Control Change messages on channel `0` using CC `29` and `30`, while the FLT radio selection emits CC `31` with values `0`, `63`, or `127`; FLT knob `2` is intentionally ignored by MIDI
- loading a preset also emits the FLT MIDI snapshot on channel `0`, resending CC `29`, `30`, and `31`
- LFO banked radio selection emits MIDI Control Change messages on channel `0` using CC `32`, `34`, and `36` with values `0..3` for the first four LFO radio buttons, while the banked LFO knob emits CC `33`, `35`, and `37`; the fifth LFO radio button is intentionally ignored by MIDI
- loading a preset also emits the LFO MIDI snapshot on channel `0`, resending CC `32..37`
- knob values are clamped to `0..127`, recalled when a bank changes, and shown on their assigned LED segments when present
- selector and radio-button LEDs reflect the currently active state, and bank `0` is selected on boot
- the `LED_DISP` block shows the currently selected preset number in the range `0..127` with blank leading digits
- the preset encoder uses a coarser step size than the other knobs so pressing it is less likely to move the displayed preset number
- preset `0` is auto-loaded on boot, short display-knob presses load the selected preset on release, and holding the display knob for `1000 ms` saves immediately on timeout
- after a hold-triggered save, the display blinks briefly as confirmation
- browsing to another preset without loading or saving for `5000 ms` causes the display to blink, then snap back to the active preset number
- presets restore bank contents but keep the currently selected bank in `ADSR`, `LFO`, `MOD`, and `OSC`
- loading an unsaved preset slot applies the default all-zero surface state until the slot is explicitly saved
- button inputs are active-low, and each knob exposes its encoder push-button state for higher-level logic
- serial logs report bank changes, button transitions, radio selections, knob movements, and preset save/load actions while the input thread runs
- the main thread logs `Heartbeat: LED blink running` every 10 s
- the firmware emits serial log messages on `ttyACM0` at `1000000` baud

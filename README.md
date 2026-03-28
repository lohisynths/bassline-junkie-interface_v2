# Bassline Junkie Interface

Zephyr firmware for the ST Nucleo-F411RE board.

The application blinks the onboard LD2 LED through the standard Zephyr `led0`
alias, logs status over the ST-LINK virtual serial port, and drives multiple
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
fourth control-surface unit. The current runtime pattern maps one encoder onto
each 10-channel PCA9685 segment, exposes latched knob-value banks selected by
buttons, maintains one clamped knob value in the range `0..127` per knob
inside each bank, lights one LED in each segment according to the active bank
value, and logs bank selection plus encoder movement as the input thread
refreshes the cached state. The input thread constructs its `InputController`,
`LEDSController`, `ADSR`, `LFO`, `MOD`, and `OSC` objects as plain local
objects, and each `Knob` owns its internal `Encoder` helper while reading its
configured button bit directly. The button input is treated as active-low, so
a raw mux bit value of `0` means pressed and `1` means released.

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
- `app/src/blocks/LFO.h` and `app/src/blocks/LFO.cpp`: reusable block that owns one knob plus three bank-selector buttons and five radio buttons, including their config tables, three banked knob-value sets, per-bank radio selection, selector LED updates, and transition logging
- `app/src/blocks/MOD.h` and `app/src/blocks/MOD.cpp`: reusable block that owns one knob plus six bank-selector buttons, including their config tables, six banked knob-value sets, selector LED updates, and transition logging
- `app/src/blocks/OSC.h` and `app/src/blocks/OSC.cpp`: reusable block that owns five knobs plus three bank-selector buttons, including their config tables, three banked knob-value sets, selector LED updates, and transition logging
- `app/src/Button.h` and `app/src/Button.cpp`: button decoder with `Config` binding, `button_msg` change reporting, and explicit LED control through `set_led_val()`
- `app/src/Encoder.h` and `app/src/Encoder.cpp`: quadrature decoder bound to one cached mux state and two CD4067 channels
- `app/src/Knob.h` and `app/src/Knob.cpp`: reusable knob UI that owns one encoder, reads one raw active-low button bit, drives one contiguous LED segment, and supports explicit value recall through `set_value()`
- `app/src/main.cpp`: entrypoint, input-thread setup, `ADSR`, `LFO`, `MOD`, and `OSC` block wiring, and top-level runtime loop
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
project headers without `../` prefixes.

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
`Encoder`, `GPIO`, `InputController`, `Knob`, `LEDSController`, `MUX`, and
`ADSR`, `LFO`, `MOD`, and `OSC` classes, the shared `utils` helpers, plus the
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
- the firmware scans all 16 channels on each configured CD4067 instance
- the firmware updates one cached input-state table containing all mux masks plus the GPIO mask
- the firmware constructs one `ADSR` block that owns four standalone buttons plus four knobs
- the firmware also constructs one `LFO` block that owns three selector buttons, five radio buttons, and one knob
- the firmware also constructs one `MOD` block that owns six standalone selector buttons plus one knob
- the firmware also constructs one `OSC` block that owns three standalone selector buttons plus five knobs
- the current `ADSR` button configurations use mux `0`, channels `15`, `14`, `13`, and `12`, with LED channels `43`, `42`, `41`, and `40`
- ADSR buttons `0`, `1`, and `2` select knob banks `0`, `1`, and `2`
- only the currently selected bank button LED is lit at `100%`; the other selector LEDs are off
- ADSR button `3` is a latched per-bank toggle, and its LED reflects the stored state for the active bank
- the firmware decodes four quadrature encoders from mux `0`: channels `1/2`, `4/5`, `7/8`, and `10/11`
- the current `ADSR` block owns four `Knob` objects that each own an internal encoder helper, read one configured active-low button bit, and bind the knob indicators to LED channels `0..9`, `10..19`, `20..29`, and `30..39`
- the firmware maintains three independent `0..127` value sets for the four knobs, one set per selector bank
- the firmware also stores one independent latched state for ADSR button `3` in each bank
- the current `LFO` button configurations use mux `3`, channel `0`, and mux `2`, channels `15`, `14`, `13`, `12`, `11`, `10`, and `9`, with LED channels `158`, `157`, `156`, `142`, `141`, `140`, `139`, and `138`
- LFO buttons `0`, `1`, and `2` select knob banks `0`, `1`, and `2`
- LFO buttons `3..7` act as radio buttons, with one latched selection stored independently in each bank
- only the currently selected LFO bank button LED is lit at `100%`, and only the currently selected radio button LED is lit for the active bank
- the firmware decodes one LFO quadrature encoder from cached input state `4`: bits `1/2`
- the current `LFO` block owns one `Knob` object that owns an internal encoder helper, reads configured active-low button bit `0` from cached input state `4`, and binds the knob indicator to LED channels `128..137`
- the firmware maintains three independent `0..127` values for the LFO knob, one per selector bank
- the firmware also stores one independent radio-button selection per LFO bank
- the current `MOD` button configurations use mux `2`, channels `3`, `4`, `5`, `6`, `7`, and `8`, with LED channels `122`, `123`, `124`, `125`, `126`, and `127`
- MOD buttons `0..5` select knob banks `0..5`
- only the currently selected MOD bank button LED is lit at `100%`; the other MOD selector LEDs are off
- the firmware decodes one MOD quadrature encoder from mux `2`: channels `1/2`
- the current `MOD` block owns one `Knob` object that owns an internal encoder helper, reads configured active-low button bit `0`, and binds the knob indicator to LED channels `112..121`
- the firmware maintains six independent `0..127` values for the MOD knob, one per selector bank
- the current `OSC` button configurations use mux `3`, channels `3`, `2`, and `1`, with LED channels `110`, `109`, and `108`
- OSC buttons `0`, `1`, and `2` select knob banks `0`, `1`, and `2`
- only the currently selected OSC bank button LED is lit at `100%`; the other OSC selector LEDs are off
- the firmware decodes five OSC quadrature encoders from mux `1`: channels `13/14`, `10/11`, `7/8`, `4/5`, and `1/2`
- the current `OSC` block owns five `Knob` objects that each own an internal encoder helper, read one configured active-low button bit, and bind the knob indicators to LED channels `96..105`, `78..87`, `68..77`, `58..67`, and `48..57`
- the firmware maintains three independent `0..127` value sets for the five OSC knobs, one set per selector bank
- bank `0` is selected on boot
- selecting a different bank immediately recalls that bank's four knob values and updates the knob LEDs
- selecting a different LFO bank immediately recalls that bank's one knob value and updates the LFO knob LEDs
- selecting a different MOD bank immediately recalls that bank's one knob value and updates the MOD knob LEDs
- selecting a different OSC bank immediately recalls that bank's five knob values and updates the OSC knob LEDs
- selecting a different bank also recalls that bank's latched ADSR button `3` state and updates its LED
- one LED per knob segment is lit at a time according to that clamped value
- the LED indication does not wrap when the knob reaches the minimum or maximum value
- each knob exposes the encoder push-button state for use elsewhere in the application
- the input thread constructs `InputController`, `LEDSController`, one `ADSR`, one `LFO`, one `MOD`, and one `OSC` object as plain local objects on its own stack before entering the polling loop
- the firmware logs `Selected knob bank N` whenever one of the selector buttons changes the active bank
- the firmware logs `Bank N button 3 latched on` / `off` whenever ADSR button `3` toggles the stored state in the active bank
- the firmware logs each current knob value as `Bank N knob M position=V` whenever a valid quadrature edge changes that value in the active bank
- the firmware logs `Selected LFO bank N` whenever one of the LFO selector buttons changes the active bank
- the firmware logs `LFO bank N radio M selected` whenever one of the LFO radio buttons changes the stored selection in the active bank
- the firmware logs the LFO knob button transitions as `LFO knob 0 button pressed` / `released`
- the firmware logs the LFO knob value as `LFO bank N knob 0 position=V` whenever a valid quadrature edge changes that value in the active LFO bank
- the firmware logs `Selected MOD bank N` whenever one of the MOD selector buttons changes the active bank
- the firmware logs the MOD knob button transitions as `MOD knob 0 button pressed` / `released`
- the firmware logs the MOD knob value as `MOD bank N knob 0 position=V` whenever a valid quadrature edge changes that value in the active MOD bank
- the firmware logs `Selected OSC bank N` whenever one of the OSC selector buttons changes the active bank
- the firmware logs each OSC knob value as `OSC bank N knob M position=V` whenever a valid quadrature edge changes that value in the active OSC bank
- the main thread logs `Heartbeat: LED blink running` every 10 s
- the firmware emits serial log messages on `ttyACM0`

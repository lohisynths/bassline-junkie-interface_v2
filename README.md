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
segment as a reusable knob UI. The current runtime pattern maps one encoder
onto each 10-channel PCA9685 segment, maintains one clamped knob value in the
range `0..127` per knob, lights one LED in each segment according to that
value, and logs button transitions plus encoder movement as the input thread
refreshes the cached state. The input thread constructs its `InputController`,
`LEDSController`, `Button` array, and `Knob` array as plain local objects, and
the `Knob` owns its internal `Encoder` helper while reading its configured
button bit directly. The button input is treated as active-low, so a
raw mux bit value of `0` means pressed and `1` means released.

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

- `app/src/Button.h` and `app/src/Button.cpp`: button decoder with `Config` binding, `button_msg` change reporting, and explicit LED control through `set_led_val()`
- `app/src/Encoder.h` and `app/src/Encoder.cpp`: quadrature decoder bound to one cached mux state and two CD4067 channels
- `app/src/Knob.h` and `app/src/Knob.cpp`: reusable knob UI that owns one encoder, reads one raw active-low button bit, and drives one contiguous LED segment
- `app/src/main.cpp`: entrypoint, input-thread setup, single-knob wiring, and top-level runtime loop
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
out-of-tree Zephyr module automatically.

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
`Encoder`, `GPIO`, `InputController`, `Knob`, `LEDSController`, and `MUX`
classes, the shared `utils` helpers, plus the CD4067 driver interface.

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
- the firmware constructs four standalone buttons from `button_configs[]`
- the current standalone button configurations use mux `0`, channels `12`, `13`, `14`, and `15`, with LED channels `40`, `41`, `42`, and `43`
- the input thread updates each standalone button, sets its LED to `100%` when pressed and `0%` when released, and logs `Button N mux=0 bit=X pressed` / `released` on transitions
- the firmware decodes four quadrature encoders from mux `0`: channels `1/2`, `4/5`, `7/8`, and `10/11`
- the firmware constructs four `Knob` objects that each own an internal encoder helper, read one configured active-low button bit, and bind the knob indicators to LED channels `0..9`, `10..19`, `20..29`, and `30..39`
- each knob maintains one internal value in the range `0..127`
- one LED per knob segment is lit at a time according to that clamped value
- the LED indication does not wrap when the knob reaches the minimum or maximum value
- each knob exposes the encoder push-button state for use elsewhere in the application
- the input thread constructs `InputController`, `LEDSController`, one `Button` array, and one `Knob` array as plain local objects on its own stack before entering the polling loop
- the firmware logs each current knob value whenever a valid quadrature edge changes that value
- the main thread logs `Heartbeat: LED blink running` every 10 s
- the firmware emits serial log messages on `ttyACM0`

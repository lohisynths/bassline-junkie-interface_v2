# Bassline Junkie Interface

Zephyr firmware for the ST Nucleo-F411RE board.

The application blinks the onboard LD2 LED through the standard Zephyr `led0`
alias, logs status over the ST-LINK virtual serial port, and drives multiple
PCA9685 PWM controllers through the `LEDS` class. It also samples multiple
CD4067 GPIO multiplexers through the `MUX` class, which is backed by an
out-of-tree Zephyr driver located in `cd4067/`. Discrete encoder GPIO inputs
are sampled through the `GPIO` class. The current runtime pattern clears all
external LED channels, lights one channel at 50% brightness, advances that
channel in a chase loop, and periodically logs both CD4067 and GPIO input
masks.

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

- `app/src/main.cpp`: entrypoint and top-level runtime loop
- `app/src/GPIO.h` and `app/src/GPIO.cpp`: discrete GPIO input initialization and bitmask reads
- `app/src/LEDS.h` and `app/src/LEDS.cpp`: PCA9685 LED control
- `app/src/MUX.h` and `app/src/MUX.cpp`: CD4067 mux aggregation and scanning

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
and hardware wiring information. The generated API docs include the `GPIO`,
`LEDS`, and `MUX` classes plus the CD4067 driver interface.

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

- the onboard LD2 LED toggles every 100 ms
- one PCA9685 channel at a time is driven at 50% brightness
- the active PCA9685 channel advances in a chase loop across all configured channels
- the firmware scans all 16 channels on each configured CD4067 instance
- the firmware logs one 16-bit active-input bitmask per mux periodically
- the firmware logs one active-input GPIO bitmask for the configured encoder inputs periodically
- the firmware emits serial log messages on `ttyACM0`

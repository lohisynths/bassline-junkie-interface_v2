# Bassline Junkie Interface

Zephyr firmware for the ST Nucleo-F411RE board.

The application blinks the onboard LD2 LED through the standard Zephyr `led0`
alias, logs status over the ST-LINK virtual serial port, and drives multiple
PCA9685 PWM controllers through the `LEDS` class. The current runtime pattern
clears all external LED channels, lights one channel at 50% brightness, and
advances that channel in a chase loop.

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
export PATH="~/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin:$PATH"
```

These exports are used here because they worked reliably for both `west build`
and `west flash` on this machine.

## Project Layout

```text
.
├── CMakeLists.txt
├── Doxyfile
├── prj.conf
├── README.md
├── docs
│   └── mainpage.md
└── src
    ├── LEDS.cpp
    ├── LEDS.h
    └── main.cpp
```

## Build

From the repository root, after the shell setup above:

```bash
west build -p always -b nucleo_f411re -d build . -- -G"Unix Makefiles"
```

`-p always` forces a pristine rebuild so stale CMake or board configuration does
not leak into the next build.
The `-- -G"Unix Makefiles"` tail tells `west` to generate a GNU Make-based build
directory instead of Ninja.

Successful builds produce artifacts under `build/zephyr/`, including
`zephyr.elf`, `zephyr.hex`, and `zephyr.bin`.

## API Documentation

This repository includes a Doxygen configuration and module documentation for
the LED control code.

Generate the docs from the repository root with:

```bash
doxygen Doxyfile
```

The generated HTML entry point is:

```text
docs/doxygen/html/index.html
```
## Flash

The generated runner configuration uses `stm32cubeprogrammer` by default.
After the shell setup above, flash with:

```bash
west flash
```

Verified on this machine with:

- Zephyr `4.3.0`
- `west` `v1.5.0`
- STM32CubeProgrammer `v2.22.0`
- Board: `NUCLEO-F411RE`

## Expected Behavior

When the application is flashed and running on the board:

- the onboard LD2 LED toggles every 100 ms
- one PCA9685 channel at a time is driven at 50% brightness
- the active PCA9685 channel advances in a chase loop across all configured channels
- the firmware emits serial log messages on `ttyACM0`

## Troubleshooting

### Board alias errors

This app expects the board to define the `led0` alias. The Zephyr
`nucleo_f411re` board definition provides it for the onboard LD2 LED.

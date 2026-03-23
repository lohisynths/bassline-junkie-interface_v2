# Nucleo-F411RE Blink + UART Console TX

Minimal standalone Zephyr application for the ST Nucleo-F411RE board.

The app blinks the board's onboard LD2 LED by using the standard Zephyr `led0`
devicetree alias.
On this board, `led0` maps to GPIO `PA5`, but the application intentionally uses
the alias rather than hard-coding the pin.

## Requirements

- A working Zephyr installation
- Zephyr Python virtual environment at `~/zephyrproject/.venv`
- Zephyr SDK `0.17.4`
- STM32CubeProgrammer installed
- Board target: `nucleo_f411re`

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
├── prj.conf
├── README.md
└── src
    └── main.c
```

## Build

From the repository root, after the shell setup above:

```bash
west build -p always -b nucleo_f411re -d build . -- -G"Unix Makefiles"
```

`-p always` forces a pristine rebuild so stale CMake or board configuration does
not leak into the next build.

Successful builds produce artifacts under `build/zephyr/`, including
`zephyr.elf`, `zephyr.hex`, and `zephyr.bin`.

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

When the application is flashed and running on the board, the onboard LD2 LED
blinks continuously with a 500 ms toggle interval.

## Troubleshooting

### Board alias errors

This app expects the board to define the `led0` alias. The Zephyr
`nucleo_f411re` board definition provides it for the onboard LD2 LED.

# Bassline Junkie Interface v2

![Bassline Junkie](app/docs/images/bassline-junkie.jpg)

Control-surface firmware for the [Bassline Junkie](https://github.com/lohisynths/bassline-junkie)
synthesizer. It runs on an ST Nucleo-F411RE using Zephyr RTOS and connects the
physical controls, indicators, preset storage, and MIDI transports to the main
synthesizer system.

## Features

- Scans five CD4067 16-channel analog multiplexers
- Drives LED outputs through PCA9685 PWM controllers
- Implements banked OSC, ADSR, LFO, filter, modulation, and volume controls
- Sends MIDI over a dedicated 1 Mbaud UART
- Exposes a host-facing USB MIDI device
- Stores 128 versioned, CRC-protected presets on a FAT-formatted SD card
- Restores the last active preset at startup
- Uses the Nucleo onboard LED as a one-second heartbeat

## Repository layout

```text
.
├── app/
│   ├── src/          Application and control-surface sources
│   ├── docs/         Doxygen main page and project image
│   ├── app.overlay   Board peripherals and pin assignments
│   └── prj.conf      Zephyr configuration
├── cd4067/           Out-of-tree Zephyr CD4067 GPIO mux driver
└── scripts/midi.py   UART MIDI monitor for development
```

The control surface is organized into `OSC`, `ADSR`, `LFO`, `FLT`, `MOD`,
`VOL`, and preset/display blocks. Shared `UI_BLOCK` infrastructure handles
banked controls and MIDI state. The main input thread refreshes the muxes every
5 ms, updates each block, and forwards supported USB MIDI messages to the UART
MIDI transport.

## Requirements

- ST Nucleo-F411RE
- Writable FAT-formatted SD card
- Zephyr `v4.3.0`
- Zephyr SDK `0.17.4`
- `west` and the Zephyr Python environment
- STM32CubeProgrammer for flashing
- Doxygen (optional, for API documentation)

The versions above are the versions known to work with this repository.

## Build

Set up your Zephyr environment first. The following example assumes the
standard `~/zephyrproject` layout; adjust the paths for your installation:

```bash
source ~/zephyrproject/.venv/bin/activate
export ZEPHYR_TOOLCHAIN_VARIANT=zephyr
export ZEPHYR_BASE=~/zephyrproject/zephyr
export ZEPHYR_SDK_INSTALL_DIR=~/Downloads/zephyr-sdk-0.17.4
export PATH="$HOME/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin:$PATH"
```

From the repository root:

```bash
west build -p always -b nucleo_f411re -d build/app app -- -G"Unix Makefiles"
```

Build artifacts are written to `build/app/zephyr/`, including `zephyr.elf`,
`zephyr.hex`, and `zephyr.bin`.

## Flash

Connect the Nucleo board and flash the existing build:

```bash
west flash -d build/app
```

## Hardware configuration

The authoritative hardware description is [`app/app.overlay`](app/app.overlay).
Its principal assignments are:

| Function | Configuration |
| --- | --- |
| CD4067 select S0–S3 | PB5, PB4, PB10, PA8 |
| CD4067 signal inputs 0–4 | PA1, PA0, PA4, PB0, PC1 |
| Application UART | USART1 on PA9/PA10 at 1,000,000 baud |
| USB MIDI | USB OTG FS on PA11/PA12 |
| SD card | SDMMC1, 4-bit bus |
| LED controllers | PCA9685 devices on I2C1, addresses `0x40`–`0x4c` |

USART2 is configured at 1 Mbaud for the Zephyr console through the Nucleo
ST-LINK virtual COM port.

## Presets and startup behavior

An SD card is required. On startup, the firmware initializes and verifies the
card before enabling the controls. If the card cannot be mounted and written,
the display latches `Err` and startup stops.

Presets are stored under `/SD:/presets` as `slot-000.bin` through
`slot-127.bin`. Records are versioned, protected by a CRC, and replaced through
a temporary file to reduce the risk of a partial write. Separate metadata
records the preset to restore at boot. Because the slot number comes from the
filename rather than the record, a preset can be moved by renaming its file.

Missing or incompatible presets briefly display `Err` and load defaults.
Storage I/O failures leave the active preset unchanged.

## MIDI

- OSC, ADSR, LFO, FLT, and VOL controls send Control Change messages on MIDI
  channel 1.
- MOD routing sends Control Change messages on MIDI channel 2.
- Volume uses CC 95.
- USB MIDI Note On, Note Off, and Control Change messages are forwarded to the
  application UART MIDI path.

For development, `scripts/midi.py` prints messages received from
`/dev/ttyUSB0` at 1 Mbaud:

```bash
python3 scripts/midi.py
```

Change the `PORT` constant in the script if the adapter uses another device.

## Documentation

Generate the Doxygen API documentation from the repository root:

```bash
doxygen app/Doxyfile
```

Open `app/docs/doxygen/html/index.html` after generation. Additional runtime and
module details are maintained in
[`app/docs/mainpage.md`](app/docs/mainpage.md).

## Related projects

- [Bassline Junkie](https://github.com/lohisynths/bassline-junkie) - main
  Bassline Junkie project repository
- [Bassline Junkie Interface v2](https://github.com/lohisynths/bassline-junkie-interface_v2) - this control-surface firmware repository
- [Lohi Buildroot](https://github.com/lohisynths/lohi-buildroot) - Buildroot
  support for Lohi systems

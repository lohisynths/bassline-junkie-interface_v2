# Bassline Junkie Interface

![Bassline Junkie](app/docs/images/bassline-junkie.jpg)

Related repositories:

- [Bassline Junkie](https://github.com/lohisynths/bassline-junkie): main
  Bassline Junkie project repository.
- [Bassline Junkie Interface v2](https://github.com/lohisynths/bassline-junkie-interface_v2):
  control-surface interface firmware repository.
- [Lohi Buildroot](https://github.com/lohisynths/lohi-buildroot): Buildroot
  support repository for Lohi systems.

# Overview

Zephyr firmware for the ST Nucleo-F411RE. The application scans CD4067
multiplexers, drives PCA9685 LED outputs, exposes a
banked control surface built on the `UI_BLOCK` CRTP template, stores presets on
a required SD card, and handles MIDI over an app-owned UART transport plus a USB MIDI
device.

The repository is split into three main pieces:

- `app/`: the application, control-surface blocks, preset storage, and board
  configuration
- `cd4067/`: the out-of-tree Zephyr module that provides the custom CD4067 GPIO
  mux driver
- `app/docs/`: the Doxygen landing page and generated API documentation

## Hardware

The board overlay in `app/app.overlay` defines the runtime wiring:

- `led0` maps to the onboard LD2 heartbeat LED
- CD4067 select lines:
  - `S0` -> `PB5`
  - `S1` -> `PB4`
  - `S2` -> `PB10`
  - `S3` -> `PA8`
- CD4067 signal inputs:
  - `MUX0` -> `PA0`
  - `MUX1` -> `PA1`
  - `MUX2` -> `PA4`
  - `MUX3` -> `PB0`
  - `MUX4` -> `PC1`
- App-owned UART pins:
  - `USART1_TX` -> `PA9`
  - `USART1_RX` -> `PA10`
- Serial transports:
  - `USART1` on `PA9`/`PA10`: app-owned UART MIDI transport at `1000000` baud
  - USB MIDI on the `usbotg_fs` peripheral: host-facing MIDI device
  - `USART2` on the ST-LINK virtual COM port: Zephyr console and logs at
    `1000000` baud

## Core Modules

The main application sources are:

- `app/src/blocks/UI_BLOCK.h` and `app/src/blocks/UI_BLOCK.cpp`: shared CRTP
  block infrastructure with metadata-driven name/MIDI defaults; the `.cpp`
  file registers the `UI_BLOCK` log module used by the base template
- `app/src/blocks/ADSR.h`: envelope block with four knobs, three bank selectors,
  and a LOOP toggle
- `app/src/blocks/FLT.h`: filter block with frequency, resonance, and keyboard
  tracking knobs plus three filter-type buttons
- `app/src/blocks/LED_DISP.h`: preset browse/save block with one encoder and an
  up-to-three-digit seven-segment display with leading zeros suppressed
- `app/src/blocks/LFO.h`: LFO block with one knob, three bank selectors, four
  waveform buttons, and a SYNC toggle
- `app/src/blocks/MOD.h` and `app/src/blocks/MOD.cpp`: modulation-routing block
  with one knob, six selector buttons, and a temporary viewer/edit mode that
  borrows OSC and FLT knobs
- `app/src/blocks/OSC.h` and `app/src/blocks/OSC.cpp`: oscillator block with
  five knobs and three bank selectors
- `app/src/blocks/VOL.h`: single volume knob that sends MIDI CC `95` and
  previews on the LED display, with its value stored in each preset
- `app/src/Button.h` and `app/src/Button.cpp`: active-low button decoder with an
  optional LED channel
- `app/src/Encoder.h` and `app/src/Encoder.cpp`: quadrature decoder for the
  muxed encoder channels
- `app/src/Knob.h` and `app/src/Knob.cpp`: reusable knob UI that owns one
  encoder, one push switch, and one LED segment
- `app/src/LEDS.h` and `app/src/LEDS.cpp`: PCA9685 LED controller wrapper
- `app/src/MUX.h` and `app/src/MUX.cpp`: CD4067 multiplexer wrapper with cached
  input snapshots for the control blocks
- `app/src/UART.h` and `app/src/UART.cpp`: polling UART wrapper for `USART1`
- `app/src/MIDI.h` and `app/src/MIDI.cpp`: MIDI channel-message helper on top of
  `UART`
- `app/src/USB_MIDI.h` and `app/src/USB_MIDI.cpp`: USB MIDI facade that
  receives host messages and forwards them into the firmware MIDI path
- `app/src/usb_midi_init.h` and `app/src/usb_midi_init.c`: Zephyr USB device
  setup and MIDI class binding for the USB transport
- `app/src/PresetStorage.h` and `app/src/PresetStorage.cpp`: versioned,
  CRC-protected SD-card preset storage with atomic file replacement
- `app/src/SDCard.h` and `app/src/SDCard.cpp`: required writable SD-card mount
  and startup verification
- `app/src/Preset.h` and `app/src/Preset.cpp`: high-level preset load/save
  controller for `LED_DISP`
- `app/src/PresetSnapshot.h`: durable preset schema for ADSR, FLT, LFO, OSC,
  VOL, and the MOD routing matrices
- `app/src/utils.h` and `app/src/utils.cpp`: shared utility helpers
- `app/src/main.cpp`: entry point, LED heartbeat, and input-thread setup

## Requirements

- A working Zephyr installation
- Zephyr Python virtual environment at `~/zephyrproject/.venv`
- This repository has been verified against Zephyr `v4.3.0`
- Zephyr SDK `0.17.4`
- STM32CubeProgrammer installed
- Board target: `nucleo_f411re`
- USB support enabled on the host if you want to use the USB MIDI device
- Doxygen, if you want to generate the API docs locally
- A writable FAT-formatted SD card; boot halts with `Err` if it cannot be mounted

## Shell Setup

Before building or flashing, prepare the shell explicitly:

```bash
source ~/zephyrproject/.venv/bin/activate
export ZEPHYR_TOOLCHAIN_VARIANT=zephyr
export ZEPHYR_BASE=~/zephyrproject/zephyr
export ZEPHYR_SDK_INSTALL_DIR=~/Downloads/zephyr-sdk-0.17.4
export PATH="$HOME/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin:$PATH"
```

These exports are the ones used successfully for both `west build` and
`west flash` on this machine.

## Build

From the repository root, after the shell setup above:

```bash
west build -p always -b nucleo_f411re -d build/app app -- -G"Unix Makefiles"
```

`-p always` forces a pristine rebuild so stale CMake or board configuration does
not leak into the next build. The `-d build/app` option keeps the generated
files under `build/app/`.

Successful builds produce artifacts under `build/app/zephyr/`, including
`zephyr.elf`, `zephyr.hex`, and `zephyr.bin`.

## API Docs

The repository includes a Doxygen configuration for the application code and
the CD4067 integration.

Generate the docs from the repository root with:

```bash
doxygen app/Doxyfile
```

The generated HTML entry point is:

```text
app/docs/doxygen/html/index.html
```

## Flash

The generated runner configuration uses `stm32cubeprogrammer` by default.
After the shell setup above, flash with:

```bash
west flash -d build/app
```

## Runtime Behavior

- The onboard LD2 LED toggles every second as a heartbeat.
- The app-owned `USART1` transport prints `Bassline Junkie Interface UART1 ready`
  during boot.
- The USB MIDI device initializes during the input-thread startup path and
  accepts Note On, Note Off, and Control Change messages from a host.
- `MUX` caches the mux inputs for the control blocks.
- `UI_BLOCK`-based control surfaces keep banked knob and button state in sync
  with the LED outputs.
- `ADSR`, `FLT`, `LFO`, and `OSC` send MIDI Control Change messages on channel
  `1`. `FLT` uses CC `33..36`, and `LFO` now starts at CC `37`.
- `VOL` sends MIDI Control Change message `95` on channel `1` and previews its
  current value on the LED display. Its value is stored per preset and restored
  when the startup slot or any saved slot is loaded.
- `MOD` sends its routing Control Change messages on channel `2` and can
  temporarily repurpose the visible OSC and FLT knobs to edit the current
  modulation row while a selector button is held long enough.
- `Preset` restores the last active slot on boot, falls back to slot `0` if no
  startup slot has been stored yet, and uses the display encoder for browse/load
  and save gestures.
- `PresetStorage` stores 128 independent snapshot files plus startup-slot
  metadata under `/SD:/presets`. Missing and incompatible slots load defaults
  and display `Err` for one second without affecting other slots. Existing
  flash presets are not migrated. Snapshot filenames are zero-based and use
  three digits (`slot-000.bin` through `slot-127.bin`). The filename alone
  selects the slot, so a snapshot can be moved to another slot by renaming it;
  the slot number is not stored in snapshot files. Startup-slot metadata retains its
  independent version-2 format.
- The preset display uses a reduced encoder step so presses are less likely to
  move the selected slot accidentally.
- USB MIDI input is bridged into the app's internal MIDI transport in the input
  thread, so host-generated Note On, Note Off, and Control Change events share
  the same control path as the hardware-driven messages.

For more detailed implementation notes, see the Doxygen main page in
`app/docs/mainpage.md`.

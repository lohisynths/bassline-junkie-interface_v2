# Bassline Junkie Interface

Zephyr firmware for the STM32 Nucleo-F411RE. The application scans four CD4067
multiplexers and discrete GPIO inputs, drives PCA9685 LED outputs, exposes
banked control surfaces through the `UI_BLOCK` CRTP template, stores 128
presets in flash, talks to a Raspberry Pi DSP over a dedicated `USART1`
transport, and exposes a USB MIDI device for host traffic.

The repository is split into three main pieces:

- `app/`: the application, control-surface blocks, preset storage, USB MIDI
  setup, and board configuration
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
- Discrete GPIO inputs:
  - `ENC10SW` -> `PB2`
  - `ENC10A` -> `PB1`
  - `ENC10B` -> `PB15`
  - `ENC13SW` -> `PB13`
  - `ENC13B` -> `PC2`
  - `ENC13A` -> `PC3`
  - `ENC14SW` -> `PC0`
  - `ENC14B` -> `PC1`
  - `ENC14A` -> `PH0`
- App-owned serial and USB endpoints:
  - `USART1_TX` -> `PA9`
  - `USART1_RX` -> `PA10`
  - `USB OTG FS` -> `PA11`/`PA12`
- Runtime transports:
  - `USART1` on `PA9`/`PA10`: app-owned UART transport for the DSP link
  - `USART2` on the ST-LINK virtual COM port: Zephyr console and logs
  - USB MIDI on `PA11`/`PA12`: host-facing MIDI device

## Core Modules

The main application sources are:

- `app/src/blocks/UI_BLOCK.h` and `app/src/blocks/UI_BLOCK.cpp`: CRTP base
  template that wires banked knobs and buttons, stores preset values, and dumps
  MIDI for the control-surface blocks
- `app/src/blocks/ADSR.h`: envelope block with four knobs, three bank selectors,
  and a LOOP toggle
- `app/src/blocks/FLT.h`: filter block with frequency, resonance, and keyboard
  tracking knobs plus three filter-type buttons
- `app/src/blocks/LED_DISP.h`: preset browse/save block with one encoder and a
  three-digit seven-segment display
- `app/src/blocks/LFO.h`: LFO block with one knob, three bank selectors, four
  waveform buttons, and a SYNC toggle
- `app/src/blocks/MOD.h` and `app/src/blocks/MOD.cpp`: modulation-routing block
  with one knob and six selector buttons plus the temporary viewer overlay
- `app/src/blocks/OSC.h` and `app/src/blocks/OSC.cpp`: oscillator block with
  five knobs and three bank selectors
- `app/src/Button.h` and `app/src/Button.cpp`: active-low button decoder with an
  optional LED channel
- `app/src/Encoder.h` and `app/src/Encoder.cpp`: quadrature decoder for the
  muxed encoder channels
- `app/src/GPIO.h` and `app/src/GPIO.cpp`: discrete GPIO input handling
- `app/src/InputController.h` and `app/src/InputController.cpp`: aggregates the
  mux and GPIO state into one cached input table
- `app/src/Knob.h` and `app/src/Knob.cpp`: reusable knob UI that owns one
  encoder, one push switch, and one LED segment
- `app/src/LEDS.h` and `app/src/LEDS.cpp`: PCA9685 LED controller wrapper
- `app/src/MUX.h` and `app/src/MUX.cpp`: CD4067 multiplexer wrapper
- `app/src/UART.h` and `app/src/UART.cpp`: polling UART wrapper for `USART1`
- `app/src/MIDI.h` and `app/src/MIDI.cpp`: MIDI channel-message helper on top of
  `UART`
- `app/src/USB_MIDI.h` and `app/src/USB_MIDI.cpp`: USB MIDI facade that
  receives host channel-voice messages and sends Note On/Off/CC messages back
  to the host
- `app/src/usb_midi_init.h` and `app/src/usb_midi_init.c`: USB device-stack
  setup and the message queue behind the USB MIDI facade
- `app/src/EEPROM.h` and `app/src/EEPROM.cpp`: flash-backed preset storage
- `app/src/Preset.h` and `app/src/Preset.cpp`: high-level preset load/save
  controller for `LED_DISP`
- `app/src/PresetDumpRequestListener.h` and
  `app/src/PresetDumpRequestListener.cpp`: reserved MIDI CC listener that asks
  `Preset` to re-dump the active state over the shared UART
- `app/src/PresetSnapshot.h`: durable preset schema for ADSR, FLT, LFO, OSC,
  and the MOD routing matrices
- `app/src/utils.h` and `app/src/utils.cpp`: shared utility helpers
- `app/src/main.cpp`: entry point, LED heartbeat, USB MIDI forwarding, and
  input-thread setup

## Requirements

- A working Zephyr installation
- Zephyr Python virtual environment at `~/zephyrproject/.venv`
- Zephyr SDK `0.17.4`
- STM32CubeProgrammer installed
- Board target: `nucleo_f411re`
- Doxygen, if you want to generate the API docs locally

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
- `USART1` carries protocol traffic between the STM32 interface and the
  Raspberry Pi DSP. `USART2` stays on the ST-LINK console, and USB MIDI bridges
  host messages into the same UART-backed MIDI path.
- `InputController` caches the mux and discrete GPIO inputs for the control
  blocks.
- `UI_BLOCK`-based control surfaces keep banked knob and button state in sync
  with the LED outputs.
- `ADSR`, `FLT`, `LFO`, and `OSC` send MIDI Control Change messages on channel
  `1`. `FLT` uses CC `33..36`, and `LFO` starts at CC `37`.
- `MOD` sends its routing Control Change messages on channel `2` and can
  temporarily preview the current modulation row on the OSC and FLT LEDs while a
  selector button is held.
- `Preset` restores the last active slot on boot, falls back to slot `0` if no
  startup slot has been stored yet, and uses the display encoder for browse/load
  and save gestures.
- When the DSP sends reserved MIDI `CC 127 = 127` on channel `16`, the
  interface re-dumps the currently active state over `USART1`. Regular block
  parameters come from `UI_BLOCK::dump_active_state()`, while the modulation
  matrix is emitted separately from the live OSC and FLT mod-routing arrays.
- `EEPROM` stores 128 preset slots plus startup-slot metadata in the dedicated
  flash partition.
- The preset display uses a reduced encoder step so presses are less likely to
  move the selected slot accidentally.

For more detailed implementation notes, see the Doxygen main page in
`app/docs/mainpage.md`.

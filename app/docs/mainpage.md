# Bassline Junkie Interface

Zephyr firmware for the STM32 Nucleo-F411RE. The application scans CD4067
multiplexers and discrete GPIO inputs, drives PCA9685 LED outputs, exposes a
banked control surface through the `UI_BLOCK` CRTP template, stores presets in
flash, and sends MIDI over an app-owned UART transport.

## Modules

- `UI_BLOCK`: CRTP base template that wires banked knobs and buttons, stores
  preset values, and dumps MIDI for the control-surface blocks
- `ADSR`: four-knob envelope block with three bank selectors and a LOOP toggle
- `FLT`: frequency, resonance, and keyboard-tracking filter block with three
  filter-type buttons
- `LED_DISP`: preset browse/save block with one encoder and a three-digit
  seven-segment display
- `LFO`: one-knob LFO block with three bank selectors, four waveform buttons,
  and a SYNC toggle
- `MOD`: one-knob modulation-routing block with six selector buttons and a
  temporary viewer overlay for OSC and FLT routing values
- `OSC`: five-knob oscillator block with three bank selectors
- `VOL`: single volume knob that sends MIDI CC `95`
- `Button`: active-low button decoder with optional LED control
- `Encoder`: quadrature decoder for muxed encoder inputs
- `GPIO`: discrete GPIO input wrapper
- `InputController`: cached aggregator for the mux and GPIO input sources
- `Knob`: reusable knob UI that owns one encoder, one push switch, and one LED
  segment
- `LEDSController`: PCA9685 LED controller wrapper
- `MUX`: CD4067 multiplexer wrapper
- `UART`: polling UART facade for `USART1`
- `MIDI`: channel-message helper layered on top of `UART`
- `EEPROM`: flash-backed preset storage
- `Preset`: preset load/save controller for the display encoder
- `PresetSnapshot`: durable schema for ADSR, FLT, LFO, OSC, and the MOD routing
  matrices
- `utils`: shared helper functions
- `main.cpp`: application entry point and input-thread setup
- `cd4067`: out-of-tree Zephyr module that provides the CD4067 GPIO mux driver

## Runtime Overview

- Boot prints `Bassline Junkie Interface UART1 ready` on `USART1`.
- `InputController` caches the CD4067 scans and discrete GPIO reads so the
  button, encoder, and knob helpers can consume a stable input snapshot.
- `ADSR`, `LFO`, and `OSC` expose banked parameters, with LEDs reflecting the
  currently selected bank.
- `FLT` exposes one set of knobs and three filter-type buttons.
- `MOD` edits OSC and FLT routing matrices, sends its MIDI Control Change data
  on channel `2`, and shows a temporary LED preview when a selector button is
  held long enough.
- `Preset` restores the last active slot on boot, saves and loads through the
  display encoder, and briefly blanks the display as save or timeout feedback.
- `EEPROM` provides 128 preset slots plus startup-slot metadata in the dedicated
  flash partition.
- `ADSR`, `FLT`, `LFO`, and `OSC` emit MIDI Control Change messages on channel
  `1`. `FLT` uses CC `33..36`, and `LFO` starts at CC `37`.
- `VOL` emits MIDI Control Change message `95` on channel `1`.
- The display encoder uses a reduced step size so a button press is less likely
  to move the selected preset slot.

## Notes

- For build, flash, wiring, and environment setup, see the repository
  `README.md`.
- The Doxygen configuration lives in `app/Doxyfile`.

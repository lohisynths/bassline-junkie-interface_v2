# Bassline Junkie Interface

Zephyr firmware for the STM32 Nucleo-F411RE. The application scans CD4067
multiplexers, drives PCA9685 LED outputs, exposes a
banked control surface through the `UI_BLOCK` CRTP template, stores presets on
a required SD card, and moves MIDI through an app-owned UART transport and a USB MIDI
device.

## Modules

- `UI_BLOCK`: CRTP base template that wires banked knobs and buttons, stores
  preset values, and dumps MIDI for the control-surface blocks
- `ADSR`: four-knob envelope block with three bank selectors and a LOOP toggle
- `FLT`: frequency, resonance, and keyboard-tracking filter block with three
  filter-type buttons
- `LED_DISP`: preset browse/save block with one encoder and an up-to-three-digit
  seven-segment display with leading zeros suppressed
- `LFO`: one-knob LFO block with three bank selectors, four waveform buttons,
  and a SYNC toggle
- `MOD`: one-knob modulation-routing block with six selector buttons and a
  temporary viewer/edit mode for OSC and FLT routing values
- `OSC`: five-knob oscillator block with three bank selectors
- `VOL`: single volume knob that sends MIDI CC `95` and previews on the LED
  display, with its value stored in each preset
- `Button`: active-low button decoder with optional LED control
- `Encoder`: quadrature decoder for muxed encoder inputs
- `Knob`: reusable knob UI that owns one encoder, one push switch, and one LED
  segment
- `LEDSController`: PCA9685 LED controller wrapper
- `MUX`: CD4067 multiplexer wrapper with cached input snapshots
- `UART`: polling UART facade for `USART1`
- `MIDI`: channel-message helper layered on top of `UART`
- `USB_MIDI`: USB MIDI facade that receives host Note On, Note Off, and Control
  Change messages and forwards them into the firmware MIDI path
- `usb_midi_init`: Zephyr USB device setup and MIDI class binding for the USB
  transport
- `PresetStorage`: versioned, CRC-protected SD-card preset storage
- `SDCard`: required writable FATFS mount and startup verification
- `Preset`: preset load/save controller for the display encoder
- `PresetSnapshot`: durable schema for ADSR, FLT, LFO, OSC, VOL, and the MOD
  routing matrices
- `utils`: shared helper functions
- `main.cpp`: application entry point and input-thread setup
- `cd4067`: out-of-tree Zephyr module that provides the CD4067 GPIO mux driver

## Runtime Overview

- Boot prints `Bassline Junkie Interface UART1 ready` on `USART1`.
- `MUX` caches the CD4067 scans so the button, encoder, and knob
  helpers can consume a stable input snapshot.
- `ADSR`, `LFO`, and `OSC` expose banked parameters, with LEDs reflecting the
  currently selected bank.
- `FLT` exposes one set of knobs and three filter-type buttons.
- `MOD` edits OSC and FLT routing matrices, sends its MIDI Control Change data
  on channel `2`, and temporarily repurposes the visible OSC and FLT knobs to
  edit the active source row when a selector button is held long enough.
- `Preset` restores the last active slot on boot, saves and loads through the
  display encoder, and briefly blanks the display as save or timeout feedback.
- `PresetStorage` provides 128 independent preset files plus startup-slot
  metadata under `/SD:/presets`. Writes use temporary-file replacement.
- Boot halts with a latched `Err` if the SD card cannot be mounted and verified.
  Missing and incompatible slots show `Err` for one second, load defaults, and
  leave other slots intact. Flash presets are not migrated.
- `ADSR`, `FLT`, `LFO`, and `OSC` emit MIDI Control Change messages on channel
  `1`. `FLT` uses CC `33..36`, and `LFO` starts at CC `37`.
- `VOL` emits MIDI Control Change message `95` on channel `1` and previews its
  current value on the LED display. Its value is stored per preset and restored
  when the startup slot or any saved slot is loaded.
- USB MIDI messages are decoded in the input thread and forwarded into the same
  MIDI transport used by the control-surface blocks.
- The display encoder uses a reduced step size so a button press is less likely
  to move the selected preset slot.

## Notes

- For build, flash, wiring, and environment setup, see the repository
  `README.md`.
- The Doxygen configuration lives in `app/Doxyfile`.

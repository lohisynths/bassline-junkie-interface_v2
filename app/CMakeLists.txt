cmake_minimum_required(VERSION 3.20.0)

list(APPEND ZEPHYR_EXTRA_MODULES ${CMAKE_CURRENT_SOURCE_DIR}/../cd4067)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(bassline_junkie_interface)

target_include_directories(app PRIVATE src)

target_sources(app PRIVATE
    src/EEPROM.cpp
    src/wait_for_dsp.cpp
    src/blocks/Preset.cpp
    src/blocks/UI_BLOCK.cpp
    src/blocks/MOD.cpp
    src/Button.cpp
    src/Encoder.cpp
    src/Knob.cpp
    src/main.cpp
    src/MIDI.cpp
    src/GPIO.cpp
    src/InputController.cpp
    src/LEDS.cpp
    src/MUX.cpp
    src/UART.cpp
    src/USB_MIDI.cpp
    src/usb_midi_init.c
    src/utils.cpp)

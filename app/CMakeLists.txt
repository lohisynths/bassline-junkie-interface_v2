cmake_minimum_required(VERSION 3.20.0)

list(APPEND ZEPHYR_EXTRA_MODULES ${CMAKE_CURRENT_SOURCE_DIR}/../cd4067)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(bassline_junkie_interface)

target_include_directories(app PRIVATE src)

target_sources(app PRIVATE
    src/blocks/ADSR.cpp
    src/blocks/FLT.cpp
    src/blocks/LED_DISP.cpp
    src/blocks/LFO.cpp
    src/blocks/MOD.cpp
    src/blocks/OSC.cpp
    src/Button.cpp
    src/Encoder.cpp
    src/Knob.cpp
    src/main.cpp
    src/MIDI.cpp
    src/PresetStore.cpp
    src/GPIO.cpp
    src/InputController.cpp
    src/LEDS.cpp
    src/MUX.cpp
    src/UART.cpp
    src/utils.cpp)

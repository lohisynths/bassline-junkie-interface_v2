/**
 * @file OSC.h
 * @brief Oscillator control-surface block.
 *
 * Created on: Mar 29, 2026
 *     Author: alax
 */

#ifndef APP_SRC_BLOCKS_OSC_H_
#define APP_SRC_BLOCKS_OSC_H_

#include "UI_BLOCK.h"

/** @brief Number of encoder knobs in the OSC block. */
static constexpr uint8_t OSC_KNOB_COUNT = 5U;

/** @brief Number of bank-selector buttons in the OSC block. */
static constexpr uint8_t OSC_BUTTON_COUNT = 3U;

/** @brief Number of banked instances (selector positions) in the OSC block. */
static constexpr uint8_t OSC_COUNT = 3U;

/**
 * @brief Logical parameter indices for one OSC bank.
 */
enum OSC_PARAMS {
    OSC_PITCH,
    OSC_SIN,
    OSC_SAW,
    OSC_SQR,
    OSC_RND,
    OSC_PARAM_COUNT
};

/**
 * @brief Oscillator block owning five knobs and three bank-selector buttons.
 *
 * Hardware wiring is declared in the two public constexpr config tables and
 * consumed automatically by @ref UI_BLOCK::init through CRTP.
 *
 * All three buttons are bank selectors (`BUTTON_COUNT == COUNT`), so
 * @ref select_function is never reached at runtime. MIDI CC numbers are
 * laid out as `instance * OSC_PARAM_COUNT + index`, giving CC `0..14`
 * across the three banks on MIDI channel `0`.
 */
class OSC : public UI_BLOCK<OSC, OSC_KNOB_COUNT, OSC_BUTTON_COUNT, OSC_PARAM_COUNT, OSC_COUNT> {
public:
    /** @brief Mux/LED bindings for the three bank-selector buttons. */
    static constexpr std::array button_configs_ = {
        Button::Config{ .mux_index = 3U, .pin = 3U, .led_number = 110U },
        Button::Config{ .mux_index = 3U, .pin = 2U, .led_number = 109U },
        Button::Config{ .mux_index = 3U, .pin = 1U, .led_number = 108U },
    };

    /** @brief Encoder/button/LED bindings for the five knobs. */
    static constexpr std::array knob_configs_ = {
        Knob::Config{
            .button_mux_index = 1U,
            .button_pin = 12U,
            .encoder_mux_index = 1U,
            .encoder_pin_a = 13U,
            .encoder_pin_b = 14U,
            .first_led = 96U,
            .led_count = 10U,
        },
        Knob::Config{
            .button_mux_index = 1U,
            .button_pin = 9U,
            .encoder_mux_index = 1U,
            .encoder_pin_a = 10U,
            .encoder_pin_b = 11U,
            .first_led = 78U,
            .led_count = 10U,
        },
        Knob::Config{
            .button_mux_index = 1U,
            .button_pin = 6U,
            .encoder_mux_index = 1U,
            .encoder_pin_a = 7U,
            .encoder_pin_b = 8U,
            .first_led = 68U,
            .led_count = 10U,
        },
        Knob::Config{
            .button_mux_index = 1U,
            .button_pin = 3U,
            .encoder_mux_index = 1U,
            .encoder_pin_a = 4U,
            .encoder_pin_b = 5U,
            .first_led = 58U,
            .led_count = 10U,
        },
        Knob::Config{
            .button_mux_index = 1U,
            .button_pin = 0U,
            .encoder_mux_index = 1U,
            .encoder_pin_a = 1U,
            .encoder_pin_b = 2U,
            .first_led = 48U,
            .led_count = 10U,
        },
    };

    /* ------------------------------------------------------------------ */
    /*  Required CRTP hooks                                               */
    /* ------------------------------------------------------------------ */

    /** @brief Returns the block name used in log output. */
    const char *get_name() { return "OSC"; }

    /**
     * @brief Maps a bank and parameter index to a MIDI CC number.
     *
     * Layout: CC `0..4` for bank 0, `5..9` for bank 1, `10..14` for bank 2.
     */
    uint8_t get_midi_nr(uint8_t instance, uint8_t index) {
        return static_cast<uint8_t>(instance * OSC_PARAM_COUNT + index);
    }

    /** @brief MIDI channel used for all OSC control-change messages. */
    uint8_t get_midi_ch() { return 0U; }
};

#endif /* APP_SRC_BLOCKS_OSC_H_ */

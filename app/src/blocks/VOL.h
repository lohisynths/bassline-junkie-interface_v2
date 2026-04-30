/**
 * @file VOL.h
 * @brief Volume control-surface block.
 *
 * Created on: Apr 5, 2026
 *     Author: alax
 */

#ifndef APP_SRC_BLOCKS_VOL_H_
#define APP_SRC_BLOCKS_VOL_H_

#include "LED_DISP.h"
#include "UI_BLOCK.h"

/** @brief Number of encoder knobs in the VOL block. */
static constexpr uint8_t VOL_KNOB_COUNT = 1U;

/** @brief Number of buttons in the VOL block. */
static constexpr uint8_t VOL_BUTTON_COUNT = 0U;

/** @brief Number of banked instances in the VOL block. */
static constexpr uint8_t VOL_COUNT = 1U;

/** @brief Number of logical parameters exposed by the VOL block. */
static constexpr uint8_t VOL_PARAM_COUNT = 1U;

/** @brief MIDI CC offset for the VOL block. */
static constexpr uint8_t VOL_MIDI_OFFSET = 95U;

/**
 * @brief Volume block owning one knob and no buttons.
 *
 * Hardware wiring is declared in the public constexpr config table and
 * consumed automatically by @ref UI_BLOCK::init through CRTP.
 *
 * The block emits MIDI CC `95` on channel `1` for the single volume
 * parameter.
 */
class VOL : public UI_BLOCK<VOL, VOL_KNOB_COUNT, VOL_BUTTON_COUNT, VOL_PARAM_COUNT, VOL_COUNT> {
public:
    /** @brief Shorthand for the CRTP base class used by VOL. */
    using ui_block = UI_BLOCK<VOL, VOL_KNOB_COUNT, VOL_BUTTON_COUNT, VOL_PARAM_COUNT, VOL_COUNT>;

    /** @brief Static block name used by the shared CRTP base logging. */
    static constexpr const char *block_name_ = "VOL";

    /** @brief MIDI CC offset for the VOL block. */
    static constexpr uint8_t midi_offset_ = VOL_MIDI_OFFSET;

    /** @brief MIDI channel used for VOL control-change messages. */
    static constexpr uint8_t midi_channel_ = 1U;

    /** @brief Mux/LED bindings for the single volume knob. */
    static constexpr std::array<Knob::Config, VOL_KNOB_COUNT> knob_configs_ = {
        Knob::Config{
            .button_mux_index = 4U,
            .button_pin = 3U,
            .encoder_mux_index = 4U,
            .encoder_pin_a = 4U,
            .encoder_pin_b = 5U,
            .first_led = 0U,
            .led_count = 0U,
            .encoder_step_divider = 1U,
        },
    };

    /** @brief No buttons exist in the VOL block. */
    static constexpr std::array<Button::Config, VOL_BUTTON_COUNT> button_configs_ = {};

};

#endif /* APP_SRC_BLOCKS_VOL_H_ */

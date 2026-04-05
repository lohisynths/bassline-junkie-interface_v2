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
 * @brief Logical parameter indices for the VOL block.
 */
enum VOL_PARAMS {
    VOL_LEVEL,
    VOL_PARAM_NR
};

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
    using ui_block = UI_BLOCK<VOL, VOL_KNOB_COUNT, VOL_BUTTON_COUNT, VOL_PARAM_COUNT, VOL_COUNT>;

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

    /** @brief Binds the shared display used to preview volume changes. */
    void bind_display(LED_DISP &display) {
        display_ = &display;
    }

    /** @brief Returns the block name used in log output. */
    const char *get_name() { return "VOL"; }

    /**
     * @brief Maps the single parameter to a MIDI CC number.
     *
     * The @p instance argument is unused because the VOL block is not banked.
     */
    uint8_t get_midi_nr(uint8_t /*instance*/, uint8_t index) {
        return static_cast<uint8_t>(VOL_MIDI_OFFSET + index);
    }

    /** @brief MIDI channel used for all VOL control-change messages. */
    uint8_t get_midi_ch() { return 1U; }

    /**
     * @brief Sends the new volume as MIDI and previews it on the display.
     */
    void knob_val_changed(uint8_t index, uint8_t value_scaled) {
        ui_block::knob_val_changed(index, value_scaled);

        if (display_ != nullptr) {
            display_->show_preview_value(value_scaled);
        }
    }

private:
    /** @brief Borrowed display used to preview volume changes. */
    LED_DISP *display_ = nullptr;
};

#endif /* APP_SRC_BLOCKS_VOL_H_ */

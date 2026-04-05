/**
 * @file VOL.h
 * @brief Master volume control-surface block.
 *
 * Created on: Apr 5, 2026
 *     Author: Codex
 */

#ifndef APP_SRC_BLOCKS_VOL_H_
#define APP_SRC_BLOCKS_VOL_H_

#include "UI_BLOCK.h"
#include "LED_DISP.h"

/** @brief Number of encoder knobs in the VOL block. */
static constexpr uint8_t VOL_KNOB_COUNT = 1U;

/** @brief Number of buttons in the VOL block. */
static constexpr uint8_t VOL_BUTTON_COUNT = 0U;

/** @brief Number of banked instances in the VOL block. */
static constexpr uint8_t VOL_COUNT = 1U;

/** @brief MIDI CC offset for the VOL block. */
static constexpr uint8_t VOL_MIDI_OFFSET = 95U;

/**
 * @brief Logical parameter indices for the VOL block.
 */
enum VOL_PARAMS {
    VOL_LEVEL,
    VOL_PARAM_COUNT
};

/**
 * @brief Single-encoder block used for master volume control.
 *
 * The block owns one encoder with push switch and no standalone buttons.
 * With one bank and one parameter, it emits MIDI CC `95` on channel `1`.
 */
class VOL : public UI_BLOCK<VOL, VOL_KNOB_COUNT, VOL_BUTTON_COUNT, VOL_PARAM_COUNT, VOL_COUNT> {
public:
    /** @brief Encoder/button binding for the volume encoder. */
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

    /** @brief VOL has no standalone buttons. */
    static constexpr std::array<Button::Config, VOL_BUTTON_COUNT> button_configs_ = {};

    /**
     * @brief Returns the block name used in log output.
     *
     * @return Block name.
     */
    const char *get_name() { return "VOL"; }

    /**
     * @brief Attaches the display block used for temporary volume previews.
     *
     * @param display Seven-segment display block.
     */
    void bind_display(LED_DISP &display) {
        display_ = &display;
    }

    /**
     * @brief Maps the single VOL parameter to MIDI CC 95.
     *
     * @param instance Unused bank index from the base CRTP interface.
     * @param index Parameter index.
     * @return MIDI CC number for the parameter.
     */
    uint8_t get_midi_nr(uint8_t instance, uint8_t index) {
        (void)instance;
        return static_cast<uint8_t>(VOL_MIDI_OFFSET + index);
    }

    /**
     * @brief MIDI channel used for VOL control-change messages.
     *
     * @return MIDI channel number.
     */
    uint8_t get_midi_ch() { return 1U; }

    /**
     * @brief Stores a knob value, sends its MIDI CC, and previews it on the display.
     *
     * @param index Changed knob index.
     * @param value_scaled Clamped knob value in the range `[0, 127]`.
     */
    void knob_val_changed(uint8_t index, uint8_t value_scaled) {
        set_current_preset_value(index, value_scaled);
        if (get_midi()) {
            get_midi()->send_cc(get_current_instance_midi_nr(index),
                                value_scaled,
                                get_midi_ch());
        }

        if (display_ != nullptr) {
            display_->show_temporary_value(value_scaled);
        }
    }

private:
    /** @brief Borrowed display block used for temporary volume readout. */
    LED_DISP *display_ = nullptr;
};

#endif /* APP_SRC_BLOCKS_VOL_H_ */

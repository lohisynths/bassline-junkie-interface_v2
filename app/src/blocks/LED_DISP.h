/**
 * @file LED_DISP.h
 * @brief 3-digit LED display control-surface block.
 *
 * Created on: Apr 1, 2026
 *     Author: alax
 */

#ifndef APP_SRC_BLOCKS_LED_DISP_H_
#define APP_SRC_BLOCKS_LED_DISP_H_

#include <zephyr/kernel.h>

#include "UI_BLOCK.h"

/** @brief Number of encoder knobs in the LED display block. */
static constexpr uint8_t LED_DISP_KNOB_COUNT = 1U;

/** @brief Number of buttons in the LED display block. */
static constexpr uint8_t LED_DISP_BUTTON_COUNT = 0U;

/** @brief Number of banked instances in the LED display block. */
static constexpr uint8_t LED_DISP_COUNT = 1U;

/** @brief Number of logical parameters exposed by the LED display block. */
static constexpr uint8_t LED_DISP_PARAM_COUNT = 1U;

/** @brief First LED channel used by the display digits. */
static constexpr uint16_t LED_DISP_FIRST_LED = 11U * 16U;

/** @brief Number of seven-segment elements reserved per digit. */
static constexpr uint8_t LED_DISP_SEGMENTS = 8U;

/** @brief Number of digits in the display. */
static constexpr uint8_t LED_DISP_DIGIT_COUNT = 3U;

/** @brief MIDI channel used for display compatibility. */
static constexpr uint8_t LED_DISP_MIDI_CHANNEL = 1U;

/**
 * @brief Logical parameter indices for the display block.
 *
 * The single parameter stores the current displayed value.
 */
enum LED_DISP_PARAMS {
    LED_DISP_VALUE,
    LED_DISP_PARAM_NR
};

/**
 * @brief 3-digit display block driven by a single encoder with push switch.
 *
 * Hardware wiring is declared in the public constexpr config tables and
 * consumed automatically by @ref UI_BLOCK::init through CRTP.
 *
 * The display value is stored in the single preset parameter and rendered on
 * a 3-digit seven-segment LED bank starting at @c LED_DISP_FIRST_LED.
 * The block keeps compatibility helpers (@ref get_long_push,
 * @ref preset_changed, and @ref get_actual_preset_nr) so polling code can
 * keep the same control flow.
 */
class LED_DISP : public UI_BLOCK<LED_DISP, LED_DISP_KNOB_COUNT, LED_DISP_BUTTON_COUNT, LED_DISP_PARAM_COUNT, LED_DISP_COUNT> {
public:
    using ui_block = UI_BLOCK<LED_DISP, LED_DISP_KNOB_COUNT, LED_DISP_BUTTON_COUNT, LED_DISP_PARAM_COUNT, LED_DISP_COUNT>;

    /** @brief Mux/LED bindings for the zero display buttons. */
    static constexpr std::array<Button::Config, LED_DISP_BUTTON_COUNT> button_configs_ = {};

    /** @brief Encoder/button binding for the display encoder. */
    static constexpr std::array<Knob::Config, LED_DISP_KNOB_COUNT> knob_configs_ = {
        Knob::Config{
            .button_mux_index = 4U,
            .button_pin = 6U,
            .encoder_mux_index = 4U,
            .encoder_pin_a = 7U,
            .encoder_pin_b = 8U,
            .first_led = 0U,
            .led_count = 0U,
            .encoder_step_divider = 4U,
        },
    };

    /** @brief Seven-segment bitmaps for digits 0..9. */
    static constexpr std::array<std::array<uint8_t, LED_DISP_SEGMENTS>, 10U> digits_ = {{
        {{1U, 1U, 1U, 1U, 1U, 1U, 0U, 0U}},
        {{0U, 1U, 1U, 0U, 0U, 0U, 0U, 0U}},
        {{1U, 1U, 0U, 1U, 1U, 0U, 1U, 0U}},
        {{1U, 1U, 1U, 1U, 0U, 0U, 1U, 0U}},
        {{0U, 1U, 1U, 0U, 0U, 1U, 1U, 0U}},
        {{1U, 0U, 1U, 1U, 0U, 1U, 1U, 0U}},
        {{1U, 0U, 1U, 1U, 1U, 1U, 1U, 0U}},
        {{1U, 1U, 1U, 0U, 0U, 0U, 0U, 0U}},
        {{1U, 1U, 1U, 1U, 1U, 1U, 1U, 0U}},
        {{1U, 1U, 1U, 1U, 0U, 1U, 1U, 0U}},
    }};

    /**
     * @brief Returns the block name used in log output.
     *
     * @return Block name.
     */
    const char *get_name() { return "Disp"; }

    /**
     * @brief Maps the single parameter to a compatibility MIDI CC number.
     *
     * @param instance Unused bank index from the base CRTP interface.
     * @param index Unused parameter index from the base CRTP interface.
     * @return Compatibility MIDI CC number.
     */
    uint8_t get_midi_nr(uint8_t instance, uint8_t index) {
        (void)instance;
        (void)index;
        return LED_DISP_MIDI_CHANNEL;
    }

    /**
     * @brief MIDI channel used for display compatibility.
     *
     * @return MIDI channel number.
     */
    uint8_t get_midi_ch() { return LED_DISP_MIDI_CHANNEL; }

    /**
     * @brief Binds the block and caches the LED controller for direct segment writes.
     *
     * @param midi MIDI transport used for display compatibility.
     * @param leds Shared LED controller for direct segment writes.
     * @param inputs Shared input controller used by the base class.
     */
    void init(MIDI &midi, LEDSController &leds, InputController &inputs) {
        leds_ = &leds;
        ui_block::init(midi, leds, inputs);
        refresh_display();
    }

    /**
     * @brief Polls the encoder and expires any active temporary overlay.
     *
     * @return Change flags from this update cycle.
     */
    ui_block::ret_value update() {
        const auto ret = ui_block::update();
        update_temporary_value_();
        return ret;
    }

    /**
     * @brief Loads a full preset and refreshes the LED display after the base class applies it.
     *
     * @param input Preset snapshot to apply.
     */
    void set_preset(const preset &input) {
        cancel_temporary_value_();
        ui_block::set_preset(input);
        refresh_display();
    }

    /**
     * @brief Loads a full modulation-routing preset and refreshes the display.
     *
     * @param input Modulation routing preset snapshot to apply.
     */
    void set_mod_preset(const mod_preset &input) {
        cancel_temporary_value_();
        ui_block::set_mod_preset(input);
        refresh_display();
    }

    /**
     * @brief Re-applies the current bank selection and refreshes the display.
     */
    void reset() {
        cancel_temporary_value_();
        ui_block::reset();
        refresh_display();
    }

    /**
     * @brief Stores the current display value, updates the LED digits, and keeps compatibility state in sync.
     *
     * @param index Unused knob index from the base CRTP interface.
     * @param value_scaled Display value to store.
     */
    void knob_val_changed(uint8_t index, uint8_t value_scaled) {
        (void)index;
        set_display_value(value_scaled);
    }

    /**
     * @brief Updates the displayed preset slot without generating MIDI.
     *
     * @param value_scaled Value to display, clamped to the valid slot range.
     */
    void set_display_value(uint8_t value_scaled) {
        if (value_scaled > 127U) {
            value_scaled = 127U;
        }

        cancel_temporary_value_();
        actual_preset_value_ = value_scaled;
        set_current_preset_value(LED_DISP_VALUE, value_scaled);
        last_preset_selected_ = static_cast<int>(value_scaled);
        render_value_(value_scaled);
    }

    /**
     * @brief Synchronizes the displayed preset number and the encoder state.
     *
     * This is used after preset load/save/browse restore so the next encoder
     * movement resumes from the currently active slot rather than the last
     * browsed value.
     *
     * @param value_scaled Preset slot to display and mirror into the encoder.
     */
    void sync_preset_value(uint8_t value_scaled) {
        set_display_value(value_scaled);

        auto &knobs = get_knobs();
        if (!knobs.empty()) {
            (void)knobs[0].set_value(value_scaled);
        }
    }

    /**
     * @brief Temporarily overlays the display with a non-preset value.
     *
     * The stored preset slot and encoder state stay unchanged; after a short
     * timeout the display returns to the real preset number automatically.
     *
     * @param value_scaled Value to preview on the seven-segment display.
     */
    void show_temporary_value(uint8_t value_scaled) {
        if (value_scaled > 127U) {
            value_scaled = 127U;
        }

        temporary_value_active_ = true;
        temporary_value_expires_at_ms_ = k_uptime_get_32() + temporary_value_hold_ms_;
        render_value_(value_scaled);
    }

    /**
     * @brief No function buttons exist on the display block.
     *
     * @param index Unused function-button index from the base CRTP interface.
     */
    void select_function(uint8_t index) {
        (void)index;
    }

    /**
     * @brief No special preset parameters need explicit restoration.
     *
     * @param value Unused special-parameter value from the base CRTP interface.
     */
    void force_function(uint8_t value) {
        (void)value;
    }

    /**
     * @brief Reports the encoder push-switch hold classification.
     *
     * @retval -1 Nothing was released yet.
     * @retval 1  The encoder switch was held longer than the configured threshold.
     * @retval 2  The encoder switch was released before the configured threshold.
     */
    int get_long_push() {
        const bool pushed = get_knobs()[0].get_state();
        const uint32_t now = k_uptime_get_32();

        if (pushed) {
            if (!last_disp_pushed_) {
                last_disp_pushed_ = true;
                last_disp_pressed_at_ms_ = now;
            }
            return -1;
        }

        if (!last_disp_pushed_) {
            return -1;
        }

        last_disp_pushed_ = false;
        const uint32_t held_ms = now - last_disp_pressed_at_ms_;
        return (held_ms >= long_push_ms_) ? 1 : 2;
    }

    /**
     * @brief Reports whether the displayed preset value changed during the last update.
     *
     * @return New preset slot number, or `-1` if nothing changed.
     */
    int preset_changed() {
        if (!get_knob_changed()) {
            return -1;
        }

        const int value = get_actual_preset_nr();
        if (last_preset_selected_ == value) {
            return -1;
        }

        last_preset_selected_ = value;
        return value;
    }

    /**
     * @brief Returns the value currently shown on the display.
     *
     * @return Displayed preset slot number.
     */
    uint8_t get_actual_preset_nr() {
        return actual_preset_value_;
    }

    /**
     * @brief Updates every display LED to the same brightness.
     *
     * @param val Brightness percentage in the range `[0, 100]`.
     */
    void set_all(uint16_t val) {
        if (leds_ == nullptr) {
            return;
        }

        if (val > 100U) {
            val = 100U;
        }

        for (uint8_t i = 0U; i < (LED_DISP_SEGMENTS * LED_DISP_DIGIT_COUNT); i++) {
            leds_->set_channel_percent(static_cast<size_t>(LED_DISP_FIRST_LED + i),
                                       static_cast<uint8_t>(100U - val));
        }
    }

private:
    /**
     * @brief Re-renders the display from the stored preset value.
     */
    void refresh_display() {
        render_value_(get_current_preset_value(LED_DISP_VALUE));
    }

    /**
     * @brief Restores the preset-slot display once a temporary overlay expires.
     */
    void update_temporary_value_() {
        if (!temporary_value_active_) {
            return;
        }

        const uint32_t now = k_uptime_get_32();
        if ((int32_t)(now - temporary_value_expires_at_ms_) < 0) {
            return;
        }

        cancel_temporary_value_();
        refresh_display();
    }

    /**
     * @brief Clears any active temporary overlay without changing the preset.
     */
    void cancel_temporary_value_() {
        temporary_value_active_ = false;
        temporary_value_expires_at_ms_ = 0U;
    }

    /**
     * @brief Renders one digit of the display.
     *
     * @param digit_nr Digit position in the range `[0, LED_DISP_DIGIT_COUNT)`.
     * @param digit Decimal digit in the range `[0, 9]`.
     */
    void set_digit(uint8_t digit_nr, uint8_t digit) {
        if (leds_ == nullptr || digit_nr >= LED_DISP_DIGIT_COUNT || digit > 9U) {
            return;
        }

        for (uint8_t i = 0U; i < 7U; i++) {
            const size_t led_nr = static_cast<size_t>(LED_DISP_FIRST_LED + i + (digit_nr * LED_DISP_SEGMENTS));
            leds_->set_channel_percent(led_nr,
                                       digits_[digit][i] ? 0U : 100U);
        }

        // Keep the decimal point disabled on every digit.
        leds_->set_channel_percent(static_cast<size_t>(LED_DISP_FIRST_LED + 7U + (digit_nr * LED_DISP_SEGMENTS)),
                                   100U);
    }

    /**
     * @brief Splits the value into three digits and updates the LED bank.
     *
     * @param value_scaled Value to render as a three-digit number.
     */
    void render_value_(uint8_t value_scaled) {
        actual_preset_value_ = value_scaled;

        const uint8_t ones = static_cast<uint8_t>(value_scaled % 10U);
        const uint8_t tens = static_cast<uint8_t>((value_scaled / 10U) % 10U);
        const uint8_t hundreds = static_cast<uint8_t>(value_scaled / 100U);

        set_digit(2U, ones);
        set_digit(1U, tens);
        set_digit(0U, hundreds);
    }

    /** @brief Cached LED controller used for direct display updates. */
    LEDSController *leds_ = nullptr;

    /** @brief Tracks the value currently shown by the display. */
    uint8_t actual_preset_value_ = 0U;

    /** @brief Hold threshold in milliseconds. */
    static constexpr uint32_t long_push_ms_ = 1000U;

    /** @brief Press timestamp for the encoder push-switch. */
    uint32_t last_disp_pressed_at_ms_ = 0U;

    /** @brief Edge detector for the encoder push-switch. */
    bool last_disp_pushed_ = false;

    /** @brief Preset-change guard. */
    int last_preset_selected_ = 0;

    /** @brief Temporary overlay hold time in milliseconds. */
    static constexpr uint32_t temporary_value_hold_ms_ = 750U;

    /** @brief Tracks whether a non-preset display overlay is active. */
    bool temporary_value_active_ = false;

    /** @brief Deadline for restoring the preset-slot display after an overlay. */
    uint32_t temporary_value_expires_at_ms_ = 0U;
};

#endif /* APP_SRC_BLOCKS_LED_DISP_H_ */

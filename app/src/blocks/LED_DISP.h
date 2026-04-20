/**
 * @file LED_DISP.h
 * @brief Up to 3-digit LED display control-surface block.
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

/** @brief Legacy MIDI CC offset used by the display block. */
static constexpr uint8_t LED_DISP_MIDI_OFFSET = 1U;

/** @brief MIDI channel used for legacy compatibility. */
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
 * @brief Up to 3-digit display block driven by a single encoder with push switch.
 *
 * Hardware wiring is declared in the public constexpr config tables and
 * consumed automatically by @ref UI_BLOCK::init through CRTP.
 *
 * The display value is stored in the single preset parameter and rendered on
 * a 3-digit seven-segment LED bank starting at @c LED_DISP_FIRST_LED, with
 * leading zeros suppressed.
 * The block keeps the old helper methods (@ref get_long_push,
 * @ref preset_changed, and @ref get_actual_preset_nr) so legacy polling code
 * can still use the same control flow.
 */
class LED_DISP : public UI_BLOCK<LED_DISP, LED_DISP_KNOB_COUNT, LED_DISP_BUTTON_COUNT, LED_DISP_PARAM_COUNT, LED_DISP_COUNT> {
public:
    /** @brief Shorthand for the CRTP base class used by LED_DISP. */
    using ui_block = UI_BLOCK<LED_DISP, LED_DISP_KNOB_COUNT, LED_DISP_BUTTON_COUNT, LED_DISP_PARAM_COUNT, LED_DISP_COUNT>;

    /** @brief Static block name used by the shared CRTP base logging. */
    static constexpr const char *block_name_ = "DISP";

    /** @brief Legacy MIDI CC offset used by the display block. */
    static constexpr uint8_t midi_offset_ = LED_DISP_MIDI_OFFSET;

    /** @brief MIDI channel used for legacy compatibility. */
    static constexpr uint8_t midi_channel_ = LED_DISP_MIDI_CHANNEL;

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
            .pressed_step_multiplier = 1U,
        },
    };

    /** @brief Legacy seven-segment bitmaps for digits 0..9. */
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
     * @brief Binds the block and caches the LED controller for direct segment writes.
     *
     * @param midi MIDI backend borrowed by the CRTP base block.
     * @param leds LED controller used both by the base block and direct digit rendering.
     * @param inputs Input controller used to initialize the display encoder.
     */
    void init(MIDI &midi, LEDSController &leds, InputController &inputs) {
        leds_ = &leds;
        ui_block::init(midi, leds, inputs);
        refresh_display();
    }

    /**
     * @brief Loads a full preset and refreshes the LED display after the base class applies it.
     *
     * @param input Full banked display preset to load.
     */
    void set_preset(const preset &input) {
        ui_block::set_preset(input);
        clear_preview_();
        refresh_display();
    }

    /**
     * @brief Loads a full mod-routing preset and refreshes the display.
     *
     * @param input Full mod-routing preset to load.
     */
    void set_mod_preset(const mod_preset &input) {
        ui_block::set_mod_preset(input);
        clear_preview_();
        refresh_display();
    }

    /**
     * @brief Re-applies the current bank selection and refreshes the display.
     */
    void reset() {
        ui_block::reset();
        clear_preview_();
        refresh_display();
    }

    /**
     * @brief Polls the base UI block and restores the preset slot after a short preview timeout.
     *
     * @return Aggregated update flags from the CRTP base block.
     */
    ui_block::ret_value update() {
        const ui_block::ret_value ret = ui_block::update();
        update_preview_restore_();
        return ret;
    }

    /**
     * @brief Stores the current display value, updates the LED digits, and keeps legacy state in sync.
     *
     * @param index Unused knob index placeholder.
     * @param value_scaled New display value in the range `[0, 127]`.
     */
    void knob_val_changed(uint8_t index, uint8_t value_scaled) {
        (void)index;
        set_display_value(value_scaled);
    }

    /**
     * @brief Updates the displayed preset slot without generating MIDI.
     *
     * @param value_scaled New preset slot number to show and store.
     */
    void set_display_value(uint8_t value_scaled) {
        if (value_scaled > 127U) {
            value_scaled = 127U;
        }

        clear_preview_();
        actual_preset_value_ = value_scaled;
        set_current_preset_value(LED_DISP_VALUE, value_scaled);
        last_preset_selected_ = static_cast<int>(value_scaled);
        render_value_(value_scaled);
    }

    /**
     * @brief Renders a transient value on the display without changing the stored preset slot.
     *
     * This is used by other blocks to preview parameter values while leaving the preset browsing
     * state untouched.
     *
     * @param value_scaled Temporary preview value to render.
     */
    void show_preview_value(uint8_t value_scaled) {
        if (value_scaled > 127U) {
            value_scaled = 127U;
        }

        preview_active_ = true;
        preview_ends_at_ms_ = k_uptime_get_32() + preview_ms_;
        render_digits_(value_scaled);
    }

    /**
     * @brief Synchronizes the displayed preset number and the encoder state.
     *
     * This is used after preset load/save/browse restore so the next encoder
     * movement resumes from the currently active slot rather than the last
     * browsed value.
     *
     * @param value_scaled Preset slot value to synchronize into the encoder and display.
     */
    void sync_preset_value(uint8_t value_scaled) {
        set_display_value(value_scaled);

        auto &knobs = get_knobs();
        if (!knobs.empty()) {
            (void)knobs[0].set_value(value_scaled);
        }
    }

    /** @brief No function buttons exist on the display block. */
    void select_function(uint8_t /*index*/) {}

    /** @brief No special preset parameters need explicit restoration. */
    void force_function(uint8_t /*value*/) {}

    /**
     * @brief Returns the first pressed knob switch and classifies the hold length.
     *
     * @retval -1 Nothing was released yet.
     * @retval 1  The encoder switch was held longer than the legacy threshold.
     * @retval 2  The encoder switch was released before the legacy threshold.
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
     * @return New preset number when it changed, otherwise `-1`.
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
     * @brief Returns the currently stored display value.
     *
     * @return Last committed preset number shown by the display.
     */
    uint8_t get_actual_preset_nr() {
        return actual_preset_value_;
    }

    /**
     * @brief Updates every display LED to the same brightness.
     *
     * @param val Brightness percentage to apply to all display segments.
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
     * @brief Renders one digit of the display.
     *
     * @param digit_nr Zero-based digit index to update.
     * @param digit Decimal digit value in the range `[0, 9]`.
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
     * @brief Blanks one digit of the display.
     *
     * @param digit_nr Zero-based digit index to clear.
     */
    void clear_digit(uint8_t digit_nr) {
        if (leds_ == nullptr || digit_nr >= LED_DISP_DIGIT_COUNT) {
            return;
        }

        for (uint8_t i = 0U; i < LED_DISP_SEGMENTS; i++) {
            leds_->set_channel_percent(static_cast<size_t>(LED_DISP_FIRST_LED + i + (digit_nr * LED_DISP_SEGMENTS)),
                                       100U);
        }
    }

    /**
     * @brief Splits the value into three digits and updates the LED bank.
     *
     * @param value_scaled Display value to render and store.
     */
    void render_value_(uint8_t value_scaled) {
        actual_preset_value_ = value_scaled;
        render_digits_(value_scaled);
    }

    /**
     * @brief Renders the numeric digits without touching the stored preset value.
     *
     * @param value_scaled Display value to render temporarily.
     */
    void render_digits_(uint8_t value_scaled) {
        const uint8_t ones = static_cast<uint8_t>(value_scaled % 10U);
        const uint8_t tens = static_cast<uint8_t>((value_scaled / 10U) % 10U);
        const uint8_t hundreds = static_cast<uint8_t>(value_scaled / 100U);

        if (value_scaled >= 100U) {
            set_digit(0U, hundreds);
            set_digit(1U, tens);
        } else if (value_scaled >= 10U) {
            clear_digit(0U);
            set_digit(1U, tens);
        } else {
            clear_digit(0U);
            clear_digit(1U);
        }

        set_digit(2U, ones);
    }

    /**
     * @brief Clears any active temporary preview state.
     */
    void clear_preview_() {
        preview_active_ = false;
        preview_ends_at_ms_ = 0U;
    }

    /**
     * @brief Restores the preset slot after the preview timeout expires.
     */
    void update_preview_restore_() {
        if (!preview_active_) {
            return;
        }

        const uint32_t now = k_uptime_get_32();
        if ((int32_t)(now - preview_ends_at_ms_) < 0) {
            return;
        }

        preview_active_ = false;
        preview_ends_at_ms_ = 0U;
        refresh_display();
    }

    /** @brief Cached LED controller used for direct display updates. */
    LEDSController *leds_ = nullptr;

    /** @brief Tracks the value currently shown by the display. */
    uint8_t actual_preset_value_ = 0U;

    /** @brief Hold threshold in milliseconds. */
    static constexpr uint32_t long_push_ms_ = 1000U;

    /** @brief Legacy press timestamp for the encoder push-switch. */
    uint32_t last_disp_pressed_at_ms_ = 0U;

    /** @brief Legacy edge detector for the encoder push-switch. */
    bool last_disp_pushed_ = false;

    /** @brief Legacy preset-change guard. */
    int last_preset_selected_ = 0;

    /** @brief Tracks whether a temporary parameter preview is active. */
    bool preview_active_ = false;

    /** @brief Timestamp when the parameter preview should end. */
    uint32_t preview_ends_at_ms_ = 0U;

    /** @brief Duration of the parameter preview in milliseconds. */
    static constexpr uint32_t preview_ms_ = 1000U;
};

#endif /* APP_SRC_BLOCKS_LED_DISP_H_ */

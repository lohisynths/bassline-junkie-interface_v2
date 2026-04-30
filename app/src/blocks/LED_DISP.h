/**
 * @file LED_DISP.h
 * @brief Up to 3-digit LED display renderer and preview helper.
 *
 * Created on: Apr 1, 2026
 *     Author: alax
 */

#ifndef APP_SRC_BLOCKS_LED_DISP_H_
#define APP_SRC_BLOCKS_LED_DISP_H_

#include <array>
#include <cstdint>

#include <zephyr/kernel.h>

#include "LEDS.h"
#include "UI_BLOCK.h"

/** @brief First LED channel used by the display digits. */
static constexpr uint16_t LED_DISP_FIRST_LED = 11U * 16U;

/** @brief Number of seven-segment elements reserved per digit. */
static constexpr uint8_t LED_DISP_SEGMENTS = 8U;

/** @brief Number of digits in the display. */
static constexpr uint8_t LED_DISP_DIGIT_COUNT = 3U;

/**
 * @brief Up to 3-digit display wrapper with stored value and transient preview support.
 *
 * This class owns only the LED segment rendering and preview timeout state.
 * The preset encoder/button wiring lives in @ref Preset.
 */
class LED_DISP {
public:
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
     * @brief Binds the display and renders the initial stored value.
     *
     * @param leds LED controller used for direct digit rendering.
     */
    void init(LEDSController &leds) {
        leds_ = &leds;
        refresh_display();
    }

    /**
     * @brief Restores the stored value after the preview timeout expires.
     */
    void update() {
        update_preview_restore_();
    }

    /**
     * @brief Stores and renders one display value.
     *
     * @param value_scaled New value to show.
     */
    void set_display_value(uint8_t value_scaled) {
        if (value_scaled > 127U) {
            value_scaled = 127U;
        }

        clear_preview_();
        actual_preset_value_ = value_scaled;
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
     * movement resumes from the currently active slot rather than the last browsed value.
     *
     * @param value_scaled Preset slot value to synchronize into the encoder and display.
     */
    void sync_preset_value(uint8_t value_scaled) {
        set_display_value(value_scaled);
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
        render_value_(actual_preset_value_);
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

    /** @brief Tracks whether a temporary parameter preview is active. */
    bool preview_active_ = false;

    /** @brief Timestamp when the parameter preview should end. */
    uint32_t preview_ends_at_ms_ = 0U;

    /** @brief Duration of the parameter preview in milliseconds. */
    static constexpr uint32_t preview_ms_ = 1000U;
};

#endif /* APP_SRC_BLOCKS_LED_DISP_H_ */

#ifndef SRC_BLOCKS_LED_DISP_H_
#define SRC_BLOCKS_LED_DISP_H_

#include "InputController.h"
#include "Knob.h"
#include "LEDS.h"

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Encapsulates the 3-digit LED display block and its dedicated knob.
 *
 * The block owns one knob with no local LED ring and projects its `0..127`
 * value onto a 3-digit 7-segment display with decimal-point LEDs. The display
 * outputs are currently active-low, so a `0%` PWM duty lights a segment and
 * `100%` turns it off.
 */
class LED_DISP {
public:
    /**
     * @brief Initializes the knob and renders the initial display value.
     *
     * @param inputs Shared input controller used by the knob.
     * @param leds Shared LED controller used by the display.
     *
     * @retval 0 The knob and display were initialized successfully.
     * @retval negative Error propagated from @ref Knob::init or display updates.
     */
    int init(InputController &inputs, LEDSController &leds);

    /**
     * @brief Updates the knob, logs its transitions, and refreshes the display.
     *
     * @retval 0 The block state was updated successfully.
     * @retval negative Error propagated from @ref Knob::update or display writes.
     */
    int update();

private:
    /** @brief First global LED channel reserved for the 3-digit display. */
    static const size_t display_first_led_ = 11U * 16U;

    /** @brief Number of digits in the display. */
    static const size_t display_digit_count_ = 3U;

    /** @brief Number of segments per digit, including the decimal point. */
    static const size_t display_segments_per_digit_ = 8U;

    /** @brief PWM percentage that lights one active-low display segment. */
    static const uint8_t display_on_percent_ = 0U;

    /** @brief PWM percentage that turns one active-low display segment off. */
    static const uint8_t display_off_percent_ = 100U;

    /** @brief Static knob binding for the display encoder and push button. */
    static constexpr Knob::Config knob_config_ = {
        .button_mux_index = 4U,
        .button_pin = 3U,
        .encoder_mux_index = 4U,
        .encoder_pin_a = 4U,
        .encoder_pin_b = 5U,
        .first_led = 0U,
        .led_count = 0U,
    };

    /** @brief Segment pattern used to blank one digit. */
    static constexpr bool blank_digit_pattern_[display_segments_per_digit_] = {
        false, false, false, false, false, false, false, false,
    };

    /** @brief Segment patterns for decimal digits 0..9 in the legacy wiring order. */
    static constexpr bool digit_patterns_[10][display_segments_per_digit_] = {
        {true, true, true, true, true, true, false, false},
        {false, true, true, false, false, false, false, false},
        {true, true, false, true, true, false, true, false},
        {true, true, true, true, false, false, true, false},
        {false, true, true, false, false, true, true, false},
        {true, false, true, true, false, true, true, false},
        {true, false, true, true, true, true, true, false},
        {true, true, true, false, false, false, false, false},
        {true, true, true, true, true, true, true, false},
        {true, true, true, true, false, true, true, false},
    };

    /**
     * @brief Writes one digit pattern to the display.
     *
     * @param digit_index Digit position in the range `[0, display_digit_count_)`.
     * @param pattern Segment pattern to write.
     *
     * @retval 0 The digit was updated successfully.
     * @retval negative Error propagated from @ref LEDSController::set_channel_percent.
     */
    int set_digit_(size_t digit_index, const bool (&pattern)[display_segments_per_digit_]);

    /**
     * @brief Renders the current knob value onto the 3-digit display.
     *
     * Leading zero digits are blanked while the ones digit is always shown.
     *
     * @retval 0 The display was updated successfully.
     * @retval negative Error propagated from @ref set_digit_.
     */
    int render_value_();

    /** @brief Display knob owned by the block. */
    Knob knob_;

    /** @brief Shared LED controller borrowed from the caller. */
    LEDSController *leds_ = nullptr;
};

#endif /* SRC_BLOCKS_LED_DISP_H_ */

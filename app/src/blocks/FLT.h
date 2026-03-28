#ifndef SRC_BLOCKS_FLT_H_
#define SRC_BLOCKS_FLT_H_

#include "Button.h"
#include "InputController.h"
#include "Knob.h"
#include "LEDS.h"

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Encapsulates the FLT control surface block.
 *
 * The block owns three radio buttons and three standalone knobs. One button is
 * selected at a time and reflected on the button LEDs, and the knobs emit
 * transition logs while managing their own LED segments when configured.
 */
class FLT {
public:
    /**
     * @brief Initializes all buttons and knobs in the FLT block.
     *
     * @param inputs Shared input controller used by all block controls.
     * @param leds Shared LED controller used by all block controls.
     *
     * @retval 0 All controls were initialized successfully.
     * @retval negative Error propagated from @ref Button::init or @ref Knob::init.
     */
    int init(InputController &inputs, LEDSController &leds);

    /**
     * @brief Updates all controls and emits the current transition logs.
     *
     * @retval 0 All controls were updated successfully.
     * @retval negative Error propagated from a contained control update.
     */
    int update();

    /**
     * @brief Returns the first knob pressed during the most recent update.
     *
     * @param knob_index Receives the zero-based knob index when one is pending.
     *
     * @retval true A knob press event was returned and consumed.
     * @retval false No knob press event is pending.
     */
    bool take_newly_pressed_knob(size_t &knob_index);

private:
    /** @brief Number of LEDs reserved for standard knob segments in this block. */
    static const size_t knob_led_count_ = 10U;

    /** @brief Static configuration for the standalone buttons. */
    static constexpr Button::Config button_configs_[] = {
        {
            .mux_index = 3U,
            .pin = 15U,
            .led_number = 174U,
        },
        {
            .mux_index = 3U,
            .pin = 14U,
            .led_number = 173U,
        },
        {
            .mux_index = 3U,
            .pin = 13U,
            .led_number = 172U,
        },
    };

    /** @brief Static configuration for the standalone knobs. */
    static constexpr Knob::Config knob_configs_[] = {
        {
            .button_mux_index = 3U,
            .button_pin = 4U,
            .encoder_mux_index = 3U,
            .encoder_pin_a = 5U,
            .encoder_pin_b = 6U,
            .first_led = 144U,
            .led_count = 12U,
        },
        {
            .button_mux_index = 3U,
            .button_pin = 7U,
            .encoder_mux_index = 3U,
            .encoder_pin_a = 8U,
            .encoder_pin_b = 9U,
            .first_led = 160U,
            .led_count = knob_led_count_,
        },
        {
            .button_mux_index = 3U,
            .button_pin = 10U,
            .encoder_mux_index = 3U,
            .encoder_pin_a = 11U,
            .encoder_pin_b = 12U,
            .first_led = 170U,
            .led_count = 0U,
        },
    };

    /** @brief Number of buttons in this block. */
    static const size_t button_count_ = ARRAY_SIZE(button_configs_);

    /** @brief Number of knobs in this block. */
    static const size_t knob_count_ = ARRAY_SIZE(knob_configs_);

    /**
     * @brief Updates the button LEDs to reflect the currently selected radio button.
     *
     * @retval 0 All button LEDs match the current radio selection.
     * @retval negative Error propagated from @ref Button::set_led_val.
     */
    int update_selector_leds_();

    /** @brief Buttons owned by the block. */
    Button buttons_[button_count_];

    /** @brief Knobs owned by the block. */
    Knob knobs_[knob_count_];

    /** @brief Currently selected radio button. */
    uint8_t selected_button_ = 0U;

    /** @brief Set when one knob button was newly pressed during the latest update. */
    bool has_newly_pressed_knob_ = false;

    /** @brief Stores the first newly pressed knob index from the latest update. */
    uint8_t newly_pressed_knob_index_ = 0U;
};

#endif /* SRC_BLOCKS_FLT_H_ */

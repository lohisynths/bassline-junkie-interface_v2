#ifndef SRC_BLOCKS_LFO_H_
#define SRC_BLOCKS_LFO_H_

#include "Button.h"
#include "InputController.h"
#include "Knob.h"
#include "LEDS.h"

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Encapsulates the LFO control surface block.
 *
 * The block owns one knob, three bank-selector buttons, and five radio
 * buttons. The selector buttons choose one of three banks, each bank stores
 * one knob value, and each bank also stores one active radio-button choice.
 */
class LFO {
public:
    /**
     * @brief Initializes all buttons and the knob in the LFO block.
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

private:
    /** @brief Number of LEDs reserved for the knob segment. */
    static const size_t knob_led_count_ = 10U;

    /** @brief Number of selector banks exposed by the first three buttons. */
    static const size_t bank_count_ = 3U;

    /** @brief Number of radio buttons after the bank selectors. */
    static const size_t radio_button_count_ = 5U;

    /** @brief Index of the first radio button in @ref button_configs_. */
    static const size_t radio_button_offset_ = 3U;

    /** @brief Static configuration for the LFO buttons. */
    static constexpr Button::Config button_configs_[] = {
        {
            .mux_index = 3U,
            .pin = 0U,
            .led_number = 158U,
        },
        {
            .mux_index = 2U,
            .pin = 15U,
            .led_number = 157U,
        },
        {
            .mux_index = 2U,
            .pin = 14U,
            .led_number = 156U,
        },
        {
            .mux_index = 2U,
            .pin = 13U,
            .led_number = 142U,
        },
        {
            .mux_index = 2U,
            .pin = 12U,
            .led_number = 141U,
        },
        {
            .mux_index = 2U,
            .pin = 11U,
            .led_number = 140U,
        },
        {
            .mux_index = 2U,
            .pin = 10U,
            .led_number = 139U,
        },
        {
            .mux_index = 2U,
            .pin = 9U,
            .led_number = 138U,
        },
    };

    /** @brief Static configuration for the LFO knob control. */
    static constexpr Knob::Config knob_configs_[] = {
        {
            .button_mux_index = 4U,
            .button_pin = 0U,
            .encoder_mux_index = 4U,
            .encoder_pin_a = 1U,
            .encoder_pin_b = 2U,
            .first_led = 128U,
            .led_count = knob_led_count_,
        },
    };

    /** @brief Number of buttons in this block. */
    static const size_t button_count_ = ARRAY_SIZE(button_configs_);

    /** @brief Number of knobs in this block. */
    static const size_t knob_count_ = ARRAY_SIZE(knob_configs_);

    /**
     * @brief Selects one knob-value bank and recalls its values onto the knob.
     *
     * @param bank_index Bank index in the range `[0, bank_count_)`.
     *
     * @retval 0 The bank was selected and its values were recalled successfully.
     * @retval -EINVAL The requested bank index is out of range.
     * @retval negative Error propagated from LED or knob updates.
     */
    int select_bank_(size_t bank_index);

    /**
     * @brief Updates the selector and radio-button LEDs to match the active bank state.
     *
     * @retval 0 All button LEDs match the current bank and radio selection.
     * @retval negative Error propagated from @ref Button::set_led_val.
     */
    int update_selector_leds_();

    /**
     * @brief Loads one stored bank into the live knob object.
     *
     * @param bank_index Bank index in the range `[0, bank_count_)`.
     *
     * @retval 0 The knob value was recalled successfully.
     * @retval -EINVAL The requested bank index is out of range.
     * @retval negative Error propagated from @ref Knob::set_value.
     */
    int recall_bank_to_knobs_(size_t bank_index);

    /** @brief Buttons owned by the block. */
    Button buttons_[button_count_];

    /** @brief Knob control owned by the block. */
    Knob knobs_[knob_count_];

    /** @brief Currently selected bank. */
    uint8_t selected_bank_ = 0U;

    /** @brief Stored knob values for each selector bank. */
    uint8_t knob_values_[bank_count_][knob_count_] = {};

    /** @brief Active radio-button selection stored independently for each bank. */
    uint8_t radio_selection_[bank_count_] = {};
};

#endif /* SRC_BLOCKS_LFO_H_ */

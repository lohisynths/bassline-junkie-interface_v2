#ifndef SRC_BLOCKS_ADSR_H_
#define SRC_BLOCKS_ADSR_H_

#include "Button.h"
#include "InputController.h"
#include "Knob.h"
#include "LEDS.h"
#include "PresetSnapshot.h"

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Encapsulates the current ADSR control surface block.
 *
 * The block owns the standalone button and knob facades that are currently
 * wired in the main application. It borrows the shared input and LED
 * controllers from the caller.
 */
class ADSR {
public:
    /**
     * @brief Initializes all buttons and knobs in the ADSR block.
     *
     * @param inputs Shared input controller used by all block controls.
     * @param leds Shared LED controller used by all block controls.
     *
     * @retval 0 All controls were initialized successfully.
     * @retval negative Error propagated from @ref Button::init or @ref Knob::init.
     */
    int init(InputController &inputs, LEDSController &leds);

    /**
     * @brief Captures the current durable ADSR block state.
     */
    void capture_state(ADSRState &state) const;

    /**
     * @brief Applies one durable ADSR block state snapshot.
     *
     * @retval 0 The requested state was applied successfully.
     * @retval negative Error propagated from LED or knob updates.
     */
    int apply_state(const ADSRState &state);

    /**
     * @brief Updates all controls and emits the current transition logs.
     *
     * @retval 0 All controls were updated successfully.
     * @retval negative Error propagated from a contained control update.
     */
    int update();

private:
    /** @brief Number of LEDs reserved for each knob segment. */
    static const size_t knob_led_count_ = 10U;

    /** @brief Number of latched selector banks exposed by the first three buttons. */
    static const size_t bank_count_ = 3U;

    /** @brief Static configuration for the standalone buttons. */
    static constexpr Button::Config button_configs_[] = {
        {
            .mux_index = 0U,
            .pin = 15U,
            .led_number = 43U,
        },
        {
            .mux_index = 0U,
            .pin = 14U,
            .led_number = 42U,
        },
        {
            .mux_index = 0U,
            .pin = 13U,
            .led_number = 41U,
        },
        {
            .mux_index = 0U,
            .pin = 12U,
            .led_number = 40U,
        },
    };

    /** @brief Static configuration for the knob controls. */
    static constexpr Knob::Config knob_configs_[] = {
        {
            .button_mux_index = 0U,
            .button_pin = 9U,
            .encoder_mux_index = 0U,
            .encoder_pin_a = 10U,
            .encoder_pin_b = 11U,
            .first_led = 30U,
            .led_count = knob_led_count_,
        },
        {
            .button_mux_index = 0U,
            .button_pin = 6U,
            .encoder_mux_index = 0U,
            .encoder_pin_a = 7U,
            .encoder_pin_b = 8U,
            .first_led = 20U,
            .led_count = knob_led_count_,
        },
        {
            .button_mux_index = 0U,
            .button_pin = 3U,
            .encoder_mux_index = 0U,
            .encoder_pin_a = 4U,
            .encoder_pin_b = 5U,
            .first_led = 10U,
            .led_count = knob_led_count_,
        },
        {
            .button_mux_index = 0U,
            .button_pin = 0U,
            .encoder_mux_index = 0U,
            .encoder_pin_a = 1U,
            .encoder_pin_b = 2U,
            .first_led = 0U,
            .led_count = knob_led_count_,
        },
    };

    /** @brief Number of standalone buttons in this block. */
    static const size_t button_count_ = ARRAY_SIZE(button_configs_);

    /** @brief Number of knobs in this block. */
    static const size_t knob_count_ = ARRAY_SIZE(knob_configs_);

    /**
     * @brief Selects one knob-value bank and recalls its values onto the four knobs.
     *
     * @param bank_index Bank index in the range `[0, bank_count_)`.
     *
     * @retval 0 The bank was selected and its values were recalled successfully.
     * @retval -EINVAL The requested bank index is out of range.
     * @retval negative Error propagated from LED or knob updates.
     */
    int select_bank_(size_t bank_index);

    /**
     * @brief Updates the selector-button LEDs to show the currently active bank.
     *
     * @retval 0 All selector LEDs match the selected bank.
     * @retval negative Error propagated from @ref Button::set_led_val.
     */
    int update_selector_leds_();

    /**
     * @brief Loads one stored bank into the live knob objects.
     *
     * @param bank_index Bank index in the range `[0, bank_count_)`.
     *
     * @retval 0 All knob values were recalled successfully.
     * @retval -EINVAL The requested bank index is out of range.
     * @retval negative Error propagated from @ref Knob::set_value.
     */
    int recall_bank_to_knobs_(size_t bank_index);

    /** @brief Standalone buttons owned by the block. */
    Button buttons_[button_count_];

    /** @brief Knob controls owned by the block. */
    Knob knobs_[knob_count_];

    /** @brief Currently selected knob-value bank. */
    uint8_t selected_bank_ = 0U;

    /** @brief Stored knob values for each selector bank. */
    uint8_t knob_values_[bank_count_][knob_count_] = {};

    /** @brief Latched button-3 state stored independently for each bank. */
    bool button3_values_[bank_count_] = {};
};

#endif /* SRC_BLOCKS_ADSR_H_ */

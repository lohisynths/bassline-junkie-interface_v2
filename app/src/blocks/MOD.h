#ifndef SRC_BLOCKS_MOD_H_
#define SRC_BLOCKS_MOD_H_

#include "Button.h"
#include "InputController.h"
#include "Knob.h"
#include "LEDS.h"
#include "PresetSnapshot.h"

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Encapsulates the MOD control surface block.
 *
 * The block owns one knob and six selector buttons. The selector buttons pick
 * one of six banks, and each bank stores one value for that knob.
 */
class MOD {
public:
    /**
     * @brief Initializes all buttons and the knob in the MOD block.
     *
     * @param inputs Shared input controller used by all block controls.
     * @param leds Shared LED controller used by all block controls.
     *
     * @retval 0 All controls were initialized successfully.
     * @retval negative Error propagated from @ref Button::init or @ref Knob::init.
     */
    int init(InputController &inputs, LEDSController &leds);

    /**
     * @brief Captures the current durable MOD block state.
     */
    void capture_state(MODState &state) const;

    /**
     * @brief Applies one durable MOD block state snapshot.
     *
     * @retval 0 The requested state was applied successfully.
     * @retval negative Error propagated from LED or knob updates.
     */
    int apply_state(const MODState &state);

    /**
     * @brief Updates all controls and emits the current transition logs.
     *
     * @retval 0 All controls were updated successfully.
     * @retval negative Error propagated from a contained control update.
     */
    int update();

    /**
     * @brief Returns the current pressed state of the MOD knob button.
     */
    bool mod_knob_pressed() const;

    /**
     * @brief Logs one identified cross-block target with bank context.
     *
     * @param block_name Name of the block that owns the pressed knob.
     * @param knob_index Zero-based knob index inside that block.
     * @param bank_index Active bank index for the owning block.
     */
    void report_link_target(const char *block_name, size_t knob_index, uint8_t bank_index);

private:
    /** @brief Number of LEDs reserved for the knob segment. */
    static const size_t knob_led_count_ = 10U;

    /** @brief Number of selector banks exposed by the buttons. */
    static const size_t bank_count_ = 6U;

    /** @brief Static configuration for the selector buttons. */
    static constexpr Button::Config button_configs_[] = {
        {
            .mux_index = 2U,
            .pin = 8U,
            .led_number = 127U,
        },
        {
            .mux_index = 2U,
            .pin = 7U,
            .led_number = 126U,
        },
        {
            .mux_index = 2U,
            .pin = 6U,
            .led_number = 125U,
        },
        {
            .mux_index = 2U,
            .pin = 5U,
            .led_number = 124U,
        },
        {
            .mux_index = 2U,
            .pin = 4U,
            .led_number = 123U,
        },
        {
            .mux_index = 2U,
            .pin = 3U,
            .led_number = 122U,
        },
    };

    /** @brief Static configuration for the MOD knob control. */
    static constexpr Knob::Config knob_configs_[] = {
        {
            .button_mux_index = 2U,
            .button_pin = 0U,
            .encoder_mux_index = 2U,
            .encoder_pin_a = 1U,
            .encoder_pin_b = 2U,
            .first_led = 112U,
            .led_count = knob_led_count_,
        },
    };

    /** @brief Number of selector buttons in this block. */
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
     * @brief Updates the selector-button LEDs to show the currently active bank.
     *
     * @retval 0 All selector LEDs match the selected bank.
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

    /** @brief Selector buttons owned by the block. */
    Button buttons_[button_count_];

    /** @brief Knob control owned by the block. */
    Knob knobs_[knob_count_];

    /** @brief Currently selected knob-value bank. */
    uint8_t selected_bank_ = 0U;

    /** @brief Stored knob values for each selector bank. */
    uint8_t knob_values_[bank_count_][knob_count_] = {};
};

#endif /* SRC_BLOCKS_MOD_H_ */

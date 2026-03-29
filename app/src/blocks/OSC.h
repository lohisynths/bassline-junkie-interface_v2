#ifndef SRC_BLOCKS_OSC_H_
#define SRC_BLOCKS_OSC_H_

#include "Button.h"
#include "InputController.h"
#include "Knob.h"
#include "LEDS.h"
#include "MIDI.h"
#include "PresetSnapshot.h"

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Encapsulates the OSC control surface block.
 *
 * The block owns five knobs and three selector buttons. The selector buttons
 * choose one of three banks, and each bank stores one value for every knob.
 */
class OSC {
public:
    /**
     * @brief Initializes all buttons and knobs in the OSC block.
     *
     * @param inputs Shared input controller used by all block controls.
     * @param leds Shared LED controller used by all block controls.
     * @param midi Optional MIDI transport used for OSC knob Control Change messages.
     *
     * @retval 0 All controls were initialized successfully.
     * @retval negative Error propagated from @ref Button::init or @ref Knob::init.
     */
    int init(InputController &inputs, LEDSController &leds, MIDI *midi = nullptr);

    /**
     * @brief Captures the current durable OSC block state.
     */
    void capture_state(OSCState &state) const;

    /**
     * @brief Applies one durable OSC block state snapshot.
     *
     * @retval 0 The requested state was applied successfully.
     * @retval negative Error propagated from LED or knob updates.
     */
    int apply_state(const OSCState &state);

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

    /**
     * @brief Returns the currently selected bank.
     */
    uint8_t selected_bank() const;

private:
    /** @brief Number of LEDs reserved for each knob segment. */
    static const size_t knob_led_count_ = 10U;

    /** @brief Number of selector banks exposed by the buttons. */
    static const size_t bank_count_ = 3U;

    /** @brief Static configuration for the selector buttons. */
    static constexpr Button::Config button_configs_[] = {
        {
            .mux_index = 3U,
            .pin = 3U,
            .led_number = 110U,
        },
        {
            .mux_index = 3U,
            .pin = 2U,
            .led_number = 109U,
        },
        {
            .mux_index = 3U,
            .pin = 1U,
            .led_number = 108U,
        },
    };

    /** @brief Static configuration for the OSC knob controls. */
    static constexpr Knob::Config knob_configs_[] = {
        {
            .button_mux_index = 1U,
            .button_pin = 12U,
            .encoder_mux_index = 1U,
            .encoder_pin_a = 13U,
            .encoder_pin_b = 14U,
            .first_led = 96U,
            .led_count = knob_led_count_,
        },
        {
            .button_mux_index = 1U,
            .button_pin = 9U,
            .encoder_mux_index = 1U,
            .encoder_pin_a = 10U,
            .encoder_pin_b = 11U,
            .first_led = 78U,
            .led_count = knob_led_count_,
        },
        {
            .button_mux_index = 1U,
            .button_pin = 6U,
            .encoder_mux_index = 1U,
            .encoder_pin_a = 7U,
            .encoder_pin_b = 8U,
            .first_led = 68U,
            .led_count = knob_led_count_,
        },
        {
            .button_mux_index = 1U,
            .button_pin = 3U,
            .encoder_mux_index = 1U,
            .encoder_pin_a = 4U,
            .encoder_pin_b = 5U,
            .first_led = 58U,
            .led_count = knob_led_count_,
        },
        {
            .button_mux_index = 1U,
            .button_pin = 0U,
            .encoder_mux_index = 1U,
            .encoder_pin_a = 1U,
            .encoder_pin_b = 2U,
            .first_led = 48U,
            .led_count = knob_led_count_,
        },
    };

    /** @brief Number of selector buttons in this block. */
    static const size_t button_count_ = ARRAY_SIZE(button_configs_);

    /** @brief Number of knobs in this block. */
    static const size_t knob_count_ = ARRAY_SIZE(knob_configs_);

    /**
     * @brief Selects one knob-value bank and recalls its values onto the knobs.
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

    /**
     * @brief Sends one OSC knob value as a MIDI Control Change message.
     *
     * @param bank_index OSC bank index in the range `[0, bank_count_)`.
     * @param knob_index OSC knob index in the range `[0, knob_count_)`.
     * @param value MIDI controller value in the range `[0, 127]`.
     */
    void send_midi_cc_(size_t bank_index, size_t knob_index, uint8_t value);

    /**
     * @brief Sends the full stored OSC parameter state over MIDI.
     */
    void send_all_midi_cc_();

    /** @brief Selector buttons owned by the block. */
    Button buttons_[button_count_];

    /** @brief Knob controls owned by the block. */
    Knob knobs_[knob_count_];

    /** @brief Currently selected knob-value bank. */
    uint8_t selected_bank_ = 0U;

    /** @brief Stored knob values for each selector bank. */
    uint8_t knob_values_[bank_count_][knob_count_] = {};

    /** @brief Optional MIDI transport used for OSC knob Control Change messages. */
    MIDI *midi_ = nullptr;

    /** @brief Set when one knob button was newly pressed during the latest update. */
    bool has_newly_pressed_knob_ = false;

    /** @brief Stores the first newly pressed knob index from the latest update. */
    uint8_t newly_pressed_knob_index_ = 0U;
};

#endif /* SRC_BLOCKS_OSC_H_ */

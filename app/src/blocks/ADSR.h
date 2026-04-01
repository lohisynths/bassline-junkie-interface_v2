/**
 * @file ADSR.h
 * @brief ADSR envelope control-surface block.
 *
 * Created on: Mar 29, 2026
 *     Author: alax
 */

#ifndef APP_SRC_BLOCKS_ADSR_H_
#define APP_SRC_BLOCKS_ADSR_H_

#include "UI_BLOCK.h"

/** @brief Number of encoder knobs in the ADSR block. */
static constexpr uint8_t ADSR_KNOB_COUNT = 4U;

/** @brief Number of buttons in the ADSR block (3 bank-selectors + 1 LOOP toggle). */
static constexpr uint8_t ADSR_BUTTON_COUNT = 4U;

/** @brief Number of banked instances (selector positions) in the ADSR block. */
static constexpr uint8_t ADSR_COUNT = 3U;

/** @brief MIDI CC offset for the ADSR block. */
static constexpr uint8_t ADSR_MIDI_OFFSET = 18U;

/**
 * @brief Logical parameter indices for one ADSR bank.
 *
 * @c ADSR_LOOP is a special parameter (index `>= ADSR_KNOB_COUNT`) stored
 * in the preset but not directly backed by a knob. It is restored on bank
 * recall via @ref ADSR::force_function.
 */
enum ADSR_PARAMS {
    ADSR_ATTACK,
    ADSR_DECAY,
    ADSR_SUSTAIN,
    ADSR_RELEASE,
    ADSR_LOOP,
    ADSR_PARAM_NR
};

/**
 * @brief ADSR envelope block owning four knobs, three bank-selector buttons,
 *        and one LOOP toggle button.
 *
 * Hardware wiring is declared in the two public constexpr config tables and
 * consumed automatically by @ref UI_BLOCK::init through CRTP.
 *
 * Buttons `[0, ADSR_COUNT)` are bank selectors; button `ADSR_COUNT` (index 3)
 * is the LOOP toggle, routed through @ref select_function. The LOOP state is
 * stored as a special preset parameter at index @c ADSR_LOOP and restored
 * during bank recall via @ref force_function.
 *
 * MIDI CC numbers are laid out as
 * `ADSR_MIDI_OFFSET + instance * ADSR_PARAM_NR + index`, giving CC `18..22`
 * for bank 0, `23..27` for bank 1, and `28..32` for bank 2, all on MIDI
 * channel `0`.
 */
class ADSR : public UI_BLOCK<ADSR, ADSR_KNOB_COUNT, ADSR_BUTTON_COUNT, ADSR_PARAM_NR, ADSR_COUNT> {
public:
    /** @brief LED arc length shared by all four knobs. */
    static constexpr uint8_t knob_led_count_ = 10U;

    /** @brief Mux/LED bindings for the four buttons (3 bank-selectors + 1 LOOP toggle). */
    static constexpr std::array button_configs_ = {
        Button::Config{ .mux_index = 0U, .pin = 15U, .led_number = 43U },
        Button::Config{ .mux_index = 0U, .pin = 14U, .led_number = 42U },
        Button::Config{ .mux_index = 0U, .pin = 13U, .led_number = 41U },
        Button::Config{ .mux_index = 0U, .pin = 12U, .led_number = 40U },
    };

    /** @brief Encoder/button/LED bindings for the four knobs. */
    static constexpr std::array knob_configs_ = {
        Knob::Config{
            .button_mux_index = 0U,
            .button_pin = 9U,
            .encoder_mux_index = 0U,
            .encoder_pin_a = 10U,
            .encoder_pin_b = 11U,
            .first_led = 30U,
            .led_count = knob_led_count_,
        },
        Knob::Config{
            .button_mux_index = 0U,
            .button_pin = 6U,
            .encoder_mux_index = 0U,
            .encoder_pin_a = 7U,
            .encoder_pin_b = 8U,
            .first_led = 20U,
            .led_count = knob_led_count_,
        },
        Knob::Config{
            .button_mux_index = 0U,
            .button_pin = 3U,
            .encoder_mux_index = 0U,
            .encoder_pin_a = 4U,
            .encoder_pin_b = 5U,
            .first_led = 10U,
            .led_count = knob_led_count_,
        },
        Knob::Config{
            .button_mux_index = 0U,
            .button_pin = 0U,
            .encoder_mux_index = 0U,
            .encoder_pin_a = 1U,
            .encoder_pin_b = 2U,
            .first_led = 0U,
            .led_count = knob_led_count_,
        },
    };

    /* ------------------------------------------------------------------ */
    /*  Required CRTP hooks                                               */
    /* ------------------------------------------------------------------ */

    /** @brief Returns the block name used in log output. */
    const char *get_name() { return "ADSR"; }

    /**
     * @brief Maps a bank and parameter index to a MIDI CC number.
     *
     * Layout: CC `18..22` for bank 0, `23..27` for bank 1, `28..32` for bank 2.
     */
    uint8_t get_midi_nr(uint8_t instance, uint8_t index) {
        return static_cast<uint8_t>(ADSR_MIDI_OFFSET + instance * ADSR_PARAM_NR + index);
    }

    /** @brief MIDI channel used for all ADSR control-change messages. */
    uint8_t get_midi_ch() { return 1U; }

    /* ------------------------------------------------------------------ */
    /*  CRTP hook overrides                                               */
    /* ------------------------------------------------------------------ */

    /**
     * @brief Restores the LOOP button LED state during bank recall.
     *
     * Called by @ref UI_BLOCK::select_instance for the @c ADSR_LOOP special
     * parameter whenever a bank switch occurs. Updates the LOOP button LED
     * to match the recalled preset value without emitting MIDI.
     *
     * @param value Recalled LOOP state (`0` = off, non-zero = on).
     */
    void force_function(uint8_t value) {
        set_sw_state(ADSR_COUNT, value != 0U);
    }

    /**
     * @brief Called when the LOOP button (function button index 0) is pressed.
     *
     * Delegates to @ref toggle_loop to invert the current bank's loop state.
     *
     * @param index Function button index (always `0` for this block).
     */
    void select_function(uint8_t /*index*/) {
        toggle_loop();
    }

    /* ------------------------------------------------------------------ */
    /*  ADSR-specific helpers                                             */
    /* ------------------------------------------------------------------ */

    /**
     * @brief Toggles the LOOP state for the current bank, updates the button
     *        LED, stores the new value in the preset, and sends a MIDI CC.
     */
    void toggle_loop() {
        const uint8_t loop_state = get_current_preset_value(ADSR_LOOP) ^ 1U;
        set_sw_state(ADSR_COUNT, loop_state != 0U);
        set_current_preset_value(ADSR_LOOP, loop_state);
        if (get_midi()) {
            get_midi()->send_cc(get_current_instance_midi_nr(ADSR_LOOP),
                                loop_state,
                                get_midi_ch());
        }
    }

    /** @brief Returns the currently selected ADSR bank index. */
    uint8_t get_current_adsr() { return get_current_instance(); }
};

#endif /* APP_SRC_BLOCKS_ADSR_H_ */

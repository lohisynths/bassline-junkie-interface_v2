/**
 * @file LFO.h
 * @brief LFO control-surface block.
 *
 * Created on: Apr 1, 2026
 *     Author: alax
 */

#ifndef APP_SRC_BLOCKS_LFO_H_
#define APP_SRC_BLOCKS_LFO_H_

#include "LED_DISP.h"
#include "UI_BLOCK.h"

/** @brief Number of encoder knobs in the LFO block (frequency). */
static constexpr uint8_t LFO_KNOB_COUNT = 1U;

/** @brief Number of buttons in the LFO block (3 bank-selectors + 4 shape selectors + 1 SYNC toggle). */
static constexpr uint8_t LFO_BUTTON_COUNT = 8U;

/** @brief Number of banked instances (selector positions) in the LFO block. */
static constexpr uint8_t LFO_COUNT = 3U;

/** @brief Number of available LFO waveform shapes. */
static constexpr uint8_t LFO_SHAPE_COUNT = 4U;

/** @brief MIDI CC offset for the LFO block. */
static constexpr uint8_t LFO_MIDI_OFFSET = 37U;

/**
 * @brief Logical parameter indices for one LFO bank.
 *
 * @c LFO_SHAPE and @c LFO_SYNC are special parameters
 * (index `>= LFO_KNOB_COUNT`) stored in the preset but not directly
 * backed by a knob. They are restored on bank recall via
 * @ref LFO::force_function.
 */
enum LFO_PARAMS {
    LFO_FREQ,
    LFO_SHAPE,
    LFO_SYNC,
    LFO_PARAM_COUNT
};

/**
 * @brief Available LFO waveform shapes.
 */
enum LFO_SHAPES {
    LFO_SIN,
    LFO_SAW,
    LFO_TRI,
    LFO_SQR,
};

/**
 * @brief LFO block owning one frequency knob, three bank-selector buttons,
 *        four waveform-shape buttons, and one SYNC toggle button.
 *
 * Hardware wiring is declared in the two public constexpr config tables and
 * consumed automatically by @ref UI_BLOCK::init through CRTP.
 *
 * Buttons `[0, LFO_COUNT)` are bank selectors. The remaining five buttons
 * are function buttons routed through the @ref select_function hook:
 * - Function indices `0..3` select waveform shapes (SIN/SAW/TRI/SQR).
 * - Function index `4` toggles SYNC mode.
 *
 * Both @c LFO_SHAPE and @c LFO_SYNC are special (non-knob) parameters;
 * @ref force_function restores their LED state on every bank recall.
 * Because @ref UI_BLOCK::select_instance calls @ref force_function once per
 * special parameter, the implementation restores the full visual state on
 * each call, making it idempotent regardless of call order.
 *
 * MIDI CC numbers are laid out as
 * `LFO_MIDI_OFFSET + instance * LFO_PARAM_COUNT + index`, giving CC `37..39`
 * for bank 0, `40..42` for bank 1, and `43..45` for bank 2, all on MIDI
 * channel `1`.
 */
class LFO : public UI_BLOCK<LFO, LFO_KNOB_COUNT, LFO_BUTTON_COUNT, LFO_PARAM_COUNT, LFO_COUNT> {
public:
    /** @brief Shorthand for the CRTP base class used by LFO. */
    using ui_block = UI_BLOCK<LFO, LFO_KNOB_COUNT, LFO_BUTTON_COUNT, LFO_PARAM_COUNT, LFO_COUNT>;

    /** @brief Static block name used by the shared CRTP base logging. */
    static constexpr const char *block_name_ = "LFO";

    /** @brief MIDI CC offset for the LFO block. */
    static constexpr uint8_t midi_offset_ = LFO_MIDI_OFFSET;

    /** @brief MIDI channel used for LFO control-change messages. */
    static constexpr uint8_t midi_channel_ = 1U;

    /** @brief LED arc length for the frequency knob. */
    static constexpr uint8_t knob_led_count_ = 10U;

    /** @brief Mux/LED bindings for the eight buttons (3 bank-selectors + 4 shape + 1 SYNC). */
    static constexpr std::array button_configs_ = {
        Button::Config{ .mux_index = 3U, .pin = 0U,  .led_number = 158U },  /* LFO 0 selector  */
        Button::Config{ .mux_index = 2U, .pin = 15U, .led_number = 157U },  /* LFO 1 selector  */
        Button::Config{ .mux_index = 2U, .pin = 14U, .led_number = 156U },  /* LFO 2 selector  */
        Button::Config{ .mux_index = 2U, .pin = 13U, .led_number = 142U },  /* shape: SIN      */
        Button::Config{ .mux_index = 2U, .pin = 12U, .led_number = 141U },  /* shape: SAW      */
        Button::Config{ .mux_index = 2U, .pin = 11U, .led_number = 140U },  /* shape: TRI      */
        Button::Config{ .mux_index = 2U, .pin = 10U, .led_number = 139U },  /* shape: SQR      */
        Button::Config{ .mux_index = 2U, .pin = 9U,  .led_number = 138U },  /* SYNC toggle     */
    };

    /** @brief Encoder/button/LED bindings for the frequency knob. */
    static constexpr std::array knob_configs_ = {
        Knob::Config{
            .button_mux_index  = 4U,
            .button_pin        = 0U,
            .encoder_mux_index = 4U,
            .encoder_pin_a     = 1U,
            .encoder_pin_b     = 2U,
            .first_led         = 128U,
            .led_count         = knob_led_count_,
        },
    };

    /* ------------------------------------------------------------------ */
    /*  CRTP hook overrides                                               */
    /* ------------------------------------------------------------------ */

    /**
     * @brief Restores shape and SYNC button LED state during bank recall.
     *
     * Called by @ref UI_BLOCK::select_instance once for each special
     * parameter (@c LFO_SHAPE then @c LFO_SYNC) on every bank switch.
     * Both calls fully restore the visual state from the current preset,
     * keeping the function idempotent regardless of which parameter
     * triggered the call.
     *
     * @param value Recalled special-parameter value (unused; full state is
     *              read directly from the preset on every invocation).
     */
    void force_function(uint8_t value) {
        (void)value;
        const uint8_t shape = get_current_preset_value(LFO_SHAPE);
        for (uint8_t i = 0U; i < LFO_SHAPE_COUNT; i++) {
            set_sw_state(LFO_COUNT + i, i == shape);
        }

        set_sw_state(LFO_COUNT + LFO_SHAPE_COUNT,
                     get_current_preset_value(LFO_SYNC) != 0U);
    }

    /**
     * @brief Dispatches a function-button press to shape selection or SYNC
     *        toggle.
     *
     * Function button indices (offset from @c LFO_COUNT):
     * - `0` → SIN, `1` → SAW, `2` → TRI, `3` → SQR (waveform shape)
     * - `4` → SYNC toggle
     *
     * @param index Function button index in the range
     *              `[0, LFO_BUTTON_COUNT - LFO_COUNT)`.
     */
    void select_function(uint8_t index) {
        if (index < LFO_SHAPE_COUNT) {
            select_lfo_shape(index);
        } else {
            toggle_sync();
        }
    }

    /* ------------------------------------------------------------------ */
    /*  LFO-specific helpers                                              */
    /* ------------------------------------------------------------------ */

    /**
     * @brief Activates the given waveform shape, updates shape button LEDs,
     *        stores the value in the preset, and sends a MIDI CC.
     *
     * @param shape New shape index in the range `[0, LFO_SHAPE_COUNT)`.
     */
    void select_lfo_shape(uint8_t shape) {
        for (uint8_t i = 0U; i < LFO_SHAPE_COUNT; i++) {
            set_sw_state(LFO_COUNT + i, i == shape);
        }
        set_current_preset_value(LFO_SHAPE, shape);
        if (get_midi()) {
            get_midi()->send_cc(get_current_instance_midi_nr(LFO_SHAPE),
                                shape,
                                get_midi_ch());
        }
    }

    /**
     * @brief Toggles the SYNC state for the current bank, updates the SYNC
     *        button LED, stores the new value in the preset, and sends a
     *        MIDI CC.
     */
    void toggle_sync() {
        const uint8_t sync = get_current_preset_value(LFO_SYNC) ^ 1U;
        set_sw_state(LFO_COUNT + LFO_SHAPE_COUNT, sync != 0U);
        set_current_preset_value(LFO_SYNC, sync);
        if (get_midi()) {
            get_midi()->send_cc(get_current_instance_midi_nr(LFO_SYNC),
                                sync,
                                get_midi_ch());
        }
    }

    /**
     * @brief Returns the currently selected LFO bank index.
     *
     * @return Active LFO bank index.
     */
    uint8_t get_current_lfo() { return get_current_instance(); }

};

#endif /* APP_SRC_BLOCKS_LFO_H_ */

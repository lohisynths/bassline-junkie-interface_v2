/**
 * @file FLT.h
 * @brief Filter control-surface block.
 *
 * Created on: Apr 1, 2026
 *     Author: alax
 */

#ifndef APP_SRC_BLOCKS_FLT_H_
#define APP_SRC_BLOCKS_FLT_H_

#include "UI_BLOCK.h"

/** @brief Number of encoder knobs in the FLT block (frequency, resonance, keyboard tracking). */
static constexpr uint8_t FLT_KNOB_COUNT = 3U;

/** @brief Number of buttons in the FLT block (3 filter-type selectors). */
static constexpr uint8_t FLT_BUTTON_COUNT = 3U;

/** @brief Number of banked instances in the FLT block (single filter, no banking). */
static constexpr uint8_t FLT_COUNT = 1U;

/** @brief Number of available filter types. */
static constexpr uint8_t FLT_TYPE_COUNT = 3U;

/** @brief MIDI CC offset for the FLT block. */
static constexpr uint8_t FLT_MIDI_OFFSET = 33U;

/**
 * @brief Logical parameter indices for the FLT block.
 *
 * @c FLT_TYPE is a special parameter (index `>= FLT_KNOB_COUNT`) stored
 * in the preset but not directly backed by a knob. It is restored on
 * recall via @ref FLT::force_function.
 */
enum FLT_PARAMS {
    FLT_FREQ,
    FLT_RES,
    FLT_KBTRACK,
    FLT_TYPE,
    FLT_PARAM_COUNT
};

/**
 * @brief Available filter types.
 */
enum FLT_TYPES {
    FLT_LP,
    FLT_BP,
    FLT_HP,
};

/**
 * @brief Filter block owning three knobs and three filter-type selector buttons.
 *
 * Hardware wiring is declared in the two public constexpr config tables and
 * consumed automatically by @ref UI_BLOCK::init through CRTP.
 *
 * With @c FLT_COUNT set to `1` the block has a single instance and no
 * bank-selector buttons. All three buttons are routed directly to
 * @ref select_function with their raw indices (`0`, `1`, `2`), selecting
 * LP, BP, or HP filter type respectively.
 *
 * @c FLT_TYPE is a special (non-knob) parameter; @ref force_function
 * restores the filter-type button LED state on every recall, keeping the
 * display consistent when a preset is loaded.
 *
 * MIDI CC numbers are `FLT_MIDI_OFFSET + index`, giving CC `33` (FREQ),
 * `34` (RES), `35` (KBTRACK), and `36` (TYPE), all on MIDI channel `1`.
 */
class FLT : public UI_BLOCK<FLT, FLT_KNOB_COUNT, FLT_BUTTON_COUNT, FLT_PARAM_COUNT, FLT_COUNT> {
public:
    /** @brief LED arc length for the resonance knob. */
    static constexpr uint8_t knob_led_count_ = 10U;

    /** @brief Number of FLT knobs shown in the MOD viewer. */
    static constexpr uint8_t mod_knob_count_ = 2U;

    /** @brief Mux/LED bindings for the three filter-type selector buttons. */
    static constexpr std::array button_configs_ = {
        Button::Config{ .mux_index = 3U, .pin = 15U, .led_number = 174U },  /* LP */
        Button::Config{ .mux_index = 3U, .pin = 14U, .led_number = 173U },  /* BP */
        Button::Config{ .mux_index = 3U, .pin = 13U, .led_number = 172U },  /* HP */
    };

    /** @brief Encoder/button/LED bindings for the three knobs. */
    static constexpr std::array knob_configs_ = {
        Knob::Config{
            .button_mux_index  = 3U,
            .button_pin        = 4U,
            .encoder_mux_index = 3U,
            .encoder_pin_a     = 5U,
            .encoder_pin_b     = 6U,
            .first_led         = 144U,
            .led_count         = 12U,
        },
        Knob::Config{
            .button_mux_index  = 3U,
            .button_pin        = 7U,
            .encoder_mux_index = 3U,
            .encoder_pin_a     = 8U,
            .encoder_pin_b     = 9U,
            .first_led         = 160U,
            .led_count         = knob_led_count_,
        },
        Knob::Config{
            .button_mux_index  = 3U,
            .button_pin        = 10U,
            .encoder_mux_index = 3U,
            .encoder_pin_a     = 11U,
            .encoder_pin_b     = 12U,
            .first_led         = 170U,
            .led_count         = 0U,
        },
    };

    /* ------------------------------------------------------------------ */
    /*  Required CRTP hooks                                               */
    /* ------------------------------------------------------------------ */

    /** @brief Returns the block name used in log output. */
    const char *get_name() { return "FLT"; }

    /**
     * @brief Maps a parameter index to a MIDI CC number.
     *
     * Layout: CC `33` (FREQ), `34` (RES), `35` (KBTRACK), `36` (TYPE).
     * The @p instance argument is unused since the FLT block is not banked.
     */
    uint8_t get_midi_nr(uint8_t /*instance*/, uint8_t index) {
        return static_cast<uint8_t>(FLT_MIDI_OFFSET + index);
    }

    /** @brief MIDI channel used for all FLT control-change messages. */
    uint8_t get_midi_ch() { return 1U; }

    /* ------------------------------------------------------------------ */
    /*  CRTP hook overrides                                               */
    /* ------------------------------------------------------------------ */

    /**
     * @brief Restores filter-type button LED state during recall.
     *
     * Called by @ref UI_BLOCK::select_instance for the @c FLT_TYPE special
     * parameter whenever a preset load or viewer-mode toggle occurs.
     * Lights only the button matching the stored type and extinguishes the
     * other two.
     *
     * @param value Recalled filter-type value (@c FLT_LP, @c FLT_BP, or
     *              @c FLT_HP).
     */
    void force_function(uint8_t value) {
        for (uint8_t i = 0U; i < FLT_TYPE_COUNT; i++) {
            set_sw_state(i, i == value);
        }
    }

    /**
     * @brief Selects a filter type when any of the three type buttons is
     *        pressed.
     *
     * Because @c FLT_COUNT `== 1`, @ref UI_BLOCK routes all button presses
     * directly here with the raw button index as @p index, so no offset is
     * needed.
     *
     * @param index Button index in the range `[0, FLT_TYPE_COUNT)`:
     *              `0` = LP, `1` = BP, `2` = HP.
     */
    void select_function(uint8_t index) {
        select_filter_type(index);
    }

    /* ------------------------------------------------------------------ */
    /*  FLT-specific helpers                                              */
    /* ------------------------------------------------------------------ */

    /**
     * @brief Activates the given filter type, updates type button LEDs,
     *        stores the value in the preset, and sends a MIDI CC.
     *
     * @param type New filter type in the range `[0, FLT_TYPE_COUNT)`.
     */
    void select_filter_type(uint8_t type) {
        for (uint8_t i = 0U; i < FLT_TYPE_COUNT; i++) {
            set_sw_state(i, i == type);
        }
        set_current_preset_value(FLT_TYPE, type);
        if (get_midi()) {
            get_midi()->send_cc(get_current_instance_midi_nr(FLT_TYPE),
                                type,
                                get_midi_ch());
        }
    }

    /** @brief Returns the currently active filter type index. */
    uint8_t get_current_filter_type() { return get_current_preset_value(FLT_TYPE); }

    /**
     * @brief Shows one MOD routing row on the FREQ and RES FLT knob LEDs.
     *
     * Only the FREQ and RES knob LEDs are previewed; the KBTRACK knob and
     * filter-type selector buttons stay on the normal FLT preset state.
     */
    void show_mod_view(uint8_t source) {
        auto &knobs = get_knobs();

        for (uint8_t i = 0U; i < mod_knob_count_; i++) {
            knobs[i].show_preview_value(get_preset_mod_value(source, i));
        }
    }

    /**
     * @brief Restores the FLT knob LEDs to the block's real preset values.
     */
    void clear_mod_view() {
        reset();
    }
};

#endif /* APP_SRC_BLOCKS_FLT_H_ */

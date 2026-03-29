#ifndef SRC_BLOCKS_LED_DISP_H_
#define SRC_BLOCKS_LED_DISP_H_

#include "InputController.h"
#include "Knob.h"
#include "LEDS.h"
#include "PresetSnapshot.h"

#include <stddef.h>
#include <stdint.h>

class ADSR;
class FLT;
class LFO;
class MOD;
class OSC;
class PresetStore;

/**
 * @brief Encapsulates the 3-digit LED display block as a preset selector UI.
 *
 * The block owns one knob with no local LED ring and interprets its `0..127`
 * value as a flash-backed preset index. The 3-digit display always shows the
 * selected preset number with blank leading digits. The display outputs are
 * active-low, so a `0%` PWM duty lights a segment and `100%` turns it off.
 */
class LED_DISP {
public:
    /**
     * @brief Initializes the preset selector and restores the last active preset.
     *
     * @param inputs Shared input controller used by the knob.
     * @param leds Shared LED controller used by the display.
     * @param preset_store Flash-backed preset store used for save/load actions.
     * @param adsr ADSR block whose durable state participates in presets.
     * @param flt FLT block whose durable state participates in presets.
     * @param lfo LFO block whose durable state participates in presets.
     * @param mod MOD block whose durable state participates in presets.
     * @param osc OSC block whose durable state participates in presets.
     *
     * @retval 0 The selector was initialized and the startup preset was applied.
     * @retval negative Error propagated from @ref Knob::init or display updates.
     */
    int init(InputController &inputs,
             LEDSController &leds,
             PresetStore &preset_store,
             ADSR &adsr,
             FLT &flt,
             LFO &lfo,
             MOD &mod,
             OSC &osc);

    /**
     * @brief Updates the preset selector, handles save/load gestures, and refreshes the display.
     *
     * @retval 0 The block state was updated successfully.
     * @retval negative Error propagated from @ref Knob::update or display writes.
     */
    int update();

private:
    /** @brief Tags the temporary blink action currently in progress. */
    enum class BlinkAction : uint8_t {
        none = 0U,
        save_feedback,
        restore_active_preset,
    };

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

    /** @brief Hold time required to save one preset instead of loading it. */
    static const int64_t save_hold_ms_ = 1000;

    /** @brief Idle time after preset browsing before the display snaps back. */
    static const int64_t browse_timeout_ms_ = 5000;

    /** @brief Duration of the blanking blink before the active preset is restored. */
    static const int64_t revert_blink_ms_ = 200;

    /** @brief Duration of the save confirmation blink after a hold-triggered save. */
    static const int64_t save_feedback_blink_ms_ = 200;

    /** @brief Static knob binding for the display encoder and push button. */
    static constexpr Knob::Config knob_config_ = {
        .button_mux_index = 4U,
        .button_pin = 3U,
        .encoder_mux_index = 4U,
        .encoder_pin_a = 4U,
        .encoder_pin_b = 5U,
        .first_led = 0U,
        .led_count = 0U,
        .encoder_step_divider = 4U,
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
     * @brief Blanks all three display digits.
     *
     * @retval 0 The display was blanked successfully.
     * @retval negative Error propagated from @ref set_digit_.
     */
    int render_blank_();

    /**
     * @brief Renders the current preset number onto the 3-digit display.
     *
     * Leading zero digits are blanked while the ones digit is always shown.
     *
     * @retval 0 The display was updated successfully.
     * @retval negative Error propagated from @ref set_digit_.
     */
    int render_value_();

    /**
     * @brief Captures one full-surface preset snapshot from the current blocks.
     */
    void capture_snapshot_(PresetSnapshot &snapshot) const;

    /**
     * @brief Applies one full-surface preset snapshot to all blocks.
     *
     * @retval 0 The snapshot was applied successfully.
     * @retval negative Error propagated from block state application.
     */
    int apply_snapshot_(const PresetSnapshot &snapshot);

    /**
     * @brief Loads and applies the preset selected on the encoder.
     *
     * @retval 0 The selected preset or default state was applied successfully.
     * @retval negative Error propagated from @ref PresetStore::load_preset or block updates.
     */
    int load_selected_preset_();

    /**
     * @brief Saves the current full-surface state into the selected preset slot.
     *
     * @retval 0 The selected preset was saved successfully.
     * @retval negative Error propagated from @ref PresetStore::save_preset.
     */
    int save_selected_preset_();

    /**
     * @brief Starts or clears the browse timeout depending on the selected preset.
     */
    void refresh_browse_deadline_(int64_t now_ms);

    /**
     * @brief Cancels any active temporary display blink.
     */
    void cancel_blink_();

    /**
     * @brief Starts one temporary display blink for the requested action.
     *
     * @retval 0 The display was blanked and the blink action was armed.
     * @retval negative Error propagated from @ref render_blank_.
     */
    int start_blink_(BlinkAction action, int64_t now_ms);

    /**
     * @brief Finishes the current temporary blink once its timeout expires.
     *
     * @retval 0 The pending blink action was completed successfully.
     * @retval negative Error propagated from display writes or preset selection restore.
     */
    int finish_blink_();

    /** @brief Display knob owned by the block. */
    Knob knob_;

    /** @brief Shared LED controller borrowed from the caller. */
    LEDSController *leds_ = nullptr;

    /** @brief Shared preset store borrowed from the caller. */
    PresetStore *preset_store_ = nullptr;

    /** @brief ADSR block borrowed from the caller. */
    ADSR *adsr_ = nullptr;

    /** @brief FLT block borrowed from the caller. */
    FLT *flt_ = nullptr;

    /** @brief LFO block borrowed from the caller. */
    LFO *lfo_ = nullptr;

    /** @brief MOD block borrowed from the caller. */
    MOD *mod_ = nullptr;

    /** @brief OSC block borrowed from the caller. */
    OSC *osc_ = nullptr;

    /** @brief Timestamp captured when the preset button was pressed. */
    int64_t press_started_at_ms_ = 0;

    /** @brief Preset currently considered active after the last load or save. */
    uint8_t active_preset_ = 0U;

    /** @brief Deadline when browsing should snap back to the active preset. */
    int64_t browse_deadline_at_ms_ = 0;

    /** @brief Tracks whether the current button hold already triggered a save. */
    bool save_triggered_during_hold_ = false;

    /** @brief Current temporary blink action, if any. */
    BlinkAction blink_action_ = BlinkAction::none;

    /** @brief Timestamp when the current temporary blink started. */
    int64_t blink_started_at_ms_ = 0;
};

#endif /* SRC_BLOCKS_LED_DISP_H_ */

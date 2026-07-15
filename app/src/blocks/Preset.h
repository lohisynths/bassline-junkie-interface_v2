#ifndef SRC_PRESET_H_
#define SRC_PRESET_H_

#include "EEPROM.h"
#include "blocks/UI_BLOCK.h"

#include <cstdint>

class ADSR;
class FLT;
class LFO;
class MOD;
class OSC;
class LED_DISP;
class VOL;

/** @brief Number of encoder knobs in the preset block. */
static constexpr uint8_t PRESET_KNOB_COUNT = 1U;

/** @brief Number of buttons in the preset block. */
static constexpr uint8_t PRESET_BUTTON_COUNT = 0U;

/** @brief Number of banked instances in the preset block. */
static constexpr uint8_t PRESET_COUNT = 1U;

/** @brief Number of logical parameters exposed by the preset block. */
static constexpr uint8_t PRESET_PARAM_COUNT = 1U;

/**
 * @brief High-level preset controller owning the preset encoder UI block.
 */
class Preset : public UI_BLOCK<Preset, PRESET_KNOB_COUNT, PRESET_BUTTON_COUNT, PRESET_PARAM_COUNT, PRESET_COUNT> {
public:
    /** @brief Shorthand for the CRTP base class used by Preset. */
    using ui_block = UI_BLOCK<Preset, PRESET_KNOB_COUNT, PRESET_BUTTON_COUNT, PRESET_PARAM_COUNT, PRESET_COUNT>;

    /** @brief Total preset slot count. */
    static constexpr uint8_t preset_count = EEPROM::preset_count;

    /** @brief Single logical parameter stored by the preset encoder block. */
    static constexpr uint8_t preset_value_param_ = 0U;

    /** @brief Static block name used by the shared CRTP base logging. */
    static constexpr const char *block_name_ = "PRESET";

    /** @brief Legacy MIDI offset retained to satisfy the shared UI_BLOCK metadata contract. */
    static constexpr uint8_t midi_offset_ = 1U;

    /** @brief Legacy MIDI channel retained to satisfy the shared UI_BLOCK metadata contract. */
    static constexpr uint8_t midi_channel_ = 1U;

    /** @brief Preset encoder has no buttons. */
    static constexpr std::array<Button::Config, PRESET_BUTTON_COUNT> button_configs_ = {};

    /** @brief Encoder/button binding for preset browsing and save/load actions. */
    static constexpr std::array<Knob::Config, PRESET_KNOB_COUNT> knob_configs_ = {
        Knob::Config{
            .button_mux_index = 4U,
            .button_pin = 3U,
            .encoder_mux_index = 4U,
            .encoder_pin_a = 4U,
            .encoder_pin_b = 5U,
            .first_led = 0U,
            .led_count = 0U,
            .encoder_step_divider = 4U,
            .pressed_step_multiplier = 1U,
            .ignore_rotation_while_pressed = true,
        },
    };

    /** @brief Hold time required to save the current preset. */
    static constexpr uint32_t save_hold_ms = 1000U;

    /** @brief Timeout before the display snaps back to the active slot. */
    static constexpr uint32_t browse_timeout_ms = 5000U;

    /** @brief Brief display blink duration used for save and restore feedback. */
    static constexpr uint32_t blink_ms = 120U;

    /** @brief Constructs an uninitialized preset controller. */
    Preset() = default;

    /**
     * @brief Binds the preset controller to the storage backend and UI blocks.
     *
     * @param eeprom EEPROM backend used for persistence.
     * @param midi MIDI backend used to initialize the shared UI block base.
     * @param leds LED controller passed through to the shared UI block base.
     * @param inputs Input controller used to initialize the preset encoder.
     * @param display LED display wrapper used for browsing and feedback.
     * @param adsr ADSR block whose preset state is stored and restored.
     * @param flt FLT block whose preset state is stored and restored.
     * @param lfo LFO block whose preset state is stored and restored.
     * @param mod MOD block whose routing state is stored and restored.
     * @param osc OSC block whose preset state is stored and restored.
     * @param vol VOL block whose preset state is stored and restored.
     * @retval 0 The controller is ready.
     * @retval negative Error propagated from the EEPROM backend.
     */
    int init(EEPROM &eeprom,
             MIDI &midi,
             LEDSController &leds,
             InputController &inputs,
             LED_DISP &display,
             ADSR &adsr,
             FLT &flt,
             LFO &lfo,
             MOD &mod,
             OSC &osc,
             VOL &vol);

    /**
     * @brief Advances the preset save/load state machine.
     */
    void update();

    /**
     * @brief Stores the browsed slot and mirrors it to the LED display.
     *
     * @param index Unused knob index placeholder.
     * @param value_scaled New browsed preset slot.
     */
    void knob_val_changed(uint8_t index, uint8_t value_scaled);

    /**
     * @brief Returns the currently active preset slot.
     *
     * @return Active preset slot number.
     */
    uint8_t get_active_slot() const;

private:
    /**
     * @brief Returns true when all borrowed blocks have been bound.
     *
     * @retval true All required collaborators are available.
     * @retval false One or more collaborators have not been bound yet.
     */
    bool is_ready_() const;

    /**
     * @brief Loads one preset slot from storage and applies it to every bound block.
     *
     * @param slot Preset slot to restore.
     * @retval true The slot was loaded and applied successfully.
     * @retval false The controller is not initialized or the load failed.
     */
    bool load_slot_(uint8_t slot);

    /**
     * @brief Captures the current block state and writes it to one preset slot.
     *
     * @param slot Preset slot to save.
     * @retval true The slot was captured and stored successfully.
     * @retval false The controller is not initialized or the save failed.
     */
    bool save_slot_(uint8_t slot);

    /**
     * @brief Copies the live block state into a preset snapshot structure.
     *
     * @param snapshot Destination snapshot that receives the current state.
     */
    void capture_snapshot_(PresetSnapshot &snapshot) const;

    /**
     * @brief Applies a stored preset snapshot to every bound block.
     *
     * @param snapshot Snapshot to restore.
     */
    void apply_snapshot_(const PresetSnapshot &snapshot);

    /**
     * @brief Returns true when a loaded snapshot matches the current block schema.
     *
     * This guards against stale flash records from older firmware layouts
     * before they are applied to live UI blocks.
     *
     * @param snapshot Snapshot loaded from storage.
     * @retval true The snapshot is compatible with the current parameter layout.
     * @retval false One or more special parameters are outside their valid range.
     */
    bool snapshot_is_compatible_(const PresetSnapshot &snapshot) const;

    /**
     * @brief Sends the snapshot modulation matrix to the MIDI backend.
     *
     * @param snapshot Snapshot containing the modulation routing values.
     */
    void send_mod_matrix_(const PresetSnapshot &snapshot);

    /**
     * @brief Starts the temporary display blink before restoring one slot number.
     *
     * @param restore_slot Slot number to show again after the blink expires.
     */
    void start_blink_(uint8_t restore_slot);

    /**
     * @brief Restores the display after a blink timeout has elapsed.
     */
    void update_display_restore_();

    /**
     * @brief Synchronizes the browsed slot across the encoder preset and display.
     *
     * @param slot Slot number to show and resume browsing from.
     */
    void sync_preset_value_(uint8_t slot);

    /** @brief Borrowed EEPROM backend used for preset persistence. */
    EEPROM *eeprom_ = nullptr;

    /** @brief Borrowed LED display block used for preset browsing feedback. */
    LED_DISP *display_ = nullptr;

    /** @brief Borrowed ADSR block restored from preset snapshots. */
    ADSR *adsr_ = nullptr;

    /** @brief Borrowed filter block restored from preset snapshots. */
    FLT *flt_ = nullptr;

    /** @brief Borrowed LFO block restored from preset snapshots. */
    LFO *lfo_ = nullptr;

    /** @brief Borrowed modulation block used to translate matrix values to MIDI. */
    MOD *mod_ = nullptr;

    /** @brief Borrowed oscillator block restored from preset snapshots. */
    OSC *osc_ = nullptr;

    /** @brief Borrowed volume block restored from preset snapshots. */
    VOL *vol_ = nullptr;

    /** @brief Slot number of the currently active preset. */
    uint8_t active_slot_ = 0U;

    /** @brief Slot number currently shown on the display while browsing. */
    uint8_t displayed_slot_ = 0U;

    /** @brief Tracks whether the display encoder switch is currently held. */
    bool display_pressed_ = false;

    /** @brief Tracks whether the current long-press save action has already fired. */
    bool save_fired_ = false;

    /** @brief Tracks whether the most recent save action succeeded. */
    bool save_succeeded_ = false;

    /** @brief Slot that was saved during the current long-press action. */
    uint8_t saved_slot_ = 0U;

    /** @brief Timestamp when the display encoder switch was pressed. */
    uint32_t display_pressed_at_ms_ = 0U;

    /** @brief Timestamp when browsing away from the active slot began. */
    uint32_t browse_timeout_started_at_ms_ = 0U;

    /** @brief Tracks whether the display is temporarily blanked for feedback. */
    bool blink_active_ = false;

    /** @brief Slot to restore after the blink finishes. */
    uint8_t blink_restore_slot_ = 0U;

    /** @brief Timestamp when the blink feedback should end. */
    uint32_t blink_ends_at_ms_ = 0U;
};

#endif /* SRC_PRESET_H_ */

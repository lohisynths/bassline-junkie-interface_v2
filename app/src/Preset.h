#ifndef SRC_PRESET_H_
#define SRC_PRESET_H_

#include "EEPROM.h"

#include <cstdint>

class ADSR;
class FLT;
class LFO;
class MOD;
class OSC;
class LED_DISP;

/**
 * @brief High-level preset controller for the LED_DISP encoder.
 */
class Preset {
public:
    /** @brief Total preset slot count. */
    static constexpr uint8_t preset_count = EEPROM::preset_count;

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
     * @retval 0 The controller is ready.
     * @retval negative Error propagated from the EEPROM backend.
     */
    int init(EEPROM &eeprom, LED_DISP &display, ADSR &adsr, FLT &flt, LFO &lfo, MOD &mod, OSC &osc);

    /**
     * @brief Advances the preset save/load state machine.
     */
    void update();

    /** @brief Returns the currently active preset slot. */
    uint8_t get_active_slot() const;

private:
    bool load_slot_(uint8_t slot);
    bool save_slot_(uint8_t slot);
    void capture_snapshot_(PresetSnapshot &snapshot) const;
    void apply_snapshot_(const PresetSnapshot &snapshot);
    void send_mod_matrix_(const PresetSnapshot &snapshot);
    void start_blink_(uint8_t restore_slot);
    void update_display_restore_();

    EEPROM *eeprom_ = nullptr;
    LED_DISP *display_ = nullptr;
    ADSR *adsr_ = nullptr;
    FLT *flt_ = nullptr;
    LFO *lfo_ = nullptr;
    MOD *mod_ = nullptr;
    OSC *osc_ = nullptr;

    uint8_t active_slot_ = 0U;
    uint8_t displayed_slot_ = 0U;
    bool display_pressed_ = false;
    bool save_fired_ = false;
    bool save_succeeded_ = false;
    uint8_t saved_slot_ = 0U;
    uint32_t display_pressed_at_ms_ = 0U;
    uint32_t browse_timeout_started_at_ms_ = 0U;
    bool blink_active_ = false;
    uint8_t blink_restore_slot_ = 0U;
    uint32_t blink_ends_at_ms_ = 0U;
};

#endif /* SRC_PRESET_H_ */

#include "Preset.h"

#include "blocks/ADSR.h"
#include "blocks/FLT.h"
#include "blocks/LED_DISP.h"
#include "blocks/LFO.h"
#include "blocks/MOD.h"
#include "blocks/OSC.h"
#include "blocks/VOL.h"

#include <errno.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(Preset, LOG_LEVEL_INF);

namespace {

constexpr uint8_t mod_source_count = 6U;
constexpr uint8_t osc_mod_dest_count = static_cast<uint8_t>(OSC_PARAM_COUNT * OSC_COUNT);
constexpr uint8_t mod_dest_count = static_cast<uint8_t>(osc_mod_dest_count + MOD_FLT_DEST_COUNT);

bool adsr_snapshot_valid(const ADSR::preset &preset)
{
    for (uint8_t instance = 0U; instance < ADSR_COUNT; ++instance) {
        if (preset[instance][ADSR_LOOP] > 1U) {
            return false;
        }
    }

    return true;
}

bool flt_snapshot_valid(const FLT::preset &preset)
{
    for (uint8_t instance = 0U; instance < FLT_COUNT; ++instance) {
        if (preset[instance][FLT_TYPE] >= FLT_TYPE_COUNT) {
            return false;
        }
    }

    return true;
}

bool lfo_snapshot_valid(const LFO::preset &preset)
{
    for (uint8_t instance = 0U; instance < LFO_COUNT; ++instance) {
        if ((preset[instance][LFO_SHAPE] >= LFO_SHAPE_COUNT) ||
            (preset[instance][LFO_SYNC] > 1U)) {
            return false;
        }
    }

    return true;
}

} // namespace

int Preset::init(EEPROM &eeprom,
                 LED_DISP &display,
                 ADSR &adsr,
                 FLT &flt,
                 LFO &lfo,
                 MOD &mod,
                 OSC &osc,
                 VOL &vol)
{
    eeprom_ = &eeprom;
    display_ = &display;
    adsr_ = &adsr;
    flt_ = &flt;
    lfo_ = &lfo;
    mod_ = &mod;
    osc_ = &osc;
    vol_ = &vol;

    const int ret = eeprom_->init();
    if (ret < 0) {
        LOG_ERR("EEPROM init failed, continuing with RAM-only presets: %d", ret);
    }

    uint8_t startup_slot = 0U;
    (void)eeprom_->load_startup_slot(startup_slot);
    (void)load_slot_(startup_slot);
    return 0;
}

void Preset::update()
{
    if (display_ == nullptr) {
        return;
    }

    update_display_restore_();

    const uint8_t current_display = display_->get_actual_preset_nr();
    const bool pressed = display_->get_knobs()[0].get_state();
    const uint32_t now = k_uptime_get_32();

    if (pressed) {
        if (!display_pressed_) {
            display_pressed_ = true;
            display_pressed_at_ms_ = now;
            save_fired_ = false;
        }

        if (!save_fired_ && ((now - display_pressed_at_ms_) >= save_hold_ms)) {
            saved_slot_ = current_display;
            save_fired_ = true;
            save_succeeded_ = save_slot_(saved_slot_);
        }
    } else if (display_pressed_) {
        const bool short_press = !save_fired_ && ((now - display_pressed_at_ms_) < save_hold_ms);
        const bool completed_save = save_fired_ && save_succeeded_;

        display_pressed_ = false;
        display_pressed_at_ms_ = 0U;

        if (short_press) {
            (void)load_slot_(current_display);
        } else if (completed_save) {
            active_slot_ = saved_slot_;
            displayed_slot_ = saved_slot_;
        }

        save_fired_ = false;
        save_succeeded_ = false;
        saved_slot_ = current_display;
        browse_timeout_started_at_ms_ = 0U;
        return;
    }

    if (blink_active_) {
        return;
    }

    if (current_display != displayed_slot_) {
        displayed_slot_ = current_display;

        if (current_display == active_slot_) {
            browse_timeout_started_at_ms_ = 0U;
        } else {
            browse_timeout_started_at_ms_ = now;
        }
    }

    if ((browse_timeout_started_at_ms_ != 0U) &&
        (current_display != active_slot_) &&
        ((now - browse_timeout_started_at_ms_) >= browse_timeout_ms)) {
        start_blink_(active_slot_);
    }
}

uint8_t Preset::get_active_slot() const
{
    return active_slot_;
}

bool Preset::is_ready_() const
{
    return (eeprom_ != nullptr) && (display_ != nullptr) && (adsr_ != nullptr) && (flt_ != nullptr) &&
           (lfo_ != nullptr) && (mod_ != nullptr) && (osc_ != nullptr) && (vol_ != nullptr);
}

bool Preset::load_slot_(uint8_t slot)
{
    if (!is_ready_()) {
        return false;
    }

    PresetSnapshot snapshot = default_preset_snapshot();
    const int ret = eeprom_->load(slot, snapshot);
    if (ret < 0) {
        LOG_ERR("Failed to load preset %u: %d", static_cast<unsigned int>(slot), ret);
        return false;
    }

    if (!snapshot_is_compatible_(snapshot)) {
        LOG_WRN("Preset %u is incompatible with the current layout, loading defaults",
                static_cast<unsigned int>(slot));
        snapshot = default_preset_snapshot();
    }

    apply_snapshot_(snapshot);
    active_slot_ = slot;
    displayed_slot_ = slot;
    display_->sync_preset_value(slot);
    const int startup_ret = eeprom_->save_startup_slot(slot);
    if (startup_ret < 0) {
        LOG_WRN("Failed to remember startup preset %u: %d",
                static_cast<unsigned int>(slot), startup_ret);
    }
    browse_timeout_started_at_ms_ = 0U;
    LOG_INF("Loaded preset %u", static_cast<unsigned int>(slot));
    return true;
}

bool Preset::save_slot_(uint8_t slot)
{
    if (!is_ready_()) {
        return false;
    }

    PresetSnapshot snapshot = default_preset_snapshot();
    capture_snapshot_(snapshot);

    const int ret = eeprom_->save(slot, snapshot);
    if (ret < 0) {
        LOG_ERR("Failed to save preset %u: %d", static_cast<unsigned int>(slot), ret);
        return false;
    }

    active_slot_ = slot;
    displayed_slot_ = slot;
    const int startup_ret = eeprom_->save_startup_slot(slot);
    if (startup_ret < 0) {
        LOG_WRN("Failed to remember startup preset %u after save: %d",
                static_cast<unsigned int>(slot), startup_ret);
    }
    start_blink_(slot);
    LOG_INF("Saved preset %u", static_cast<unsigned int>(slot));
    return true;
}

void Preset::capture_snapshot_(PresetSnapshot &snapshot) const
{
    snapshot.adsr = adsr_->get_preset();
    snapshot.flt = flt_->get_preset();
    snapshot.lfo = lfo_->get_preset();
    snapshot.osc = osc_->get_preset();
    snapshot.vol = vol_->get_preset();
    snapshot.osc_mod = osc_->get_mod_preset();
    snapshot.flt_mod = flt_->get_mod_preset();
}

void Preset::apply_snapshot_(const PresetSnapshot &snapshot)
{
    adsr_->set_preset(snapshot.adsr);
    flt_->set_preset(snapshot.flt);
    lfo_->set_preset(snapshot.lfo);
    osc_->set_preset(snapshot.osc);
    vol_->set_preset(snapshot.vol);
    osc_->get_mod_preset() = snapshot.osc_mod;
    flt_->get_mod_preset() = snapshot.flt_mod;
    send_mod_matrix_(snapshot);
}

bool Preset::snapshot_is_compatible_(const PresetSnapshot &snapshot) const
{
    return adsr_snapshot_valid(snapshot.adsr) &&
           flt_snapshot_valid(snapshot.flt) &&
           lfo_snapshot_valid(snapshot.lfo);
}

void Preset::send_mod_matrix_(const PresetSnapshot &snapshot)
{
    if ((mod_ == nullptr) || (osc_ == nullptr) || (flt_ == nullptr) || (osc_->get_midi() == nullptr)) {
        return;
    }

    for (uint8_t src = 0U; src < mod_source_count; ++src) {
        for (uint8_t dst = 0U; dst < mod_dest_count; ++dst) {
            const bool is_osc_dest = dst < osc_mod_dest_count;
            const uint8_t value = is_osc_dest
                ? snapshot.osc_mod[src][dst]
                : snapshot.flt_mod[src][static_cast<size_t>(dst - osc_mod_dest_count)];

            osc_->get_midi()->send_cc(mod_->get_midi_nr(src, dst),
                                      value,
                                      mod_->get_midi_ch());
        }
    }
}

void Preset::start_blink_(uint8_t restore_slot)
{
    if (display_ == nullptr) {
        return;
    }

    blink_active_ = true;
    blink_restore_slot_ = restore_slot;
    blink_ends_at_ms_ = k_uptime_get_32() + blink_ms;
    display_->set_all(0U);
}

void Preset::update_display_restore_()
{
    if (!blink_active_ || (display_ == nullptr)) {
        return;
    }

    const uint32_t now = k_uptime_get_32();
    if ((int32_t)(now - blink_ends_at_ms_) < 0) {
        return;
    }

    display_->sync_preset_value(blink_restore_slot_);
    blink_active_ = false;
    blink_ends_at_ms_ = 0U;
}

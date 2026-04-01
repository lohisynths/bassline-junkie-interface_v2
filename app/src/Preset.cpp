#include "Preset.h"

#include "blocks/ADSR.h"
#include "blocks/FLT.h"
#include "blocks/LED_DISP.h"
#include "blocks/LFO.h"
#include "blocks/MOD.h"
#include "blocks/OSC.h"

#include <errno.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(Preset, LOG_LEVEL_INF);

namespace {

constexpr uint8_t mod_source_count = 6U;
constexpr uint8_t osc_mod_dest_count = static_cast<uint8_t>(OSC_PARAM_COUNT * OSC_COUNT);
constexpr uint8_t mod_dest_count = static_cast<uint8_t>(osc_mod_dest_count + FLT_KNOB_COUNT);

} // namespace

int Preset::init(EEPROM &eeprom, LED_DISP &display, ADSR &adsr, FLT &flt, LFO &lfo, MOD &mod, OSC &osc)
{
    eeprom_ = &eeprom;
    display_ = &display;
    adsr_ = &adsr;
    flt_ = &flt;
    lfo_ = &lfo;
    mod_ = &mod;
    osc_ = &osc;

    const int ret = eeprom_->init();
    if (ret < 0) {
        LOG_ERR("EEPROM init failed, continuing with RAM-only presets: %d", ret);
    }

    (void)load_slot_(0U);
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
    } else {
        if (display_pressed_) {
            const uint32_t held_ms = now - display_pressed_at_ms_;
            display_pressed_ = false;
            display_pressed_at_ms_ = 0U;

            if (!save_fired_) {
                if (held_ms < save_hold_ms) {
                    (void)load_slot_(current_display);
                }
            } else if (save_succeeded_) {
                active_slot_ = saved_slot_;
                displayed_slot_ = saved_slot_;
            }

            save_fired_ = false;
            save_succeeded_ = false;
            saved_slot_ = current_display;
            browse_timeout_started_at_ms_ = 0U;
            return;
        }
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

bool Preset::load_slot_(uint8_t slot)
{
    if ((eeprom_ == nullptr) || (display_ == nullptr) || (adsr_ == nullptr) || (flt_ == nullptr) ||
        (lfo_ == nullptr) || (mod_ == nullptr) || (osc_ == nullptr)) {
        return false;
    }

    PresetSnapshot snapshot = default_preset_snapshot();
    const int ret = eeprom_->load(slot, snapshot);
    if (ret < 0) {
        LOG_ERR("Failed to load preset %u: %d", static_cast<unsigned int>(slot), ret);
        return false;
    }

    apply_snapshot_(snapshot);
    active_slot_ = slot;
    displayed_slot_ = slot;
    display_->set_display_value(slot);
    browse_timeout_started_at_ms_ = 0U;
    LOG_INF("Loaded preset %u", static_cast<unsigned int>(slot));
    return true;
}

bool Preset::save_slot_(uint8_t slot)
{
    if ((eeprom_ == nullptr) || (display_ == nullptr) || (adsr_ == nullptr) || (flt_ == nullptr) ||
        (lfo_ == nullptr) || (mod_ == nullptr) || (osc_ == nullptr)) {
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
    snapshot.osc_mod = osc_->get_mod_preset();
    snapshot.flt_mod = flt_->get_mod_preset();
}

void Preset::apply_snapshot_(const PresetSnapshot &snapshot)
{
    adsr_->set_preset(snapshot.adsr);
    flt_->set_preset(snapshot.flt);
    lfo_->set_preset(snapshot.lfo);
    osc_->set_preset(snapshot.osc);
    osc_->get_mod_preset() = snapshot.osc_mod;
    flt_->get_mod_preset() = snapshot.flt_mod;
    send_mod_matrix_(snapshot);
}

void Preset::send_mod_matrix_(const PresetSnapshot &snapshot)
{
    if ((mod_ == nullptr) || (osc_ == nullptr) || (flt_ == nullptr) || (osc_->get_midi() == nullptr)) {
        return;
    }

    for (uint8_t src = 0U; src < mod_source_count; ++src) {
        for (uint8_t dst = 0U; dst < mod_dest_count; ++dst) {
            uint8_t value = 0U;
            if (dst < (OSC_PARAM_COUNT * OSC_COUNT)) {
                value = snapshot.osc_mod[src][dst];
            } else {
                value = snapshot.flt_mod[src][static_cast<size_t>(dst - (OSC_PARAM_COUNT * OSC_COUNT))];
            }

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
    display_->set_all(100U);
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

    display_->set_display_value(blink_restore_slot_);
    blink_active_ = false;
    blink_ends_at_ms_ = 0U;
}

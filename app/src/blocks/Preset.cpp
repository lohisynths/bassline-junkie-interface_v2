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

int Preset::init(PresetStorage &storage,
                 MIDI &midi,
                 LEDSController &leds,
                 MUX &inputs,
                 LED_DISP &display,
                 ADSR &adsr,
                 FLT &flt,
                 LFO &lfo,
                 MOD &mod,
                 OSC &osc,
                 VOL &vol)
{
    ui_block::init(midi, leds, inputs);
    storage_ = &storage;
    display_ = &display;
    adsr_ = &adsr;
    flt_ = &flt;
    lfo_ = &lfo;
    mod_ = &mod;
    osc_ = &osc;
    vol_ = &vol;

    uint8_t startup_slot = 0U;
    const PresetLoadResult startup_result = storage_->load_startup_slot(startup_slot);
    if (startup_result == PresetLoadResult::io_error) {
        LOG_ERR("Failed to read startup preset metadata");
        return -EIO;
    }
    const bool startup_incompatible = startup_result == PresetLoadResult::incompatible;
    if (startup_incompatible) {
        LOG_WRN("Startup preset metadata is incompatible, using slot 0");
        startup_slot = 0U;
    }
    if (!load_slot_(startup_slot)) return -EIO;
    if (startup_incompatible) display_->show_error();
    return 0;
}

void Preset::update()
{
    if (display_ == nullptr) {
        return;
    }

    (void)ui_block::update();
    update_display_restore_();

    const uint8_t current_display = get_current_preset_value(preset_value_param_);
    const bool pressed = get_knobs()[0].get_state();
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

void Preset::knob_val_changed(uint8_t index, uint8_t value_scaled)
{
    (void)index;
    sync_preset_value_(value_scaled);
}

uint8_t Preset::get_active_slot() const
{
    return active_slot_;
}

bool Preset::is_ready_() const
{
    return (storage_ != nullptr) && (display_ != nullptr) && (adsr_ != nullptr) && (flt_ != nullptr) &&
           (lfo_ != nullptr) && (mod_ != nullptr) && (osc_ != nullptr) && (vol_ != nullptr);
}

bool Preset::load_slot_(uint8_t slot)
{
    if (!is_ready_()) {
        return false;
    }

    PresetSnapshot snapshot = default_preset_snapshot();
    const PresetLoadResult load_result = storage_->load(slot, snapshot);
    if (load_result == PresetLoadResult::io_error) {
        LOG_ERR("Failed to load preset %u", static_cast<unsigned int>(slot));
        return false;
    }

    const bool missing = load_result == PresetLoadResult::not_found;
    const bool incompatible = (load_result == PresetLoadResult::incompatible) ||
                              !snapshot_is_compatible_(snapshot);
    if (incompatible) {
        LOG_WRN("Preset %u is incompatible with the current layout, loading defaults",
                static_cast<unsigned int>(slot));
        snapshot = default_preset_snapshot();
    } else if (missing) {
        LOG_INF("Preset %u has not been saved, loading default preset",
                static_cast<unsigned int>(slot));
    }

    apply_snapshot_(snapshot);
    active_slot_ = slot;
    displayed_slot_ = slot;
    sync_preset_value_(slot);
    const int startup_ret = storage_->save_startup_slot(slot);
    if (startup_ret < 0) {
        LOG_WRN("Failed to remember startup preset %u: %d",
                static_cast<unsigned int>(slot), startup_ret);
    }
    browse_timeout_started_at_ms_ = 0U;
    if (missing || incompatible) display_->show_error();
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

    const int ret = storage_->save(slot, snapshot);
    if (ret < 0) {
        LOG_ERR("Failed to save preset %u: %d", static_cast<unsigned int>(slot), ret);
        return false;
    }

    active_slot_ = slot;
    displayed_slot_ = slot;
    sync_preset_value_(slot);
    const int startup_ret = storage_->save_startup_slot(slot);
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
    mod_->refresh_from_bound_presets();
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

    sync_preset_value_(blink_restore_slot_);
    blink_active_ = false;
    blink_ends_at_ms_ = 0U;
}

void Preset::sync_preset_value_(uint8_t slot)
{
    set_current_preset_value(preset_value_param_, slot);

    auto &knobs = get_knobs();
    if (!knobs.empty()) {
        (void)knobs[0].set_value(slot);
    }

    if (display_ != nullptr) {
        display_->sync_preset_value(slot);
    }
}

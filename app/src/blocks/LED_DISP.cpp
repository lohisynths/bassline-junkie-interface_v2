#include "LED_DISP.h"

#include "ADSR.h"
#include "FLT.h"
#include "LFO.h"
#include "MOD.h"
#include "OSC.h"
#include "PresetStore.h"

#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(LED_DISP, LOG_LEVEL_INF);

int LED_DISP::init(InputController &inputs,
                   LEDSController &leds,
                   PresetStore &preset_store,
                   ADSR &adsr,
                   FLT &flt,
                   LFO &lfo,
                   MOD &mod,
                   OSC &osc)
{
    leds_ = nullptr;
    preset_store_ = nullptr;
    adsr_ = nullptr;
    flt_ = nullptr;
    lfo_ = nullptr;
    mod_ = nullptr;
    osc_ = nullptr;
    press_started_at_ms_ = 0;
    active_preset_ = 0U;
    browse_started_at_ms_ = 0;
    revert_blink_active_ = false;
    revert_blink_started_at_ms_ = 0;

    int ret = knob_.init(inputs, knob_config_, leds);
    if (ret < 0) {
        return ret;
    }

    leds_ = &leds;
    preset_store_ = &preset_store;
    adsr_ = &adsr;
    flt_ = &flt;
    lfo_ = &lfo;
    mod_ = &mod;
    osc_ = &osc;

    ret = render_value_();
    if (ret < 0) {
        return ret;
    }

    return load_selected_preset_();
}

int LED_DISP::update()
{
    Knob::knob_msg knob_msg;

    int ret = knob_.update(knob_msg);
    if (ret < 0) {
        LOG_ERR("Failed to update LED display knob: %d", ret);
        return ret;
    }

    if (knob_msg.value_changed) {
        if (revert_blink_active_) {
            revert_blink_active_ = false;
            revert_blink_started_at_ms_ = 0;
        }

        refresh_browse_timeout_();

        ret = render_value_();
        if (ret < 0) {
            LOG_ERR("Failed to update preset display value: %d", ret);
            return ret;
        }

        LOG_INF("Selected preset %d", (int)knob_.get_value());
    }

    if (knob_msg.switch_changed) {
        LOG_INF("Preset selector button %s",
                knob_.get_state() ? "pressed" : "released");

        if (knob_.get_state()) {
            if (revert_blink_active_) {
                revert_blink_active_ = false;
                revert_blink_started_at_ms_ = 0;

                ret = render_value_();
                if (ret < 0) {
                    LOG_ERR("Failed to restore preset display during button press: %d", ret);
                    return ret;
                }
            }

            press_started_at_ms_ = k_uptime_get();
            return 0;
        }

        const int64_t held_ms = k_uptime_get() - press_started_at_ms_;
        press_started_at_ms_ = 0;

        if (held_ms >= save_hold_ms_) {
            ret = save_selected_preset_();
            if (ret < 0) {
                LOG_ERR("Failed to save preset %d: %d", (int)knob_.get_value(), ret);
                return ret;
            }
        } else {
            ret = load_selected_preset_();
            if (ret < 0) {
                LOG_ERR("Failed to load preset %d: %d", (int)knob_.get_value(), ret);
                return ret;
            }
        }
    }

    if (revert_blink_active_) {
        if ((k_uptime_get() - revert_blink_started_at_ms_) >= revert_blink_ms_) {
            revert_blink_active_ = false;
            revert_blink_started_at_ms_ = 0;
            browse_started_at_ms_ = 0;

            ret = knob_.set_value(active_preset_);
            if (ret < 0) {
                LOG_ERR("Failed to restore active preset %d: %d", (int)active_preset_, ret);
                return ret;
            }

            ret = render_value_();
            if (ret < 0) {
                LOG_ERR("Failed to redraw restored preset %d: %d", (int)active_preset_, ret);
                return ret;
            }

            LOG_INF("Restored active preset %d after browse timeout", (int)active_preset_);
        }

        return 0;
    }

    if ((browse_started_at_ms_ != 0) &&
        !knob_.get_state() &&
        ((k_uptime_get() - browse_started_at_ms_) >= browse_timeout_ms_)) {
        revert_blink_active_ = true;
        revert_blink_started_at_ms_ = k_uptime_get();

        ret = render_blank_();
        if (ret < 0) {
            LOG_ERR("Failed to blink preset display before restore: %d", ret);
            return ret;
        }
    }

    return 0;
}

int LED_DISP::set_digit_(size_t digit_index,
                         const bool (&pattern)[display_segments_per_digit_])
{
    if (leds_ == nullptr) {
        return -EACCES;
    }

    for (size_t segment_index = 0U; segment_index < display_segments_per_digit_; ++segment_index) {
        const size_t channel =
            display_first_led_ + (digit_index * display_segments_per_digit_) + segment_index;
        const uint8_t brightness =
            pattern[segment_index] ? display_on_percent_ : display_off_percent_;
        const int ret = leds_->set_channel_percent(channel, brightness);
        if (ret < 0) {
            return ret;
        }
    }

    return 0;
}

int LED_DISP::render_value_()
{
    const uint8_t value = knob_.get_value();
    const uint8_t ones = value % 10U;
    const uint8_t tens = (value / 10U) % 10U;
    const uint8_t hundreds = value / 100U;

    int ret = 0;

    if (value >= 100U) {
        ret = set_digit_(0U, digit_patterns_[hundreds]);
    } else {
        ret = set_digit_(0U, blank_digit_pattern_);
    }
    if (ret < 0) {
        return ret;
    }

    if (value >= 10U) {
        ret = set_digit_(1U, digit_patterns_[tens]);
    } else {
        ret = set_digit_(1U, blank_digit_pattern_);
    }
    if (ret < 0) {
        return ret;
    }

    return set_digit_(2U, digit_patterns_[ones]);
}

int LED_DISP::render_blank_()
{
    int ret = set_digit_(0U, blank_digit_pattern_);
    if (ret < 0) {
        return ret;
    }

    ret = set_digit_(1U, blank_digit_pattern_);
    if (ret < 0) {
        return ret;
    }

    return set_digit_(2U, blank_digit_pattern_);
}

void LED_DISP::capture_snapshot_(PresetSnapshot &snapshot) const
{
    snapshot = default_preset_snapshot();

    adsr_->capture_state(snapshot.adsr);
    flt_->capture_state(snapshot.flt);
    lfo_->capture_state(snapshot.lfo);
    mod_->capture_state(snapshot.mod);
    osc_->capture_state(snapshot.osc);
}

int LED_DISP::apply_snapshot_(const PresetSnapshot &snapshot)
{
    int ret = adsr_->apply_state(snapshot.adsr);
    if (ret < 0) {
        return ret;
    }

    ret = flt_->apply_state(snapshot.flt);
    if (ret < 0) {
        return ret;
    }

    ret = lfo_->apply_state(snapshot.lfo);
    if (ret < 0) {
        return ret;
    }

    ret = mod_->apply_state(snapshot.mod);
    if (ret < 0) {
        return ret;
    }

    return osc_->apply_state(snapshot.osc);
}

int LED_DISP::load_selected_preset_()
{
    if ((preset_store_ == nullptr) ||
        (adsr_ == nullptr) ||
        (flt_ == nullptr) ||
        (lfo_ == nullptr) ||
        (mod_ == nullptr) ||
        (osc_ == nullptr)) {
        return -EACCES;
    }

    PresetSnapshot snapshot = default_preset_snapshot();
    bool slot_was_saved = false;

    int ret = preset_store_->load_preset(knob_.get_value(), snapshot, slot_was_saved);
    if (ret < 0) {
        return ret;
    }

    ret = apply_snapshot_(snapshot);
    if (ret < 0) {
        return ret;
    }

    active_preset_ = knob_.get_value();
    browse_started_at_ms_ = 0;
    revert_blink_active_ = false;
    revert_blink_started_at_ms_ = 0;

    LOG_INF("%s preset %d",
            slot_was_saved ? "Loaded" : "Loaded default state for empty",
            (int)knob_.get_value());

    return render_value_();
}

int LED_DISP::save_selected_preset_()
{
    if (preset_store_ == nullptr) {
        return -EACCES;
    }

    PresetSnapshot snapshot = default_preset_snapshot();
    capture_snapshot_(snapshot);

    const int ret = preset_store_->save_preset(knob_.get_value(), snapshot);
    if (ret < 0) {
        return ret;
    }

    active_preset_ = knob_.get_value();
    browse_started_at_ms_ = 0;
    revert_blink_active_ = false;
    revert_blink_started_at_ms_ = 0;

    LOG_INF("Saved preset %d", (int)knob_.get_value());
    return render_value_();
}

void LED_DISP::refresh_browse_timeout_()
{
    if (knob_.get_value() == active_preset_) {
        browse_started_at_ms_ = 0;
        return;
    }

    browse_started_at_ms_ = k_uptime_get();
}

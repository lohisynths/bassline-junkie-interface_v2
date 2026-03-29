#include "MOD.h"

#include <errno.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(MOD, LOG_LEVEL_INF);

namespace {

uint8_t clamp_knob_value_(uint8_t value)
{
    return (value > 127U) ? 127U : value;
}

uint8_t clamp_target_offset_(uint8_t value)
{
    return (value >= MODState::target_count_per_group) ? 0U : value;
}

} // namespace

int MOD::init(InputController &inputs, LEDSController &leds)
{
    int ret = 0;

    for (size_t i = 0U; (i < button_count_) && (ret == 0); ++i) {
        ret = buttons_[i].init(inputs, button_configs_[i], leds);
    }

    for (size_t i = 0U; (i < knob_count_) && (ret == 0); ++i) {
        ret = knobs_[i].init(inputs, knob_configs_[i], leds);
    }

    if (ret == 0) {
        selected_group_ = 0U;
        preview_started_at_ms_ = 0;
        preview_button_index_ = 0U;
        preview_active_ = false;
        preview_button_still_pressed_ = false;
        for (size_t group = 0U; group < selector_group_count_; ++group) {
            selected_target_offset_[group] = 0U;
        }
        ret = select_group_(selected_group_);
    }

    return ret;
}

void MOD::capture_state(MODState &state) const
{
    state = {};

    for (size_t bank = 0U; bank < virtual_bank_count_; ++bank) {
        for (size_t knob = 0U; knob < knob_count_; ++knob) {
            state.knob_values[bank][knob] = knob_values_[bank][knob];
        }
    }

    for (size_t group = 0U; group < selector_group_count_; ++group) {
        state.selected_target_offset[group] = selected_target_offset_[group];
    }
}

int MOD::apply_state(const MODState &state)
{
    preview_started_at_ms_ = 0;
    preview_button_index_ = selected_group_;
    preview_active_ = false;
    preview_button_still_pressed_ = false;

    for (size_t bank = 0U; bank < virtual_bank_count_; ++bank) {
        for (size_t knob = 0U; knob < knob_count_; ++knob) {
            knob_values_[bank][knob] = clamp_knob_value_(state.knob_values[bank][knob]);
        }
    }

    for (size_t group = 0U; group < selector_group_count_; ++group) {
        selected_target_offset_[group] = clamp_target_offset_(state.selected_target_offset[group]);
    }

    int ret = recall_virtual_bank_to_knobs_(current_virtual_bank_index_());
    if (ret < 0) {
        return ret;
    }

    return update_selector_leds_();
}

int MOD::update()
{
    int ret = 0;
    const int64_t now_ms = k_uptime_get();

    for (size_t i = 0U; i < button_count_; ++i) {
        Button::button_msg button_msg;

        ret = buttons_[i].update(button_msg);
        if (ret < 0) {
            LOG_ERR("Failed to update MOD button %u: %d", (unsigned int)i, ret);
            return ret;
        }

        if (button_msg.switch_changed) {
            if (buttons_[i].get_state()) {
                preview_started_at_ms_ = now_ms;
                preview_button_index_ = (uint8_t)i;
                preview_active_ = false;
                preview_button_still_pressed_ = true;
            } else if (preview_button_still_pressed_ && (preview_button_index_ == i)) {
                preview_started_at_ms_ = 0;
                preview_active_ = false;
                preview_button_still_pressed_ = false;
            }
        }

        if (!button_msg.switch_changed || !buttons_[i].get_state() || (i == selected_group_)) {
            continue;
        }

        ret = select_group_(i);
        if (ret < 0) {
            LOG_ERR("Failed to select MOD group %u: %d", (unsigned int)i, ret);
            return ret;
        }

        LOG_INF("Selected MOD group %u target=%u virtual_bank=%u",
                (unsigned int)i,
                (unsigned int)selected_target_offset_[i],
                (unsigned int)current_virtual_bank_index_());
    }

    if (preview_button_still_pressed_) {
        if (!buttons_[preview_button_index_].get_state()) {
            preview_started_at_ms_ = 0;
            preview_active_ = false;
            preview_button_still_pressed_ = false;
        } else if (!preview_active_ &&
                   ((now_ms - preview_started_at_ms_) >= preview_hold_ms_)) {
            preview_active_ = true;
            LOG_INF("MOD preview active for group %u", (unsigned int)preview_button_index_);
        }
    }

    for (size_t i = 0U; i < knob_count_; ++i) {
        Knob::knob_msg msg;

        ret = knobs_[i].update(msg);
        if (ret < 0) {
            LOG_ERR("Failed to update MOD knob %u: %d", (unsigned int)i, ret);
            return ret;
        }

        if (msg.switch_changed) {
            LOG_INF("MOD knob %u button %s",
                    (unsigned int)i,
                    knobs_[i].get_state() ? "pressed" : "released");
        }

        if (msg.value_changed) {
            const size_t virtual_bank_index = current_virtual_bank_index_();
            knob_values_[virtual_bank_index][i] = knobs_[i].get_value();
            LOG_INF("MOD group %u target=%u virtual_bank=%u knob %u position=%d",
                    (unsigned int)selected_group_,
                    (unsigned int)selected_target_offset_[selected_group_],
                    (unsigned int)virtual_bank_index,
                    (unsigned int)i,
                    (int)knobs_[i].get_value());
        }
    }

    return 0;
}

bool MOD::mod_knob_pressed() const
{
    return knobs_[0].get_state();
}

bool MOD::osc_flt_led_preview_active() const
{
    return preview_active_;
}

uint8_t MOD::preview_group_index() const
{
    return preview_button_index_;
}

uint8_t MOD::preview_value_for_target_offset(uint8_t group_index, uint8_t target_offset) const
{
    if ((group_index >= selector_group_count_) || (target_offset >= targets_per_group_)) {
        return 0U;
    }

    return knob_values_[virtual_bank_index_(group_index, target_offset)][0];
}

void MOD::report_link_target(const char *block_name, size_t knob_index, uint8_t bank_index)
{
    uint8_t target_offset = 0U;
    if (!target_offset_for_link_(block_name, knob_index, bank_index, target_offset)) {
        LOG_WRN("Ignored MOD link target: %s knob %u bank %u",
                (block_name != nullptr) ? block_name : "<null>",
                (unsigned int)knob_index,
                (unsigned int)bank_index);
        return;
    }

    const size_t current_virtual_bank = current_virtual_bank_index_();
    const size_t requested_virtual_bank = virtual_bank_index_(selected_group_, target_offset);

    LOG_INF("MOD link target: %s knob %u bank %u -> group %u target=%u virtual_bank=%u",
            block_name,
            (unsigned int)knob_index,
            (unsigned int)bank_index,
            (unsigned int)selected_group_,
            (unsigned int)target_offset,
            (unsigned int)requested_virtual_bank);

    if (requested_virtual_bank == current_virtual_bank) {
        LOG_INF("MOD link target unchanged for group %u target=%u virtual_bank=%u",
                (unsigned int)selected_group_,
                (unsigned int)target_offset,
                (unsigned int)requested_virtual_bank);
        return;
    }

    const uint8_t previous_target_offset = selected_target_offset_[selected_group_];
    selected_target_offset_[selected_group_] = target_offset;

    const int ret = recall_virtual_bank_to_knobs_(requested_virtual_bank);
    if (ret < 0) {
        selected_target_offset_[selected_group_] = previous_target_offset;
        LOG_ERR("Failed to recall MOD virtual bank %u for link target: %d",
                (unsigned int)requested_virtual_bank,
                ret);
        return;
    }
}

int MOD::select_group_(size_t group_index)
{
    if (group_index >= selector_group_count_) {
        return -EINVAL;
    }

    const uint8_t previous_group = selected_group_;
    selected_group_ = (uint8_t)group_index;

    const int ret = recall_virtual_bank_to_knobs_(current_virtual_bank_index_());
    if (ret < 0) {
        selected_group_ = previous_group;
        return ret;
    }

    return update_selector_leds_();
}

int MOD::update_selector_leds_()
{
    for (size_t i = 0U; i < button_count_; ++i) {
        const int ret = buttons_[i].set_led_val((i == selected_group_) ? 100U : 0U);
        if (ret < 0) {
            return ret;
        }
    }

    return 0;
}

int MOD::recall_virtual_bank_to_knobs_(size_t virtual_bank_index)
{
    if (virtual_bank_index >= virtual_bank_count_) {
        return -EINVAL;
    }

    for (size_t i = 0U; i < knob_count_; ++i) {
        const int ret = knobs_[i].set_value(knob_values_[virtual_bank_index][i]);
        if (ret < 0) {
            return ret;
        }

        LOG_INF("MOD group %u target=%u virtual_bank=%u knob %u recalled position=%u",
                (unsigned int)selected_group_,
                (unsigned int)selected_target_offset_[selected_group_],
                (unsigned int)virtual_bank_index,
                (unsigned int)i,
                (unsigned int)knob_values_[virtual_bank_index][i]);
    }

    return 0;
}

bool MOD::target_offset_for_link_(const char *block_name,
                                  size_t knob_index,
                                  uint8_t bank_index,
                                  uint8_t &target_offset) const
{
    if (block_name == nullptr) {
        return false;
    }

    if (strcmp(block_name, "OSC") == 0) {
        if ((bank_index >= 3U) || (knob_index >= 5U)) {
            return false;
        }

        target_offset = (uint8_t)((bank_index * 5U) + knob_index);
        return true;
    }

    if (strcmp(block_name, "FLT") == 0) {
        if ((bank_index != 0U) || (knob_index >= 2U)) {
            return false;
        }

        target_offset = (uint8_t)(15U + knob_index);
        return true;
    }

    return false;
}

size_t MOD::virtual_bank_index_(size_t group_index, uint8_t target_offset) const
{
    return (group_index * targets_per_group_) + (size_t)target_offset;
}

size_t MOD::current_virtual_bank_index_() const
{
    return virtual_bank_index_(selected_group_, selected_target_offset_[selected_group_]);
}

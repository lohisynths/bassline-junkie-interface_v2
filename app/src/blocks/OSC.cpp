#include "OSC.h"

#include <errno.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(OSC, LOG_LEVEL_INF);

namespace {

uint8_t clamp_knob_value_(uint8_t value)
{
    return (value > 127U) ? 127U : value;
}

} // namespace

int OSC::init(InputController &inputs, LEDSController &leds)
{
    int ret = 0;

    for (size_t i = 0U; (i < button_count_) && (ret == 0); ++i) {
        ret = buttons_[i].init(inputs, button_configs_[i], leds);
    }

    for (size_t i = 0U; (i < knob_count_) && (ret == 0); ++i) {
        ret = knobs_[i].init(inputs, knob_configs_[i], leds);
    }

    if (ret == 0) {
        selected_bank_ = 0U;
        ret = select_bank_(selected_bank_);
    }

    return ret;
}

void OSC::capture_state(OSCState &state) const
{
    state = {};

    for (size_t bank = 0U; bank < bank_count_; ++bank) {
        for (size_t knob = 0U; knob < knob_count_; ++knob) {
            state.knob_values[bank][knob] = knob_values_[bank][knob];
        }
    }
}

int OSC::apply_state(const OSCState &state)
{
    for (size_t bank = 0U; bank < bank_count_; ++bank) {
        for (size_t knob = 0U; knob < knob_count_; ++knob) {
            knob_values_[bank][knob] = clamp_knob_value_(state.knob_values[bank][knob]);
        }
    }

    has_newly_pressed_knob_ = false;
    newly_pressed_knob_index_ = 0U;

    int ret = recall_bank_to_knobs_(selected_bank_);
    if (ret < 0) {
        return ret;
    }

    return update_selector_leds_();
}

int OSC::update()
{
    int ret = 0;
    has_newly_pressed_knob_ = false;

    for (size_t i = 0U; i < button_count_; ++i) {
        Button::button_msg button_msg;

        ret = buttons_[i].update(button_msg);
        if (ret < 0) {
            LOG_ERR("Failed to update OSC button %u: %d", (unsigned int)i, ret);
            return ret;
        }

        if (!button_msg.switch_changed || !buttons_[i].get_state() || (i == selected_bank_)) {
            continue;
        }

        ret = select_bank_(i);
        if (ret < 0) {
            LOG_ERR("Failed to select OSC bank %u: %d", (unsigned int)i, ret);
            return ret;
        }

        LOG_INF("Selected OSC bank %u", (unsigned int)i);
    }

    for (size_t i = 0U; i < knob_count_; ++i) {
        Knob::knob_msg msg;

        ret = knobs_[i].update(msg);
        if (ret < 0) {
            LOG_ERR("Failed to update OSC knob %u: %d", (unsigned int)i, ret);
            return ret;
        }

        if (msg.switch_changed) {
            if (knobs_[i].get_state() && !has_newly_pressed_knob_) {
                has_newly_pressed_knob_ = true;
                newly_pressed_knob_index_ = (uint8_t)i;
            }

            LOG_INF("OSC knob %u button %s",
                    (unsigned int)i,
                    knobs_[i].get_state() ? "pressed" : "released");
        }

        if (msg.value_changed) {
            knob_values_[selected_bank_][i] = knobs_[i].get_value();
            LOG_INF("OSC bank %u knob %u position=%d",
                    (unsigned int)selected_bank_,
                    (unsigned int)i,
                    (int)knobs_[i].get_value());
        }
    }

    return 0;
}

bool OSC::take_newly_pressed_knob(size_t &knob_index)
{
    if (!has_newly_pressed_knob_) {
        return false;
    }

    knob_index = newly_pressed_knob_index_;
    has_newly_pressed_knob_ = false;
    return true;
}

uint8_t OSC::selected_bank() const
{
    return selected_bank_;
}

int OSC::select_bank_(size_t bank_index)
{
    if (bank_index >= bank_count_) {
        return -EINVAL;
    }

    selected_bank_ = (uint8_t)bank_index;

    int ret = recall_bank_to_knobs_(bank_index);
    if (ret < 0) {
        return ret;
    }

    return update_selector_leds_();
}

int OSC::update_selector_leds_()
{
    for (size_t i = 0U; i < button_count_; ++i) {
        const int ret = buttons_[i].set_led_val((i == selected_bank_) ? 100U : 0U);
        if (ret < 0) {
            return ret;
        }
    }

    return 0;
}

int OSC::recall_bank_to_knobs_(size_t bank_index)
{
    if (bank_index >= bank_count_) {
        return -EINVAL;
    }

    for (size_t i = 0U; i < knob_count_; ++i) {
        const int ret = knobs_[i].set_value(knob_values_[bank_index][i]);
        if (ret < 0) {
            return ret;
        }

        LOG_INF("OSC knob %u recalled position=%u",
                (unsigned int)i,
                (unsigned int)knob_values_[bank_index][i]);
    }

    return 0;
}

#include "MOD.h"

#include <errno.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(MOD, LOG_LEVEL_INF);

namespace {

uint8_t clamp_knob_value_(uint8_t value)
{
    return (value > 127U) ? 127U : value;
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
        selected_bank_ = 0U;
        ret = select_bank_(selected_bank_);
    }

    return ret;
}

void MOD::capture_state(MODState &state) const
{
    state = {};

    for (size_t bank = 0U; bank < bank_count_; ++bank) {
        for (size_t knob = 0U; knob < knob_count_; ++knob) {
            state.knob_values[bank][knob] = knob_values_[bank][knob];
        }
    }
}

int MOD::apply_state(const MODState &state)
{
    for (size_t bank = 0U; bank < bank_count_; ++bank) {
        for (size_t knob = 0U; knob < knob_count_; ++knob) {
            knob_values_[bank][knob] = clamp_knob_value_(state.knob_values[bank][knob]);
        }
    }

    int ret = recall_bank_to_knobs_(selected_bank_);
    if (ret < 0) {
        return ret;
    }

    return update_selector_leds_();
}

int MOD::update()
{
    int ret = 0;

    for (size_t i = 0U; i < button_count_; ++i) {
        Button::button_msg button_msg;

        ret = buttons_[i].update(button_msg);
        if (ret < 0) {
            LOG_ERR("Failed to update MOD button %u: %d", (unsigned int)i, ret);
            return ret;
        }

        if (!button_msg.switch_changed || !buttons_[i].get_state() || (i == selected_bank_)) {
            continue;
        }

        ret = select_bank_(i);
        if (ret < 0) {
            LOG_ERR("Failed to select MOD bank %u: %d", (unsigned int)i, ret);
            return ret;
        }

        LOG_INF("Selected MOD bank %u", (unsigned int)i);
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
            knob_values_[selected_bank_][i] = knobs_[i].get_value();
            LOG_INF("MOD bank %u knob %u position=%d",
                    (unsigned int)selected_bank_,
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

void MOD::report_link_target(const char *block_name, size_t knob_index, uint8_t bank_index)
{
    LOG_INF("MOD link target: %s knob %u bank %u",
            block_name,
            (unsigned int)knob_index,
            (unsigned int)bank_index);
}

int MOD::select_bank_(size_t bank_index)
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

int MOD::update_selector_leds_()
{
    for (size_t i = 0U; i < button_count_; ++i) {
        const int ret = buttons_[i].set_led_val((i == selected_bank_) ? 100U : 0U);
        if (ret < 0) {
            return ret;
        }
    }

    return 0;
}

int MOD::recall_bank_to_knobs_(size_t bank_index)
{
    if (bank_index >= bank_count_) {
        return -EINVAL;
    }

    for (size_t i = 0U; i < knob_count_; ++i) {
        const int ret = knobs_[i].set_value(knob_values_[bank_index][i]);
        if (ret < 0) {
            return ret;
        }

        LOG_INF("MOD knob %u recalled position=%u",
                (unsigned int)i,
                (unsigned int)knob_values_[bank_index][i]);
    }

    return 0;
}

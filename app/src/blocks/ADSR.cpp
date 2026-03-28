#include "ADSR.h"

#include <errno.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ADSR, LOG_LEVEL_INF);

int ADSR::init(InputController &inputs, LEDSController &leds)
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

int ADSR::update()
{
    int ret = 0;

    for (size_t i = 0U; i < button_count_; ++i) {
        Button::button_msg button_msg;

        ret = buttons_[i].update(button_msg);
        if (ret < 0) {
            LOG_ERR("Failed to update button %u: %d", (unsigned int)i, ret);
            return ret;
        }

        if (i >= bank_count_) {
            continue;
        }

        if (!button_msg.switch_changed || !buttons_[i].get_state() ||
            (i == selected_bank_)) {
            continue;
        }

        ret = select_bank_(i);
        if (ret < 0) {
            LOG_ERR("Failed to select knob bank %u: %d", (unsigned int)i, ret);
            return ret;
        }

        LOG_INF("Selected knob bank %u", (unsigned int)i);
    }

    for (size_t i = 0U; i < knob_count_; ++i) {
        Knob::knob_msg msg;

        ret = knobs_[i].update(msg);
        if (ret < 0) {
            LOG_ERR("Failed to update knob %u: %d", (unsigned int)i, ret);
            return ret;
        }

        if (msg.switch_changed) {
            LOG_INF("Knob %u button %s",
                    (unsigned int)i,
                    knobs_[i].get_state() ? "pressed" : "released");
        }

        if (msg.value_changed) {
            knob_values_[selected_bank_][i] = knobs_[i].get_value();
            LOG_INF("Bank %u knob %u position=%d",
                    (unsigned int)selected_bank_,
                    (unsigned int)i,
                    (int)knobs_[i].get_value());
        }
    }

    return 0;
}

int ADSR::select_bank_(size_t bank_index)
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

int ADSR::update_selector_leds_()
{
    for (size_t i = 0U; i < button_count_; ++i) {
        const uint8_t brightness = (i == selected_bank_) ? 100U : 0U;
        const int ret = buttons_[i].set_led_val(brightness);
        if (ret < 0) {
            return ret;
        }
    }

    return 0;
}

int ADSR::recall_bank_to_knobs_(size_t bank_index)
{
    if (bank_index >= bank_count_) {
        return -EINVAL;
    }

    for (size_t i = 0U; i < knob_count_; ++i) {
        const int ret = knobs_[i].set_value(knob_values_[bank_index][i]);
        if (ret < 0) {
            return ret;
        }

        LOG_INF("Knob %u recalled position=%u",
                (unsigned int)i,
                (unsigned int)knob_values_[bank_index][i]);
    }

    return 0;
}

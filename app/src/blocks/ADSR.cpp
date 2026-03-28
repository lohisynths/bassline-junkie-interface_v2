#include "ADSR.h"

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

        if (!button_msg.switch_changed) {
            continue;
        }

        const bool button_state = buttons_[i].get_state();
        ret = buttons_[i].set_led_val(button_state ? 100U : 0U);
        if (ret < 0) {
            LOG_ERR("Failed to set button %u LED: %d", (unsigned int)i, ret);
            return ret;
        }

        LOG_INF("Button %u mux=%u bit=%u %s",
                (unsigned int)i,
                (unsigned int)button_configs_[i].mux_index,
                (unsigned int)button_configs_[i].pin,
                button_state ? "pressed" : "released");
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
            LOG_INF("Knob %u position=%d",
                    (unsigned int)i,
                    (int)knobs_[i].get_value());
        }
    }

    return 0;
}

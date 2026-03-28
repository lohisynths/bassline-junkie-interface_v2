#include "LED_DISP.h"

#include <errno.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(LED_DISP, LOG_LEVEL_INF);

int LED_DISP::init(InputController &inputs, LEDSController &leds)
{
    leds_ = nullptr;

    int ret = knob_.init(inputs, knob_config_, leds);
    if (ret < 0) {
        return ret;
    }

    leds_ = &leds;
    return render_value_();
}

int LED_DISP::update()
{
    Knob::knob_msg knob_msg;

    int ret = knob_.update(knob_msg);
    if (ret < 0) {
        LOG_ERR("Failed to update LED display knob: %d", ret);
        return ret;
    }

    if (knob_msg.switch_changed) {
        LOG_INF("LED display knob button %s",
                knob_.get_state() ? "pressed" : "released");
    }

    if (!knob_msg.value_changed) {
        return 0;
    }

    ret = render_value_();
    if (ret < 0) {
        LOG_ERR("Failed to update LED display value: %d", ret);
        return ret;
    }

    LOG_INF("LED display position=%d", (int)knob_.get_value());
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

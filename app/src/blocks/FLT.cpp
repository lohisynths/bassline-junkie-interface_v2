#include "FLT.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(FLT, LOG_LEVEL_INF);

namespace {

uint8_t clamp_knob_value_(uint8_t value)
{
    return (value > 127U) ? 127U : value;
}

uint8_t clamp_index_(uint8_t index, size_t count)
{
    if ((count == 0U) || (index < count)) {
        return index;
    }

    return (uint8_t)(count - 1U);
}

} // namespace

int FLT::init(InputController &inputs, LEDSController &leds)
{
    int ret = 0;

    for (size_t i = 0U; (i < button_count_) && (ret == 0); ++i) {
        ret = buttons_[i].init(inputs, button_configs_[i], leds);
    }

    for (size_t i = 0U; (i < knob_count_) && (ret == 0); ++i) {
        ret = knobs_[i].init(inputs, knob_configs_[i], leds);
    }

    if (ret == 0) {
        selected_button_ = 0U;
        ret = update_selector_leds_();
    }

    return ret;
}

void FLT::capture_state(FLTState &state) const
{
    state = {};
    state.selected_button = selected_button_;

    for (size_t i = 0U; i < knob_count_; ++i) {
        state.knob_values[i] = knobs_[i].get_value();
    }
}

int FLT::apply_state(const FLTState &state)
{
    selected_button_ = clamp_index_(state.selected_button, button_count_);
    has_newly_pressed_knob_ = false;
    newly_pressed_knob_index_ = 0U;

    int ret = update_selector_leds_();
    if (ret < 0) {
        return ret;
    }

    for (size_t i = 0U; i < knob_count_; ++i) {
        ret = knobs_[i].set_value(clamp_knob_value_(state.knob_values[i]));
        if (ret < 0) {
            return ret;
        }
    }

    return 0;
}

int FLT::update()
{
    int ret = 0;
    has_newly_pressed_knob_ = false;

    for (size_t i = 0U; i < button_count_; ++i) {
        Button::button_msg button_msg;

        ret = buttons_[i].update(button_msg);
        if (ret < 0) {
            LOG_ERR("Failed to update FLT button %u: %d", (unsigned int)i, ret);
            return ret;
        }

        if (!button_msg.switch_changed || !buttons_[i].get_state()) {
            continue;
        }

        if (selected_button_ == i) {
            continue;
        }

        selected_button_ = (uint8_t)i;
        ret = update_selector_leds_();
        if (ret < 0) {
            LOG_ERR("Failed to update FLT radio selection %u: %d", (unsigned int)i, ret);
            return ret;
        }

        LOG_INF("FLT radio button %u selected", (unsigned int)i);
    }

    for (size_t i = 0U; i < knob_count_; ++i) {
        Knob::knob_msg knob_msg;

        ret = knobs_[i].update(knob_msg);
        if (ret < 0) {
            LOG_ERR("Failed to update FLT knob %u: %d", (unsigned int)i, ret);
            return ret;
        }

        if (knob_msg.switch_changed) {
            if (knobs_[i].get_state() && !has_newly_pressed_knob_) {
                has_newly_pressed_knob_ = true;
                newly_pressed_knob_index_ = (uint8_t)i;
            }

            LOG_INF("FLT knob %u button %s",
                    (unsigned int)i,
                    knobs_[i].get_state() ? "pressed" : "released");
        }

        if (knob_msg.value_changed) {
            LOG_INF("FLT knob %u position=%d",
                    (unsigned int)i,
                    (int)knobs_[i].get_value());
        }
    }

    return 0;
}

bool FLT::take_newly_pressed_knob(size_t &knob_index)
{
    if (!has_newly_pressed_knob_) {
        return false;
    }

    knob_index = newly_pressed_knob_index_;
    has_newly_pressed_knob_ = false;
    return true;
}

int FLT::update_selector_leds_()
{
    for (size_t i = 0U; i < button_count_; ++i) {
        const int ret = buttons_[i].set_led_val((i == selected_button_) ? 100U : 0U);
        if (ret < 0) {
            return ret;
        }
    }

    return 0;
}

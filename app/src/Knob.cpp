#include "Knob.h"

#include <errno.h>

static const uint8_t knob_brightness_percent = 50U;
static const uint8_t knob_max_value = 127U;

int Knob::init(InputController &inputs, const Config &config, LEDSController &leds)
{
    initialized_ = false;

    if ((config.button_mux_index >= InputController::input_count) ||
        (config.button_pin >= 16U) ||
        (config.led_count == 0U) ||
        ((config.first_led + config.led_count) > LEDSController::led_count)) {
        return -EINVAL;
    }

    int ret = encoder_.init(inputs,
                            config.encoder_mux_index,
                            config.encoder_pin_a,
                            config.encoder_pin_b);
    if (ret < 0) {
        return ret;
    }

    inputs_ = &inputs;
    button_mux_index_ = config.button_mux_index;
    button_pin_ = config.button_pin;
    leds_ = &leds;
    first_led_ = config.first_led;
    led_count_ = config.led_count;

    value_ = 0U;
    pressed_ = false;
    previous_led_index_ = led_index_(value_);

    initialized_ = true;
    return leds_->set_channel_percent(first_led_ + previous_led_index_, knob_brightness_percent);
}

bool Knob::get_state()
{
    return pressed_;
}

uint8_t Knob::get_value()
{
    return value_;
}

int Knob::update(knob_msg &msg)
{
    if (!initialized_) {
        return -EACCES;
    }

    msg = {};

    const bool previous_button_pressed = pressed_;
    const uint8_t previous_value = value_;

    int ret = encoder_.update();
    if (ret < 0) {
        return ret;
    }

    const uint16_t state = inputs_->state(button_mux_index_);
    pressed_ = ((state >> button_pin_) & 0x1U) == 0U;
    msg.switch_changed = (pressed_ != previous_button_pressed);

    const int32_t delta = encoder_.delta();
    if (delta == 0) {
        return 0;
    }

    const int32_t next_value = (int32_t)value_ + delta;
    if (next_value < 0) {
        value_ = 0U;
    } else if (next_value > (int32_t)knob_max_value) {
        value_ = knob_max_value;
    } else {
        value_ = (uint8_t)next_value;
    }

    msg.value_changed = (value_ != previous_value);
    if (!msg.value_changed) {
        return 0;
    }

    const size_t current_led_index = led_index_(value_);
    if (current_led_index == previous_led_index_) {
        return 0;
    }

    ret = leds_->set_channel_percent(first_led_ + previous_led_index_, 0U);
    if (ret < 0) {
        return ret;
    }

    ret = leds_->set_channel_percent(first_led_ + current_led_index, knob_brightness_percent);
    if (ret < 0) {
        return ret;
    }

    previous_led_index_ = current_led_index;

    return 0;
}

size_t Knob::led_index_(uint8_t value)
{
    const size_t index = ((size_t)value * led_count_) / (size_t)(knob_max_value + 1U);
    return (led_count_ - 1U) - index;
}

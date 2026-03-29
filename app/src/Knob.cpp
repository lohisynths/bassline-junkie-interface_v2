#include "Knob.h"

#include <errno.h>

static const uint8_t knob_brightness_percent = 50U;
static const uint8_t knob_max_value = 127U;

int Knob::init(InputController &inputs, const Config &config, LEDSController &leds)
{
    initialized_ = false;

    if ((config.button_mux_index >= InputController::input_count) ||
        (config.button_pin >= 16U) ||
        (config.encoder_step_divider == 0U) ||
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
    encoder_step_divider_ = config.encoder_step_divider;

    value_ = 0U;
    pressed_ = false;
    displayed_led_index_ = led_count_;
    pending_encoder_steps_ = 0;

    initialized_ = true;
    return render_current_value_();
}

bool Knob::get_state() const
{
    return pressed_;
}

uint8_t Knob::get_value() const
{
    return value_;
}

int Knob::set_value(uint8_t value)
{
    if (!initialized_) {
        return -EACCES;
    }

    if (value > knob_max_value) {
        value_ = knob_max_value;
    } else {
        value_ = value;
    }

    pending_encoder_steps_ = 0;

    return render_current_value_();
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

    pending_encoder_steps_ += encoder_.delta();

    int32_t delta = 0;
    while (pending_encoder_steps_ >= (int32_t)encoder_step_divider_) {
        pending_encoder_steps_ -= (int32_t)encoder_step_divider_;
        ++delta;
    }

    while (pending_encoder_steps_ <= -(int32_t)encoder_step_divider_) {
        pending_encoder_steps_ += (int32_t)encoder_step_divider_;
        --delta;
    }

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

    return render_current_value_();
}

int Knob::show_preview_value(uint8_t value)
{
    if (!initialized_) {
        return -EACCES;
    }

    if (led_count_ == 0U) {
        return 0;
    }

    if (value > knob_max_value) {
        value = knob_max_value;
    }

    return render_led_index_(led_index_(value));
}

int Knob::restore_displayed_value()
{
    if (!initialized_) {
        return -EACCES;
    }

    return render_current_value_();
}

int Knob::render_current_value_()
{
    if (led_count_ == 0U) {
        return 0;
    }

    return render_led_index_(led_index_(value_));
}

int Knob::render_led_index_(size_t led_index)
{
    if (led_count_ == 0U) {
        return 0;
    }

    if (led_index >= led_count_) {
        return -EINVAL;
    }

    if (led_index == displayed_led_index_) {
        return 0;
    }

    int ret = 0;
    if (displayed_led_index_ < led_count_) {
        ret = leds_->set_channel_percent(first_led_ + displayed_led_index_, 0U);
        if (ret < 0) {
            return ret;
        }
    }

    ret = leds_->set_channel_percent(first_led_ + led_index, knob_brightness_percent);
    if (ret < 0) {
        return ret;
    }

    displayed_led_index_ = led_index;

    return 0;
}

size_t Knob::led_index_(uint8_t value)
{
    if (led_count_ == 0U) {
        return 0U;
    }

    const size_t index = ((size_t)value * led_count_) / (size_t)(knob_max_value + 1U);
    return (led_count_ - 1U) - index;
}

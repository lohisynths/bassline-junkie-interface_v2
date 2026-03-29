#include "Button.h"

#include <errno.h>

static const uint8_t button_led_off_percent = 0U;
static const uint8_t button_led_on_percent = 100U;

int Button::init(InputController &inputs, const Config &config, LEDSController &leds)
{
    if ((config.mux_index >= InputController::input_count) ||
        (config.pin >= 16U) ||
        (config.led_number >= LEDSController::led_count)) {
        return -EINVAL;
    }

    inputs_ = &inputs;
    leds_ = &leds;
    mux_index_ = config.mux_index;
    pin_ = config.pin;
    led_number_ = config.led_number;
    pressed_ = false;

    return set_led_val(button_led_off_percent);
}

int Button::update(button_msg &msg)
{
    msg = {};

    if (inputs_ == nullptr) {
        return -EACCES;
    }

    const bool previous_pressed = pressed_;
    const uint16_t state = inputs_->state(mux_index_);
    pressed_ = ((state >> pin_) & 0x1U) == 0U;
    msg.switch_changed = (pressed_ != previous_pressed);

    return 0;
}

bool Button::get_state() const
{
    return pressed_;
}

int Button::set_led_val(uint8_t percent)
{
    if (leds_ == nullptr) {
        return -EACCES;
    }

    return leds_->set_channel_percent(led_number_, percent);
}

#include "Button.h"

#include <errno.h>

static const uint8_t button_led_off_percent = 0U;
static const uint8_t button_led_on_percent = 100U;

int Button::init(InputController &inputs,
                 size_t mux_index,
                 uint8_t pin,
                 LEDSController &leds,
                 size_t led_number)
{
    if ((mux_index >= InputController::state_count) ||
        (pin >= 16U) ||
        (led_number >= LEDSController::led_count)) {
        return -EINVAL;
    }

    inputs_ = &inputs;
    leds_ = &leds;
    mux_index_ = mux_index;
    pin_ = pin;
    led_number_ = led_number;
    pressed_ = false;

    return set_led_val(button_led_off_percent);
}

int Button::update()
{
    if ((inputs_ == nullptr) || (leds_ == nullptr)) {
        return -EACCES;
    }

    const uint16_t state = inputs_->state(mux_index_);
    pressed_ = ((state >> pin_) & 0x1U) == 0U;

    const uint8_t led_percent = pressed_ ? button_led_on_percent : button_led_off_percent;
    return set_led_val(led_percent);
}

bool Button::get_state()
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

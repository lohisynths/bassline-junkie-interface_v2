#include "Button.h"

#include <errno.h>

int Button::init(InputController &inputs, size_t state_index, uint8_t pin)
{
    if ((state_index >= InputController::state_count) || (pin >= 16U)) {
        return -EINVAL;
    }

    inputs_ = &inputs;
    state_index_ = state_index;
    pin_ = pin;
    pressed_ = false;

    return 0;
}

int Button::update()
{
    if (inputs_ == nullptr) {
        return -EACCES;
    }

    const uint16_t state = inputs_->state(state_index_);
    pressed_ = ((state >> pin_) & 0x1U) == 0U;

    return 0;
}

bool Button::get_state()
{
    return pressed_;
}

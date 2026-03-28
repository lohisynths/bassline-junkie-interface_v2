#include "Encoder.h"

#include <errno.h>

int Encoder::init(InputController &inputs, size_t mux_index, uint8_t pin_a, uint8_t pin_b)
{
    if ((mux_index >= InputController::input_count) || (pin_a >= 16U) || (pin_b >= 16U) ||
        (pin_a == pin_b)) {
        return -EINVAL;
    }

    inputs_ = &inputs;
    mux_index_ = mux_index;
    pin_a_ = pin_a;
    pin_b_ = pin_b;
    seeded_ = false;
    previous_ab_ = 0U;
    delta_ = 0;
    position_ = 0;

    return 0;
}

int Encoder::update()
{
    if (inputs_ == nullptr) {
        return -EACCES;
    }

    const uint16_t mux_state = inputs_->state(mux_index_);

    // Sample the configured mux channels and pack them into a 2-bit AB state.
    // Bit 0 holds phase A and bit 1 holds phase B so the state can be compared
    // directly against the previous sample by transition_().
    const uint8_t current_ab = (uint8_t)((((mux_state >> pin_a_) & 0x1U) << 0) |
                                         (((mux_state >> pin_b_) & 0x1U) << 1));

    // Reset the per-update delta. If no valid quadrature edge is detected in
    // this call, delta() will return 0.
    delta_ = 0;

    if (!seeded_) {
        // The first sample only establishes the initial AB state. We do not
        // report movement until we have two samples to compare.
        previous_ab_ = current_ab;
        seeded_ = true;
        return 0;
    }

    // Translate the previous/current AB pair into one quarter-step of motion:
    // -1 for one direction, +1 for the other, or 0 for no movement / invalid
    // transition. Invalid transitions can happen when the state repeats or
    // mechanical bounce skips an expected intermediate state.
    const int8_t quarter_step = transition_(previous_ab_, current_ab);
    previous_ab_ = current_ab;

    // In quarter-step mode we expose the transition result directly. Each valid
    // edge updates both the one-shot delta and the accumulated position.
    delta_ = quarter_step;
    position_ += quarter_step;

    return 0;
}

int32_t Encoder::delta() const
{
    return delta_;
}

int32_t Encoder::position() const
{
    return position_;
}

int8_t Encoder::transition_(uint8_t previous_ab, uint8_t current_ab)
{
    // The 4-bit index is previous AB in bits 3:2 and current AB in bits 1:0.
    // Valid quadrature edges map to +/-1 quarter-steps; repeated or illegal
    // state changes map to 0 so contact bounce and skipped states are ignored.
    static const int8_t transition_table[16] = {
        0, -1, 1, 0,
        1, 0, 0, -1,
        -1, 0, 0, 1,
        0, 1, -1, 0,
    };

    return transition_table[(previous_ab << 2) | current_ab];
}

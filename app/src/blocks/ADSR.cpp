#include "ADSR.h"

#include <errno.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ADSR, LOG_LEVEL_INF);

namespace {

uint8_t clamp_knob_value_(uint8_t value)
{
    return (value > 127U) ? 127U : value;
}

} // namespace

int ADSR::init(InputController &inputs, LEDSController &leds, MIDI *midi)
{
    int ret = 0;

    midi_ = (midi != nullptr && midi->is_initialized()) ? midi : nullptr;

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

void ADSR::capture_state(ADSRState &state) const
{
    state = {};

    for (size_t bank = 0U; bank < bank_count_; ++bank) {
        state.button3_values[bank] = button3_values_[bank] ? 1U : 0U;

        for (size_t knob = 0U; knob < knob_count_; ++knob) {
            state.knob_values[bank][knob] = knob_values_[bank][knob];
        }
    }
}

int ADSR::apply_state(const ADSRState &state)
{
    for (size_t bank = 0U; bank < bank_count_; ++bank) {
        button3_values_[bank] = state.button3_values[bank] != 0U;

        for (size_t knob = 0U; knob < knob_count_; ++knob) {
            knob_values_[bank][knob] = clamp_knob_value_(state.knob_values[bank][knob]);
        }
    }

    int ret = recall_bank_to_knobs_(selected_bank_);
    if (ret < 0) {
        return ret;
    }

    ret = update_selector_leds_();
    if (ret < 0) {
        return ret;
    }

    send_all_midi_cc_();
    return 0;
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

        if ((i < bank_count_) &&
            button_msg.switch_changed &&
            buttons_[i].get_state() &&
            (i != selected_bank_)) {
            ret = select_bank_(i);
            if (ret < 0) {
                LOG_ERR("Failed to select knob bank %u: %d", (unsigned int)i, ret);
                return ret;
            }

            LOG_INF("Selected knob bank %u", (unsigned int)i);
            continue;
        }

        if ((i != (button_count_ - 1U)) ||
            !button_msg.switch_changed ||
            !buttons_[i].get_state()) {
            continue;
        }

        button3_values_[selected_bank_] = !button3_values_[selected_bank_];
        ret = update_selector_leds_();
        if (ret < 0) {
            LOG_ERR("Failed to update button 3 latch for bank %u: %d",
                    (unsigned int)selected_bank_,
                    ret);
            return ret;
        }

        LOG_INF("Bank %u button 3 latched %s",
                (unsigned int)selected_bank_,
                button3_values_[selected_bank_] ? "on" : "off");

        send_button3_midi_cc_(selected_bank_, button3_values_[selected_bank_]);
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

            send_knob_midi_cc_(selected_bank_, i, knobs_[i].get_value());
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
        uint8_t brightness = 0U;
        if (i < bank_count_) {
            brightness = (i == selected_bank_) ? 100U : 0U;
        } else if (button3_values_[selected_bank_]) {
            brightness = 100U;
        }

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

void ADSR::send_knob_midi_cc_(size_t bank_index, size_t knob_index, uint8_t value)
{
    if ((midi_ == nullptr) || (bank_index >= bank_count_) || (knob_index >= knob_count_)) {
        return;
    }

    const uint8_t cc_number = (uint8_t)(midi_cc_base_ + (bank_index * 5U) + knob_index);
    const int ret = midi_->send_cc(cc_number, value, 0U);
    if (ret < 0) {
        LOG_ERR("Failed to send ADSR MIDI CC %u for bank %u knob %u: %d",
                (unsigned int)cc_number,
                (unsigned int)bank_index,
                (unsigned int)knob_index,
                ret);
    }
}

void ADSR::send_button3_midi_cc_(size_t bank_index, bool enabled)
{
    if ((midi_ == nullptr) || (bank_index >= bank_count_)) {
        return;
    }

    const uint8_t cc_number = (uint8_t)(midi_cc_base_ + (bank_index * 5U) + knob_count_);
    const uint8_t value = enabled ? 127U : 0U;
    const int ret = midi_->send_cc(cc_number, value, 0U);
    if (ret < 0) {
        LOG_ERR("Failed to send ADSR MIDI CC %u for bank %u button 3: %d",
                (unsigned int)cc_number,
                (unsigned int)bank_index,
                ret);
    }
}

void ADSR::send_all_midi_cc_()
{
    for (size_t bank = 0U; bank < bank_count_; ++bank) {
        for (size_t knob = 0U; knob < knob_count_; ++knob) {
            send_knob_midi_cc_(bank, knob, knob_values_[bank][knob]);
        }

        send_button3_midi_cc_(bank, button3_values_[bank]);
    }
}

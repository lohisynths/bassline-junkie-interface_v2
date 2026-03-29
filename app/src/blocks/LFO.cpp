#include "LFO.h"

#include <errno.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(LFO, LOG_LEVEL_INF);

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

int LFO::init(InputController &inputs, LEDSController &leds, MIDI *midi)
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
        if (ret == 0) {
            send_all_midi_cc_();
        }
    }

    return ret;
}

void LFO::capture_state(LFOState &state) const
{
    state = {};

    for (size_t bank = 0U; bank < bank_count_; ++bank) {
        state.radio_selection[bank] = radio_selection_[bank];

        for (size_t knob = 0U; knob < knob_count_; ++knob) {
            state.knob_values[bank][knob] = knob_values_[bank][knob];
        }
    }
}

int LFO::apply_state(const LFOState &state)
{
    for (size_t bank = 0U; bank < bank_count_; ++bank) {
        radio_selection_[bank] = clamp_index_(state.radio_selection[bank], radio_button_count_);

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

int LFO::update()
{
    int ret = 0;

    for (size_t i = 0U; i < button_count_; ++i) {
        Button::button_msg button_msg;

        ret = buttons_[i].update(button_msg);
        if (ret < 0) {
            LOG_ERR("Failed to update LFO button %u: %d", (unsigned int)i, ret);
            return ret;
        }

        if (!button_msg.switch_changed || !buttons_[i].get_state()) {
            continue;
        }

        if ((i < bank_count_) && (i != selected_bank_)) {
            ret = select_bank_(i);
            if (ret < 0) {
                LOG_ERR("Failed to select LFO bank %u: %d", (unsigned int)i, ret);
                return ret;
            }

            LOG_INF("Selected LFO bank %u", (unsigned int)i);
            continue;
        }

        if (i < radio_button_offset_) {
            continue;
        }

        const size_t radio_index = i - radio_button_offset_;
        if (radio_index >= radio_button_count_) {
            continue;
        }

        if (radio_selection_[selected_bank_] == radio_index) {
            continue;
        }

        radio_selection_[selected_bank_] = (uint8_t)radio_index;
        ret = update_selector_leds_();
        if (ret < 0) {
            LOG_ERR("Failed to update LFO radio selection for bank %u: %d",
                    (unsigned int)selected_bank_,
                    ret);
            return ret;
        }

        LOG_INF("LFO bank %u radio %u selected",
                (unsigned int)selected_bank_,
                (unsigned int)radio_index);
        send_radio_midi_cc_(selected_bank_, (uint8_t)radio_index);
    }

    for (size_t i = 0U; i < knob_count_; ++i) {
        Knob::knob_msg msg;

        ret = knobs_[i].update(msg);
        if (ret < 0) {
            LOG_ERR("Failed to update LFO knob %u: %d", (unsigned int)i, ret);
            return ret;
        }

        if (msg.switch_changed) {
            LOG_INF("LFO knob %u button %s",
                    (unsigned int)i,
                    knobs_[i].get_state() ? "pressed" : "released");
        }

        if (msg.value_changed) {
            knob_values_[selected_bank_][i] = knobs_[i].get_value();
            LOG_INF("LFO bank %u knob %u position=%d",
                    (unsigned int)selected_bank_,
                    (unsigned int)i,
                    (int)knobs_[i].get_value());
            send_knob_midi_cc_(selected_bank_, knobs_[i].get_value());
        }
    }

    return 0;
}

int LFO::select_bank_(size_t bank_index)
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

int LFO::update_selector_leds_()
{
    for (size_t i = 0U; i < button_count_; ++i) {
        uint8_t brightness = 0U;

        if (i < bank_count_) {
            brightness = (i == selected_bank_) ? 100U : 0U;
        } else {
            const size_t radio_index = i - radio_button_offset_;
            brightness = (radio_selection_[selected_bank_] == radio_index) ? 100U : 0U;
        }

        const int ret = buttons_[i].set_led_val(brightness);
        if (ret < 0) {
            return ret;
        }
    }

    return 0;
}

int LFO::recall_bank_to_knobs_(size_t bank_index)
{
    if (bank_index >= bank_count_) {
        return -EINVAL;
    }

    for (size_t i = 0U; i < knob_count_; ++i) {
        const int ret = knobs_[i].set_value(knob_values_[bank_index][i]);
        if (ret < 0) {
            return ret;
        }

        LOG_INF("LFO knob %u recalled position=%u",
                (unsigned int)i,
                (unsigned int)knob_values_[bank_index][i]);
    }

    return 0;
}

void LFO::send_radio_midi_cc_(size_t bank_index, uint8_t radio_index)
{
    if ((midi_ == nullptr) || (bank_index >= bank_count_) || (radio_index >= 4U)) {
        return;
    }

    const uint8_t cc_number = (uint8_t)(midi_cc_base_ + (bank_index * 2U));
    const int ret = midi_->send_cc(cc_number, radio_index, 0U);
    if (ret < 0) {
        LOG_ERR("Failed to send LFO MIDI CC %u for bank %u radio %u: %d",
                (unsigned int)cc_number,
                (unsigned int)bank_index,
                (unsigned int)radio_index,
                ret);
    }
}

void LFO::send_knob_midi_cc_(size_t bank_index, uint8_t value)
{
    if ((midi_ == nullptr) || (bank_index >= bank_count_)) {
        return;
    }

    const uint8_t cc_number = (uint8_t)(midi_cc_base_ + (bank_index * 2U) + 1U);
    const int ret = midi_->send_cc(cc_number, value, 0U);
    if (ret < 0) {
        LOG_ERR("Failed to send LFO MIDI CC %u for bank %u knob: %d",
                (unsigned int)cc_number,
                (unsigned int)bank_index,
                ret);
    }
}

void LFO::send_all_midi_cc_()
{
    for (size_t bank = 0U; bank < bank_count_; ++bank) {
        if (radio_selection_[bank] < 4U) {
            send_radio_midi_cc_(bank, radio_selection_[bank]);
        }

        send_knob_midi_cc_(bank, knob_values_[bank][0]);
    }
}

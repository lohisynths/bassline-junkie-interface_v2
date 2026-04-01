/*
 * OSC.h
 *
 *  Created on: Mar 29, 2026
 *      Author: alax
 */

#ifndef APP_SRC_BLOCKS_OSC_H_
#define APP_SRC_BLOCKS_OSC_H_

#include "UI_BLOCK.h"

#define OSC_KNOB_COUNT              (5)
#define OSC_BUTTON_COUNT            (3)
#define OSC_COUNT                   (3)



enum OSC_PARAMS {
    OSC_PITCH,
    OSC_SIN,
    OSC_SAW,
    OSC_SQR,
    OSC_RND,
    OSC_PARAM_COUNT
};


class OSC : public UI_BLOCK<OSC, OSC_KNOB_COUNT, OSC_BUTTON_COUNT, OSC_PARAM_COUNT, OSC_COUNT> {
public:

    static constexpr std::array button_configs_ = {
        Button::Config{ .mux_index = 3U, .pin = 3U, .led_number = 110U },
        Button::Config{ .mux_index = 3U, .pin = 2U, .led_number = 109U },
        Button::Config{ .mux_index = 3U, .pin = 1U, .led_number = 108U },
    };

    static constexpr std::array knob_configs_ = {
        Knob::Config{
            .button_mux_index = 1U,
            .button_pin = 12U,
            .encoder_mux_index = 1U,
            .encoder_pin_a = 13U,
            .encoder_pin_b = 14U,
            .first_led = 96U,
            .led_count = 10U,
        },
        Knob::Config{
            .button_mux_index = 1U,
            .button_pin = 9U,
            .encoder_mux_index = 1U,
            .encoder_pin_a = 10U,
            .encoder_pin_b = 11U,
            .first_led = 78U,
            .led_count = 10U,
        },
        Knob::Config{
            .button_mux_index = 1U,
            .button_pin = 6U,
            .encoder_mux_index = 1U,
            .encoder_pin_a = 7U,
            .encoder_pin_b = 8U,
            .first_led = 68U,
            .led_count = 10U,
        },
        Knob::Config{
            .button_mux_index = 1U,
            .button_pin = 3U,
            .encoder_mux_index = 1U,
            .encoder_pin_a = 4U,
            .encoder_pin_b = 5U,
            .first_led = 58U,
            .led_count = 10U,
        },
        Knob::Config{
            .button_mux_index = 1U,
            .button_pin = 0U,
            .encoder_mux_index = 1U,
            .encoder_pin_a = 1U,
            .encoder_pin_b = 2U,
            .first_led = 48U,
            .led_count = 10U,
        },
    };
};

#endif /* APP_SRC_BLOCKS_OSC_H_ */

/*
 * UIBLOCK.h
 *
 *  Created on: Mar 29, 2026
 *      Author: alax
 */

#ifndef APP_SRC_BLOCKS_UI_BLOCK_H_
#define APP_SRC_BLOCKS_UI_BLOCK_H_

#include <cstdint>
#include <array>

#include "Button.h"
#include "Knob.h"
#include "MIDI.h"
#include "InputController.h"
#include "LEDS.h"





template<uint8_t KNOB_COUNT, uint8_t BUTTON_COUNT, uint8_t PARAM_COUNT, uint8_t COUNT>
class UI_BLOCK {
public:



    void init(const std::array<Knob::Config, KNOB_COUNT> knob_settings,
              const std::array<Button::Config, BUTTON_COUNT>  button_config,
              MIDI *_midi, LEDSController &_leds, InputController &_mux) {
        midi = _midi;
        init_internal(knob_settings, button_config, _leds, _mux);
//        select_instance(current_instance);
    }

private:

    void init_internal(std::array<Knob::Config, KNOB_COUNT> knob_settings,
                       std::array<Button::Config, BUTTON_COUNT>  button_config,
                       LEDSController &_leds, InputController &_mux) {
        for (int i = 0; i < KNOB_COUNT; i++) {
            knob[i].init(_mux, knob_settings[i],_leds);
        }
        for (int i = 0; i < BUTTON_COUNT; i++) {
            sw[i].init(_mux, button_config[i], _leds);
        }
    }

    void update_knobs() {
        for (uint8_t i = 0; i < KNOB_COUNT; i++) {
            Knob::knob_msg ret = knob[i].update();
            if (ret.switch_changed) {
                bool state = !knob[i].get_state();
//                LOG::LOG0("%s %d encoder switch %d ", get_name(), current_instance, i);
//                LOG::LOG0( (!state) ? "pushed\r\n" : "released\r\n" );
                knob_sw_changed(i, state);

            }
            if (ret.value_changed) {
//                LOG::LOG0("%s %d knob %d changed %d\r\n", get_name(), current_instance, i, knob[i].get_knob_value());
                knob_val_changed(i, knob[i].get_knob_value());
            }
        }
    }

    virtual void knob_val_changed(uint8_t index, uint16_t value_scaled) {
//        set_current_preset_value(index, value_scaled);
//        knob[index].led_indicator_set_value(value_scaled);
//        midi->send_cc(get_current_instance_midi_nr(index), value_scaled, get_midi_ch());
    }

    virtual void knob_sw_changed(uint8_t index, bool state) {};


    std::array<Button, BUTTON_COUNT> sw;
    std::array<Knob, KNOB_COUNT> knob;

    MIDI *midi = nullptr;

};

#endif /* APP_SRC_BLOCKS_UI_BLOCK_H_ */

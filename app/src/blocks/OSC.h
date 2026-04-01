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


class OSC : public UI_BLOCK<OSC_KNOB_COUNT, OSC_BUTTON_COUNT, OSC_PARAM_COUNT, OSC_COUNT> {
public:


};

#endif /* APP_SRC_BLOCKS_OSC_H_ */

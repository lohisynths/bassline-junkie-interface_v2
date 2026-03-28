/*
 * InputController.cpp
 *
 *  Created on: Mar 27, 2026
 *      Author: alax
 */

#include "InputController.h"

#include <errno.h>

int InputController::init() {
    initialized_ = false;

    int ret = mux_.init();
    if (ret < 0) {
        return ret;
    }

    ret = gpio_.init();
    if (ret < 0) {
        return ret;
    }

    initialized_ = true;
    return 0;
}

int InputController::update() {
    if (!initialized_) {
        return -EACCES;
    }

    for (size_t i = 0; i < mux_count_; ++i) {
        int ret = mux_.read_state(i, &active_masks_[i]);
        if (ret < 0) {
            return ret;
        }
    }

    return gpio_.read_state(&active_masks_[gpio_mux_index_]);
}

int InputController::log_state() {
    if (!initialized_) {
        return -EACCES;
    }

    int ret = mux_.log_state_binary();
    if (ret < 0) {
        return ret;
    }

    return gpio_.log_state_binary();
}

uint16_t InputController::state(size_t state_index) const {
    if (state_index >= state_count) {
        return 0U;
    }

    return active_masks_[state_index];
}

/*
 * InputController.cpp
 *
 *  Created on: Mar 27, 2026
 *      Author: alax
 */

#include "InputController.h"

#include <errno.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(input_controller, LOG_LEVEL_INF);

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

    for (size_t i = 0U; i < mux_count_; ++i) {
        previous_mux_masks_[i] = 1U;
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

void InputController::log_mux_changes()
{
    for (size_t mux_index = 0U; mux_index < mux_count_; ++mux_index) {
        const uint16_t changed_mask = previous_mux_masks_[mux_index] ^ active_masks_[mux_index];
        if (changed_mask == 0U) {
            continue;
        }

        for (uint8_t bit = 0U; bit < 16U; ++bit) {
            const uint16_t bit_mask = (uint16_t)(1U << bit);
            if ((changed_mask & bit_mask) == 0U) {
                continue;
            }

            const bool active = (active_masks_[mux_index] & bit_mask) != 0U;
            LOG_INF("MUX%u bit %u changed to %u",
                    (unsigned int)mux_index,
                    (unsigned int)bit,
                    active ? 1U : 0U);
        }

        previous_mux_masks_[mux_index] = active_masks_[mux_index];
    }
}

/*
 * LEDS.cpp
 *
 *  Created on: Mar 26, 2026
 *      Author: alax
 */

#include "LEDS.h"
#include <stdio.h>

const LEDS::pca9685_controller LEDS::pca9685_controllers[] = {
    PCA9685_CTRL(pca9685_40, 0x40),
    PCA9685_CTRL(pca9685_41, 0x41),
    PCA9685_CTRL(pca9685_42, 0x42),
    PCA9685_CTRL(pca9685_43, 0x43),
    PCA9685_CTRL(pca9685_44, 0x44),
    PCA9685_CTRL(pca9685_45, 0x45),
    PCA9685_CTRL(pca9685_46, 0x46),
    PCA9685_CTRL(pca9685_47, 0x47),
    PCA9685_CTRL(pca9685_48, 0x48),
    PCA9685_CTRL(pca9685_49, 0x49),
    PCA9685_CTRL(pca9685_4a, 0x4A),
    PCA9685_CTRL(pca9685_4b, 0x4B),
    PCA9685_CTRL(pca9685_4c, 0x4C),
};

LEDS::LEDS() {
    report_pca9685_status();
    set_all_pca9685_channels(0);
}

LEDS::~LEDS() = default;

size_t LEDS::get_leds_count() {
    return ARRAY_SIZE(pca9685_controllers) * pca9685_channel_count;
}

void LEDS::set_all_pca9685_channels(uint32_t pulse) {
    for (size_t ctrl = 0; ctrl < ARRAY_SIZE(pca9685_controllers); ctrl++) {
        for (uint32_t channel = 0; channel < pca9685_channel_count; channel++) {
            (void)pwm_set(pca9685_controllers[ctrl].dev, channel,
                          pca9685_period, pulse, 0U);
        }
    }
}

void LEDS::report_pca9685_status(void) {
    for (size_t ctrl = 0; ctrl < ARRAY_SIZE(pca9685_controllers); ctrl++) {
        if (device_is_ready(pca9685_controllers[ctrl].dev)) {
            printf("PCA9685 ready at 0x%02X\r\n",
                   pca9685_controllers[ctrl].address);
        } else {
            printf("PCA9685 not ready at 0x%02X\r\n",
                   pca9685_controllers[ctrl].address);
        }
    }
}

void LEDS::run_pca9685_chase_step(size_t step) {
    const size_t controller_index = step / pca9685_channel_count;
    const uint32_t channel = step % pca9685_channel_count;

    set_all_pca9685_channels(0U);

    if (controller_index < ARRAY_SIZE(pca9685_controllers) &&
        device_is_ready(pca9685_controllers[controller_index].dev)) {
        (void)pwm_set(pca9685_controllers[controller_index].dev, channel,
                      pca9685_period, pca9685_period / 2U, 0U);
    }
}

void LEDS::set_one_pca9685_channel(uint8_t channel, uint32_t pulse) {
    if (channel >= get_leds_count()) {
        printf("Invalid PCA9685 channel %u\r\n", channel);
        return;
    }

    const size_t device = channel / pca9685_channel_count;
    const uint32_t channel_internal = channel % pca9685_channel_count;
    const int err = pwm_set(pca9685_controllers[device].dev, channel_internal,
                            pca9685_period, pulse, 0U);
    if (err != 0) {
        printf("Setting PWM value failed with %d error code.\r\n", err);
    }
}

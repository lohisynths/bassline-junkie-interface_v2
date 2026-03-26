/*
 * LEDS.cpp
 *
 *  Created on: Mar 26, 2026
 *      Author: alax
 */

#include "LEDS.h"
#include <errno.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(leds, LOG_LEVEL_INF);

#define PCA9685_CTRL(node_label)                           \
    {                                                      \
        .dev = DEVICE_DT_GET(DT_NODELABEL(node_label)),    \
        .address = DT_REG_ADDR(DT_NODELABEL(node_label)),  \
    }

const LEDS::pca9685_controller LEDS::pca9685_controllers[] = {
    PCA9685_CTRL(pca9685_40),
    PCA9685_CTRL(pca9685_41),
    PCA9685_CTRL(pca9685_42),
    PCA9685_CTRL(pca9685_43),
    PCA9685_CTRL(pca9685_44),
    PCA9685_CTRL(pca9685_45),
    PCA9685_CTRL(pca9685_46),
    PCA9685_CTRL(pca9685_47),
    PCA9685_CTRL(pca9685_48),
    PCA9685_CTRL(pca9685_49),
    PCA9685_CTRL(pca9685_4a),
    PCA9685_CTRL(pca9685_4b),
    PCA9685_CTRL(pca9685_4c),
};

const size_t LEDS::controller_count = ARRAY_SIZE(LEDS::pca9685_controllers);
const size_t LEDS::led_count = LEDS::controller_count * LEDS::pca9685_channel_count;

int LEDS::init() {
    initialized_ = false;

    const int status = report_status();
    if (status != 0) {
        return status;
    }

    initialized_ = true;

    const int err = clear_all();
    if (err != 0) {
        initialized_ = false;
    }

    return err;
}

int LEDS::clear_all() {
    int err = 0;

    if (!initialized_) {
        LOG_ERR("LED controller not initialized");
        return -EACCES;
    }

    for (size_t ctrl = 0; ctrl < controller_count; ctrl++) {
        for (uint32_t channel = 0; channel < pca9685_channel_count; channel++) {
            err = pwm_set(pca9685_controllers[ctrl].dev, channel, pca9685_period, 0U, 0U);
            if (err != 0) {
                LOG_ERR("Failed to clear controller 0x%02x channel %u: %d",
                        pca9685_controllers[ctrl].address, channel, err);
                return err;
            }
        }
    }

    return 0;
}

int LEDS::report_status() {
    int err = 0;

    for (size_t ctrl = 0; ctrl < controller_count; ctrl++) {
        if (device_is_ready(pca9685_controllers[ctrl].dev)) {
            LOG_INF("PCA9685 ready at 0x%02x",
                    pca9685_controllers[ctrl].address);
        } else {
            LOG_ERR("PCA9685 not ready at 0x%02x",
                    pca9685_controllers[ctrl].address);
            if (err == 0) {
                err = -ENODEV;
            }
        }
    }

    return err;
}

int LEDS::set_channel(size_t channel, uint32_t pulse) {
    int err = 0;

    if (!initialized_) {
        LOG_ERR("LED controller not initialized");
        return -EACCES;
    }

    if (channel >= led_count) {
        LOG_ERR("Invalid PCA9685 channel %u", (unsigned int)channel);
        return -EINVAL;
    }

    const size_t device = channel / pca9685_channel_count;
    const uint32_t channel_internal = channel % pca9685_channel_count;
    err = pwm_set(pca9685_controllers[device].dev, channel_internal, pca9685_period, pulse, 0U);
    if (err != 0) {
        LOG_ERR("Failed to set controller 0x%02x channel %u: %d",
                pca9685_controllers[device].address, channel_internal, err);
    }

    return err;
}

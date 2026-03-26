/*
 * MUX.cpp
 *
 *  Created on: Mar 26, 2026
 *      Author: alax
 */

#include "MUX.h"

#include <errno.h>
#include <zephyr/devicetree.h>
#include <zephyr/device.h>
#include <zephyr/drivers/mux/cd4067.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(mux, LOG_LEVEL_INF);

#define CD4067_MUX(node_label)                       \
    {                                                \
        .dev = DEVICE_DT_GET(DT_NODELABEL(node_label)), \
        .sig_pin = DT_GPIO_PIN(DT_NODELABEL(node_label), sig_gpios), \
    }

const MUX::mux_device MUX::mux_devices[] = {
    CD4067_MUX(cd4067_0),
    CD4067_MUX(cd4067_1),
    CD4067_MUX(cd4067_2),
    CD4067_MUX(cd4067_3),
};

const size_t MUX::mux_count = ARRAY_SIZE(MUX::mux_devices);

int MUX::init() {
    initialized_ = false;

    int err = 0;

    for (size_t i = 0; i < mux_count; ++i) {
        if (device_is_ready(mux_devices[i].dev)) {
            LOG_INF("CD4067 mux%u ready on SIG pin %u", (unsigned int)i,
                    (unsigned int)mux_devices[i].sig_pin);
        } else {
            LOG_ERR("CD4067 mux%u not ready on SIG pin %u", (unsigned int)i,
                    (unsigned int)mux_devices[i].sig_pin);
            if (err == 0) {
                err = -ENODEV;
            }
        }
    }

    if (err != 0) {
        return err;
    }

    initialized_ = true;
    return 0;
}

int MUX::read_state(size_t mux_index, uint16_t *active_mask) {
    if ((mux_index >= mux_count) || (active_mask == nullptr)) {
        return -EINVAL;
    }

    *active_mask = 0U;

    for (uint8_t channel = 0U; channel < CD4067_CHANNEL_COUNT; ++channel) {
        int value = 0;
        int ret = cd4067_set_channel(mux_devices[mux_index].dev, channel);

        if (ret < 0) {
            return ret;
        }

        ret = cd4067_read_raw(mux_devices[mux_index].dev, &value);
        if (ret < 0) {
            return ret;
        }

        if (value != 0) {
            *active_mask |= (uint16_t)(1U << channel);
        }
    }

    return 0;
}

int MUX::log_state() {
    if (!initialized_) {
        LOG_ERR("CD4067 devices not initialized");
        return -EACCES;
    }

    for (size_t i = 0; i < mux_count; ++i) {
        uint16_t active_mask = 0U;
        int ret = read_state(i, &active_mask);
        if (ret < 0) {
            return ret;
        }

        LOG_INF("CD4067 mux%u active mask: 0x%04x", (unsigned int)i, active_mask);
    }

    return 0;
}

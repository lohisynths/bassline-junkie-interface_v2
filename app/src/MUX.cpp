/*
 * MUX.cpp
 *
 *  Created on: Mar 26, 2026
 *      Author: alax
 */

#include "MUX.h"

#include <errno.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/mux/cd4067.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(mux, LOG_LEVEL_INF);

#define CD4067_NODE DT_NODELABEL(cd4067_0)

#if !DT_NODE_HAS_STATUS(CD4067_NODE, okay)
#error "This build expects a usable cd4067_0 devicetree node"
#endif

static const struct device *const mux = DEVICE_DT_GET(CD4067_NODE);

int MUX::init() {
    initialized_ = false;

    if (!device_is_ready(mux)) {
        LOG_ERR("CD4067 device is not ready");
        return -ENODEV;
    }

    initialized_ = true;
    return 0;
}

int MUX::log_state() {
    uint16_t active_mask = 0U;

    if (!initialized_) {
        LOG_ERR("CD4067 device not initialized");
        return -EACCES;
    }

    for (uint8_t channel = 0U; channel < CD4067_CHANNEL_COUNT; ++channel) {
        int value = 0;
        int ret = cd4067_set_channel(mux, channel);

        if (ret < 0) {
            return ret;
        }

        ret = cd4067_read_raw(mux, &value);
        if (ret < 0) {
            return ret;
        }

        if (value != 0) {
            active_mask |= (uint16_t)(1U << channel);
        }
    }

    LOG_INF("CD4067 active mask: 0x%04x", active_mask);

    return 0;
}

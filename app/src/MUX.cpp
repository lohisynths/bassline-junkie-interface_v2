/*
 * MUX.cpp
 *
 *  Created on: Mar 26, 2026
 *      Author: alax
 */

#include "MUX.h"
#include "utils.h"

#include <errno.h>
#include <zephyr/devicetree.h>
#include <zephyr/device.h>
#include <zephyr/drivers/mux/cd4067.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(MUX, LOG_LEVEL_INF);

#define CD4067_MUX(node_label)                                        \
    {                                                                 \
        .dev = DEVICE_DT_GET(DT_NODELABEL(node_label)),               \
        .sig = GPIO_DT_SPEC_GET(DT_NODELABEL(node_label), sig_gpios), \
    }

const MUX::mux_device MUX::mux_devices[] = {
    CD4067_MUX(cd4067_0),

    CD4067_MUX(cd4067_1),
    CD4067_MUX(cd4067_2),
    CD4067_MUX(cd4067_3),
    CD4067_MUX(cd4067_4),
};

int MUX::init() {
    initialized_ = false;

    int err = 0;

    for (size_t i = 0; i < mux_count; ++i) {
        if (device_is_ready(mux_devices[i].dev)) {
            LOG_INF("CD4067 mux%u ready on %s pin %u", (unsigned int)i,
                    mux_devices[i].sig.port->name,
                    (unsigned int)mux_devices[i].sig.pin);
        } else {
            LOG_ERR("CD4067 mux%u not ready on %s pin %u", (unsigned int)i,
                    mux_devices[i].sig.port->name,
                    (unsigned int)mux_devices[i].sig.pin);
            if (err == 0) {
                err = -ENODEV;
            }
        }
    }

    if (err != 0) {
        return err;
    }

    for (size_t i = 0U; i < mux_count; ++i) {
        previous_masks_[i] = 1U;
    }

    initialized_ = true;
    return 0;
}

int MUX::update() {
    if (!initialized_) {
        return -EACCES;
    }

    for (size_t i = 0U; i < mux_count; ++i) {
        const int ret = read_state(i, &active_masks_[i]);
        if (ret < 0) {
            return ret;
        }
    }

    return 0;
}

int MUX::read_state(size_t mux_index, uint16_t *active_mask) {
    if ((mux_index >= mux_count) || (active_mask == nullptr)) {
        return -EINVAL;
    }

    if (!initialized_) {
        LOG_ERR("CD4067 devices not initialized");
        return -EACCES;
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

int MUX::log_state_binary() {
    for (size_t i = 0; i < mux_count; ++i) {
        uint16_t active_mask = 0U;
        int ret = read_state(i, &active_mask);
        if (ret < 0) {
            return ret;
        }

        char binary_mask[17];
        mask_to_binary_string(active_mask, binary_mask, sizeof(binary_mask));
        LOG_INF("CD4067 mux%u active mask: 0b%s", (unsigned int)i, binary_mask);
    }

    return 0;
}

void MUX::log_mux_changes()
{
    for (size_t state_index = 0U; state_index < mux_count; ++state_index) {
        const uint16_t changed_mask = previous_masks_[state_index] ^ active_masks_[state_index];
        if (changed_mask == 0U) {
            continue;
        }

        for (uint8_t bit = 0U; bit < 16U; ++bit) {
            const uint16_t bit_mask = (uint16_t)(1U << bit);
            if ((changed_mask & bit_mask) == 0U) {
                continue;
            }

            const bool active = (active_masks_[state_index] & bit_mask) != 0U;
            LOG_INF("MUX %u bit %u changed to %u",
                    (unsigned int)state_index,
                    (unsigned int)bit,
                    active ? 1U : 0U);
        }

        previous_masks_[state_index] = active_masks_[state_index];
    }
}

uint16_t MUX::state(size_t state_index) const {
    if (state_index >= mux_count) {
        return 0U;
    }

    return active_masks_[state_index];
}

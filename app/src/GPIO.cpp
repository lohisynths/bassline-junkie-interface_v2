/*
 * GPIO.cpp
 *
 *  Created on: Mar 27, 2026
 *      Author: alax
 */

#include "GPIO.h"

#include <errno.h>
#include <zephyr/devicetree.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(app_gpio, LOG_LEVEL_INF);

#define GPIO_INPUT_PIN(node_label, signal_name)                    \
    {                                                              \
        .name = signal_name,                                       \
        .spec = GPIO_DT_SPEC_GET(DT_NODELABEL(node_label), gpios), \
    }

const GPIO::input_pin GPIO::input_pins[] = {
    GPIO_INPUT_PIN(enc13sw, "ENC13SW"),
    GPIO_INPUT_PIN(enc13b, "ENC13B"),
    GPIO_INPUT_PIN(enc13a, "ENC13A"),
    GPIO_INPUT_PIN(enc14sw, "ENC14SW"),
    GPIO_INPUT_PIN(enc14b, "ENC14B"),
    GPIO_INPUT_PIN(enc14a, "ENC14A"),
};

const size_t GPIO::input_count = ARRAY_SIZE(GPIO::input_pins);

int GPIO::init() {
    initialized_ = false;

    for (size_t i = 0; i < input_count; ++i) {
        const gpio_dt_spec &spec = input_pins[i].spec;

        if (!gpio_is_ready_dt(&spec)) {
            LOG_ERR("%s controller not ready on %s pin %u", input_pins[i].name,
                    spec.port->name, (unsigned int)spec.pin);
            return -ENODEV;
        }

        const int ret = gpio_pin_configure_dt(&spec, GPIO_INPUT);
        if (ret < 0) {
            LOG_ERR("Failed to configure %s on %s pin %u: %d", input_pins[i].name,
                    spec.port->name, (unsigned int)spec.pin, ret);
            return ret;
        }

        LOG_INF("%s ready on %s pin %u", input_pins[i].name, spec.port->name,
                (unsigned int)spec.pin);
    }

    initialized_ = true;
    return 0;
}

int GPIO::read_pin(size_t input_index, bool *active) {
    if ((input_index >= input_count) || (active == nullptr)) {
        return -EINVAL;
    }

    if (!initialized_) {
        LOG_ERR("GPIO inputs not initialized");
        return -EACCES;
    }

    const int value = gpio_pin_get_dt(&input_pins[input_index].spec);
    if (value < 0) {
        LOG_ERR("Failed to read %s: %d", input_pins[input_index].name, value);
        return value;
    }

    *active = (value != 0);
    return 0;
}

int GPIO::log_state() {
    bool states[input_count];
    uint8_t active_mask = 0U;

    for (size_t i = 0; i < input_count; ++i) {
        int ret = read_pin(i, &states[i]);
        if (ret < 0) {
            return ret;
        }

        if (states[i]) {
            active_mask |= (uint8_t)(1U << i);
        }
    }

    LOG_INF("GPIO active mask: 0x%02x", active_mask);

    return 0;
}

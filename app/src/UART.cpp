/*
 * UART.cpp
 *
 *  Created on: Mar 29, 2026
 *      Author: alax
 */

#include "UART.h"

#include <errno.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(app_uart, LOG_LEVEL_INF);

int UART::init() {
    initialized_ = false;

    if (!device_is_ready(dev_)) {
        LOG_ERR("USART1 device is not ready");
        return -ENODEV;
    }

    initialized_ = true;
    LOG_INF("USART1 ready on PA9/PA10 at 115200 baud");

    return 0;
}

bool UART::is_initialized() const {
    return initialized_;
}

int UART::write_byte(uint8_t byte) {
    if (!initialized_) {
        return -EACCES;
    }

    uart_poll_out(dev_, byte);
    return 0;
}

int UART::write(const uint8_t *data, size_t length) {
    if (!initialized_) {
        return -EACCES;
    }

    if ((data == nullptr) && (length != 0U)) {
        return -EINVAL;
    }

    for (size_t i = 0; i < length; ++i) {
        uart_poll_out(dev_, data[i]);
    }

    return 0;
}

int UART::write(const char *text) {
    if (!initialized_) {
        return -EACCES;
    }

    if (text == nullptr) {
        return -EINVAL;
    }

    while (*text != '\0') {
        uart_poll_out(dev_, (unsigned char)(*text));
        ++text;
    }

    return 0;
}

int UART::read_byte(uint8_t *byte) {
    if (!initialized_) {
        return -EACCES;
    }

    if (byte == nullptr) {
        return -EINVAL;
    }

    unsigned char value = 0U;
    const int ret = uart_poll_in(dev_, &value);
    if (ret == -1) {
        return -EAGAIN;
    }
    if (ret < 0) {
        return ret;
    }

    *byte = value;
    return 0;
}

int UART::read_available(uint8_t *buffer, size_t capacity, size_t *received) {
    if (!initialized_) {
        return -EACCES;
    }

    if (received == nullptr) {
        return -EINVAL;
    }

    *received = 0U;

    if ((capacity != 0U) && (buffer == nullptr)) {
        return -EINVAL;
    }

    while (*received < capacity) {
        const int ret = read_byte(&buffer[*received]);
        if (ret == -EAGAIN) {
            return 0;
        }
        if (ret < 0) {
            return ret;
        }

        (*received)++;
    }

    return 0;
}

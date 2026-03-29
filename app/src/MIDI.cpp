/*
 * MIDI.cpp
 *
 *  Created on: Mar 29, 2026
 *      Author: alax
 */

#include "MIDI.h"

#include <errno.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(midi, LOG_LEVEL_INF);

int MIDI::init(UART &uart, const char *name) {
    initialized_ = false;
    uart_ = nullptr;

    if (!uart.is_initialized()) {
        return -EACCES;
    }

    ARG_UNUSED(name);
    uart_ = &uart;
    initialized_ = true;

    LOG_INF("MIDI ready on UART transport");
    return 0;
}

bool MIDI::is_initialized() const {
    return initialized_;
}

ssize_t MIDI::write(const void *buffer, uint8_t length) {
    if (!initialized_ || (uart_ == nullptr)) {
        return -EACCES;
    }

    const uint8_t *ptr = static_cast<const uint8_t *>(buffer);
    if ((ptr == nullptr) && (length != 0U)) {
        return -EINVAL;
    }

    const uint8_t *start = ptr;
    const uint8_t *end = ptr + length;

    while (ptr != end) {
        const int ret = uart_->write_byte(*ptr);
        if (ret < 0) {
            return (ptr == start) ? ret : (ptr - start);
        }

        ++ptr;
    }

    return ptr - start;
}

int MIDI::send_note_on(uint8_t key, uint8_t velocity, uint8_t channel) {
    LOG_INF("NoteOn key:%u velocity:%u channel:%u",
            key & 0x7FU, velocity & 0x7FU, channel & 0x0FU);

    uint8_t msg[3];
    msg[0] = 0x90U | (channel & 0x0FU);
    msg[1] = key & 0x7FU;
    msg[2] = velocity & 0x7FU;

    const ssize_t written = write(msg, sizeof(msg));
    if (written < 0) {
        return (int)written;
    }

    return (written == (ssize_t)sizeof(msg)) ? 0 : -EIO;
}

int MIDI::send_note_off(uint8_t key, uint8_t velocity, uint8_t channel) {
    LOG_INF("NoteOff key:%u velocity:%u channel:%u",
            key & 0x7FU, velocity & 0x7FU, channel & 0x0FU);

    uint8_t msg[3];
    msg[0] = 0x80U | (channel & 0x0FU);
    msg[1] = key & 0x7FU;
    msg[2] = velocity & 0x7FU;

    const ssize_t written = write(msg, sizeof(msg));
    if (written < 0) {
        return (int)written;
    }

    return (written == (ssize_t)sizeof(msg)) ? 0 : -EIO;
}

int MIDI::send_cc(uint8_t control, uint8_t value, uint8_t channel) {
    LOG_INF("ControlChange control:%u value:%u channel:%u",
            control & 0x7FU, value & 0x7FU, channel & 0x0FU);

    uint8_t msg[3];
    msg[0] = 0xB0U | (channel & 0x0FU);
    msg[1] = control & 0x7FU;
    msg[2] = value & 0x7FU;

    const ssize_t written = write(msg, sizeof(msg));
    if (written < 0) {
        return (int)written;
    }

    return (written == (ssize_t)sizeof(msg)) ? 0 : -EIO;
}

/*
 * USB_MIDI.cpp
 *
 *  Created on: Apr 4, 2026
 *      Author: alax
 */

#include "USB_MIDI.h"
#include "usb_midi_init.h"

#include <errno.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(usb_midi, LOG_LEVEL_INF);

int USB_MIDI::init() {
    initialized_ = false;

    const int ret = usb_midi_device_init();
    if (ret < 0) {
        LOG_ERR("USB MIDI device init failed: %d", ret);
        return ret;
    }

    initialized_ = true;
    LOG_INF("USB MIDI ready");
    return 0;
}

bool USB_MIDI::is_initialized() const {
    return initialized_;
}

int USB_MIDI::receive(Message *msg) {
    if (!initialized_) {
        return -EACCES;
    }

    if (msg == nullptr) {
        return -EINVAL;
    }

    struct usb_midi_msg raw;
    const int ret = usb_midi_receive(&raw);
    if (ret < 0) {
        return ret;
    }

    msg->command = raw.command;
    msg->channel = raw.channel;
    msg->data1   = raw.data1;
    msg->data2   = raw.data2;

    return 0;
}

int USB_MIDI::send_note_on(uint8_t key, uint8_t velocity, uint8_t channel) {
    if (!initialized_) {
        return -EACCES;
    }

    LOG_INF("USB NoteOn key:%u velocity:%u channel:%u",
            key & 0x7FU, velocity & 0x7FU, channel & 0x0FU);

    const struct usb_midi_msg msg = {
        .command = 0x9U,
        .channel = static_cast<uint8_t>(channel & 0x0FU),
        .data1   = static_cast<uint8_t>(key & 0x7FU),
        .data2   = static_cast<uint8_t>(velocity & 0x7FU),
    };

    return usb_midi_send_msg(&msg);
}

int USB_MIDI::send_note_off(uint8_t key, uint8_t velocity, uint8_t channel) {
    if (!initialized_) {
        return -EACCES;
    }

    LOG_INF("USB NoteOff key:%u velocity:%u channel:%u",
            key & 0x7FU, velocity & 0x7FU, channel & 0x0FU);

    const struct usb_midi_msg msg = {
        .command = 0x8U,
        .channel = static_cast<uint8_t>(channel & 0x0FU),
        .data1   = static_cast<uint8_t>(key & 0x7FU),
        .data2   = static_cast<uint8_t>(velocity & 0x7FU),
    };

    return usb_midi_send_msg(&msg);
}

int USB_MIDI::send_cc(uint8_t control, uint8_t value, uint8_t channel) {
    if (!initialized_) {
        return -EACCES;
    }

    LOG_INF("USB CC control:%u value:%u channel:%u",
            control & 0x7FU, value & 0x7FU, channel & 0x0FU);

    const struct usb_midi_msg msg = {
        .command = 0xBU,
        .channel = static_cast<uint8_t>(channel & 0x0FU),
        .data1   = static_cast<uint8_t>(control & 0x7FU),
        .data2   = static_cast<uint8_t>(value & 0x7FU),
    };

    return usb_midi_send_msg(&msg);
}

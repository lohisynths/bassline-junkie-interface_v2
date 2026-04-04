/*
 * usb_midi_init.h
 *
 *  Created on: Apr 4, 2026
 *      Author: alax
 */

#ifndef SRC_USB_MIDI_INIT_H_
#define SRC_USB_MIDI_INIT_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Parsed MIDI channel voice message received from the USB host.
 */
struct usb_midi_msg {
    /** Upper nibble of the MIDI status byte (e.g. 0x9 for Note On). */
    uint8_t command;

    /** MIDI channel in the range 0..15. */
    uint8_t channel;

    /** First data byte (key number or controller number). */
    uint8_t data1;

    /** Second data byte (velocity or controller value). */
    uint8_t data2;
};

/**
 * @brief Initializes the USB device stack and enables the MIDI class.
 *
 * Configures string descriptors, a full-speed configuration, registers
 * the MIDI 2.0 class (with MIDI 1.0 channel voice support), sets up
 * receive callbacks, and enables the USB device controller.
 *
 * @retval 0 Initialization and enable completed successfully.
 * @retval negative Error code from the USB device stack.
 */
int usb_midi_device_init(void);

/**
 * @brief Dequeues the next received MIDI channel voice message, if any.
 *
 * @param msg Output message when data is available.
 *
 * @retval 0 One message was dequeued into @p msg.
 * @retval -EAGAIN No message is currently available.
 * @retval -EINVAL @p msg is null.
 */
int usb_midi_receive(struct usb_midi_msg *msg);

/**
 * @brief Sends one MIDI channel voice message to the USB host.
 *
 * @param msg Message to send.
 *
 * @retval 0 The message was sent successfully.
 * @retval -EINVAL @p msg is null.
 * @retval negative Error code from the USB MIDI class driver.
 */
int usb_midi_send_msg(const struct usb_midi_msg *msg);

#ifdef __cplusplus
}
#endif

#endif /* SRC_USB_MIDI_INIT_H_ */

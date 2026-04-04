/*
 * USB_MIDI.h
 *
 *  Created on: Apr 4, 2026
 *      Author: alax
 */

#ifndef SRC_USB_MIDI_H_
#define SRC_USB_MIDI_H_

#include <stdint.h>

/**
 * @brief Receives MIDI channel voice messages from a USB host and sends
 *        outbound MIDI messages back through the same USB connection.
 *
 * The class wraps Zephyr's USB MIDI 2.0 device class operating in MIDI 1.0
 * channel voice mode.  Inbound messages are buffered in an internal queue
 * and can be consumed one at a time with @ref receive.
 */
class USB_MIDI {
public:
    /** @brief Constructs an unbound USB MIDI facade. */
    USB_MIDI() = default;

    /**
     * @brief Parsed MIDI channel voice message.
     */
    struct Message {
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
     * @retval 0 Initialization completed successfully.
     * @retval negative Error code from the USB device stack.
     */
    int init();

    /**
     * @brief Reports whether @ref init completed successfully.
     *
     * @retval true The USB MIDI facade has been initialized.
     * @retval false The USB MIDI facade is not ready for use yet.
     */
    bool is_initialized() const;

    /**
     * @brief Dequeues the next received MIDI channel voice message.
     *
     * Call this from the main polling loop.  Returns immediately when no
     * message is available.
     *
     * @param msg Output message when data is available.
     *
     * @retval 0 One message was dequeued into @p msg.
     * @retval -EACCES The USB MIDI facade has not been initialized.
     * @retval -EAGAIN No message is currently available.
     * @retval -EINVAL @p msg is null.
     */
    int receive(Message *msg);

    /**
     * @brief Sends one MIDI Note On to the USB host.
     *
     * @param key MIDI key number.
     * @param velocity MIDI velocity value.
     * @param channel MIDI channel in the range `0..15`.
     *
     * @retval 0 The message was sent successfully.
     * @retval -EACCES The USB MIDI facade has not been initialized.
     * @retval negative Error propagated from the USB MIDI class driver.
     */
    int send_note_on(uint8_t key, uint8_t velocity, uint8_t channel);

    /**
     * @brief Sends one MIDI Note Off to the USB host.
     *
     * @param key MIDI key number.
     * @param velocity MIDI release velocity value.
     * @param channel MIDI channel in the range `0..15`.
     *
     * @retval 0 The message was sent successfully.
     * @retval -EACCES The USB MIDI facade has not been initialized.
     * @retval negative Error propagated from the USB MIDI class driver.
     */
    int send_note_off(uint8_t key, uint8_t velocity, uint8_t channel);

    /**
     * @brief Sends one MIDI Control Change to the USB host.
     *
     * @param control MIDI controller number.
     * @param value MIDI controller value.
     * @param channel MIDI channel in the range `0..15`.
     *
     * @retval 0 The message was sent successfully.
     * @retval -EACCES The USB MIDI facade has not been initialized.
     * @retval negative Error propagated from the USB MIDI class driver.
     */
    int send_cc(uint8_t control, uint8_t value, uint8_t channel);

private:
    /** @brief Tracks whether @ref init completed successfully. */
    bool initialized_ = false;
};

#endif /* SRC_USB_MIDI_H_ */

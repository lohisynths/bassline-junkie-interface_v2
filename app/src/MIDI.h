/*
 * MIDI.h
 *
 *  Created on: Mar 29, 2026
 *      Author: alax
 */

#ifndef SRC_MIDI_H_
#define SRC_MIDI_H_

#include "UART.h"

#include <sys/types.h>
#include <stdint.h>

/**
 * @brief Sends MIDI channel messages over one initialized UART transport.
 */
class MIDI {
public:
    /** @brief Constructs an unbound MIDI facade. */
    MIDI() = default;

    /**
     * @brief Binds this MIDI facade to one initialized UART transport.
     *
     * @param uart UART transport used for outbound MIDI bytes.
     * @param name Optional label reserved for API compatibility.
     *
     * @retval 0 Initialization completed successfully.
     * @retval -EINVAL @p name is null.
     * @retval -EACCES @p uart is not initialized yet.
     */
    int init(UART &uart, const char *name = "MIDI");

    /**
     * @brief Reports whether this MIDI facade is ready to send messages.
     *
     * @retval true The MIDI facade has been initialized.
     * @retval false The MIDI facade is not ready for use yet.
     */
    bool is_initialized() const;

    /**
     * @brief Sends one MIDI Note On channel message.
     *
     * @param key MIDI key number.
     * @param velocity MIDI velocity value.
     * @param channel MIDI channel in the range `0..15`.
     *
     * @retval 0 The message was sent successfully.
     * @retval negative Error propagated from @ref write.
     */
    int send_note_on(uint8_t key, uint8_t velocity, uint8_t channel);

    /**
     * @brief Sends one MIDI Note Off channel message.
     *
     * @param key MIDI key number.
     * @param velocity MIDI release velocity value.
     * @param channel MIDI channel in the range `0..15`.
     *
     * @retval 0 The message was sent successfully.
     * @retval negative Error propagated from @ref write.
     */
    int send_note_off(uint8_t key, uint8_t velocity, uint8_t channel);

    /**
     * @brief Sends one MIDI Control Change channel message.
     *
     * @param control MIDI controller number.
     * @param value MIDI controller value.
     * @param channel MIDI channel in the range `0..15`.
     *
     * @retval 0 The message was sent successfully.
     * @retval negative Error propagated from @ref write.
     */
    int send_cc(uint8_t control, uint8_t value, uint8_t channel);

private:
    /**
     * @brief Writes one raw MIDI byte sequence to the configured UART.
     *
     * @param buffer Source byte buffer.
     * @param length Number of bytes to send.
     *
     * @return Number of bytes written on success.
     * @retval -EACCES The MIDI facade has not been initialized.
     * @retval -EINVAL @p buffer is null for a non-zero @p length.
     * @retval negative Error propagated from @ref UART::write_byte.
     */
    ssize_t write(const void *buffer, uint8_t length);

    /** @brief Tracks whether @ref init completed successfully. */
    bool initialized_ = false;

    /** @brief UART transport used for outbound MIDI bytes. */
    UART *uart_ = nullptr;
};

#endif /* SRC_MIDI_H_ */

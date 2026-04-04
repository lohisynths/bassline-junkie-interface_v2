/**
 * @file PresetDumpRequestListener.h
 * @brief UART-side listener for DSP preset-dump requests.
 *
 * The DSP requests a full state resend by sending one reserved MIDI CC message over the shared UART link. This helper owns the small parser
 * state required to recognize that message and dispatch the request to the
 * preset controller.
 */

#ifndef SRC_PRESETDUMPREQUESTLISTENER_H_
#define SRC_PRESETDUMPREQUESTLISTENER_H_

#include <cstddef>
#include <cstdint>

class Preset;
class UART;

/**
 * @brief Listens for a reserved MIDI CC that requests a preset dump.
 *
 * The recognized request is `CC 127 = 127` on MIDI channel 16. The listener is
 * intentionally byte-oriented so it can be polled from the main loop without
 * introducing a full MIDI decoder just for this one transport-level command.
 */
class PresetDumpRequestListener {
public:
    /** @brief Temporary UART read buffer size used per poll. */
    static constexpr size_t uart_rx_chunk_size = 16U;

    /** @brief Zero-based MIDI channel carrying the preset-dump request. */
    static constexpr uint8_t preset_dump_request_channel = 15U;

    /** @brief CC number reserved for preset-dump requests. */
    static constexpr uint8_t preset_dump_request_control = 127U;

    /** @brief CC value that confirms the preset-dump request. */
    static constexpr uint8_t preset_dump_request_value = 127U;

    /** @brief Full MIDI status byte for the reserved preset-dump request CC. */
    static constexpr uint8_t preset_dump_request_status = 0xB0U | preset_dump_request_channel;

    /**
     * @brief Binds the listener to the shared UART and preset controller.
     *
     * @param uart   Shared UART carrying both control traffic and MIDI CC data.
     * @param preset Preset controller that should dump the active state on request.
     */
    void init(UART &uart, Preset &preset);

    /**
     * @brief Polls the UART and triggers a preset dump when the request CC arrives.
     *
     * This method is intended to be called from the main loop. It drains any
     * currently available UART bytes, advances the internal parser, and calls
     * the preset controller once the reserved request message is fully matched.
     */
    void poll();

private:
    /**
     * @brief Feeds one byte into the request parser.
     *
     * @param byte Byte read from the shared UART stream.
     * @retval true The request message has been fully received.
     * @retval false More data is needed or the byte was unrelated.
     */
    bool process_byte_(uint8_t byte);

    /** @brief Borrowed UART transport used to receive request bytes. */
    UART *uart_ = nullptr;

    /** @brief Borrowed preset controller invoked once a request is recognized. */
    Preset *preset_ = nullptr;

    /** @brief Last received MIDI status byte. */
    uint8_t status_ = 0U;

    /** @brief Number of data bytes collected for the current CC message. */
    uint8_t data_length_ = 0U;

    /** @brief Two-byte scratch buffer for CC number and CC value. */
    uint8_t data_[2] = {};
};

#endif /* SRC_PRESETDUMPREQUESTLISTENER_H_ */

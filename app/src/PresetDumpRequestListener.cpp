#include "PresetDumpRequestListener.h"

#include "Preset.h"
#include "UART.h"

#include <array>

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(PresetDumpRequestListener, LOG_LEVEL_INF);

void PresetDumpRequestListener::init(UART &uart, Preset &preset)
{
    uart_ = &uart;
    preset_ = &preset;
    status_ = 0U;
    data_length_ = 0U;
    data_[0] = 0U;
    data_[1] = 0U;
}

void PresetDumpRequestListener::poll()
{
    if ((uart_ == nullptr) || (preset_ == nullptr)) {
        return;
    }

    std::array<uint8_t, uart_rx_chunk_size> rx_buffer = {};
    size_t received = 0U;
    const int ret = uart_->read_available(rx_buffer.data(), rx_buffer.size(), &received);
    if (ret < 0) {
        LOG_WRN("Failed to read preset request UART: %d", ret);
        return;
    }

    for (size_t i = 0U; i < received; ++i) {
        LOG_INF("USART1 RX byte 0x%02x", rx_buffer[i]);
        if (process_byte_(rx_buffer[i])) {
            LOG_INF("Received preset dump request via MIDI CC");
            preset_->dump_active_slot();
        }
    }
}

bool PresetDumpRequestListener::process_byte_(uint8_t byte)
{
    if (byte & 0x80U) {
        status_ = byte;
        data_length_ = 0U;
        return false;
    }

    if (status_ != preset_dump_request_status) {
        return false;
    }

    if (data_length_ >= 2U) {
        data_length_ = 0U;
    }

    data_[data_length_] = byte & 0x7FU;
    ++data_length_;

    if (data_length_ != 2U) {
        return false;
    }

    data_length_ = 0U;
    return (data_[0] == preset_dump_request_control) &&
           (data_[1] == preset_dump_request_value);
}

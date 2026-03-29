/*
 * UART.h
 *
 *  Created on: Mar 29, 2026
 *      Author: alax
 */

#ifndef SRC_UART_H_
#define SRC_UART_H_

#include <zephyr/device.h>
#include <zephyr/devicetree.h>

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Wraps the app-owned USART1 transport exposed through Zephyr.
 */
class UART {
public:
    /** @brief Constructs a UART facade. */
    UART() = default;

    /**
     * @brief Verifies that the configured USART1 device is ready for use.
     *
     * @retval 0 Initialization completed successfully.
     * @retval -ENODEV The configured USART1 device is not ready.
     */
    int init();

    /**
     * @brief Reports whether @ref init completed successfully.
     *
     * @retval true The UART facade has been initialized.
     * @retval false The UART facade is not ready for use yet.
     */
    bool is_initialized() const;

    /**
     * @brief Writes one byte through USART1.
     *
     * @param byte Byte to send.
     *
     * @retval 0 The byte was sent successfully.
     * @retval -EACCES The UART facade has not been initialized.
     */
    int write_byte(uint8_t byte);

    /**
     * @brief Writes a raw byte buffer through USART1.
     *
     * @param data Buffer to send.
     * @param length Number of bytes to send from @p data.
     *
     * @retval 0 The buffer was sent successfully.
     * @retval -EACCES The UART facade has not been initialized.
     * @retval -EINVAL The buffer pointer is invalid for the requested length.
     */
    int write(const uint8_t *data, size_t length);

    /**
     * @brief Writes a null-terminated string through USART1.
     *
     * @param text String to send, excluding the trailing null terminator.
     *
     * @retval 0 The string was sent successfully.
     * @retval -EACCES The UART facade has not been initialized.
     * @retval -EINVAL @p text is null.
     */
    int write(const char *text);

    /**
     * @brief Reads one byte from USART1 if data is already available.
     *
     * @param byte Output byte when data is available.
     *
     * @retval 0 One byte was read successfully.
     * @retval -EACCES The UART facade has not been initialized.
     * @retval -EINVAL @p byte is null.
     * @retval -EAGAIN No byte is currently available.
     * @retval negative Driver-specific error returned by the UART driver.
     */
    int read_byte(uint8_t *byte);

    /**
     * @brief Reads all currently available bytes up to one caller buffer size.
     *
     * @param buffer Output byte buffer.
     * @param capacity Maximum number of bytes to store in @p buffer.
     * @param received Output number of bytes copied into @p buffer.
     *
     * @retval 0 The read completed successfully, including the case where no
     *         bytes were available and @p received becomes `0`.
     * @retval -EACCES The UART facade has not been initialized.
     * @retval -EINVAL One or more arguments are invalid.
     * @retval negative Driver-specific error returned by the UART driver.
     */
    int read_available(uint8_t *buffer, size_t capacity, size_t *received);

private:
    /** @brief Tracks whether @ref init completed successfully. */
    bool initialized_ = false;

    /** @brief Zephyr device handle for USART1. */
    const struct device *dev_ = DEVICE_DT_GET(DT_NODELABEL(usart1));
};

#endif /* SRC_UART_H_ */

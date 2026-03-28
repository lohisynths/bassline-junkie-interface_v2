#ifndef SRC_UTILS_H_
#define SRC_UTILS_H_

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Converts a 16-bit mask to a null-terminated binary string.
 *
 * @param mask Source bitmask.
 * @param buffer Destination character buffer.
 * @param buffer_size Size of @p buffer in bytes.
 */
void mask_to_binary_string(uint16_t mask, char *buffer, size_t buffer_size);

#endif /* SRC_UTILS_H_ */

#include "utils.h"

void mask_to_binary_string(uint16_t mask, char *buffer, size_t buffer_size)
{
    if (buffer_size < 17U) {
        return;
    }

    for (size_t bit = 0U; bit < 16U; ++bit) {
        const uint16_t bit_mask = (uint16_t)(1U << (15U - bit));
        buffer[bit] = ((mask & bit_mask) != 0U) ? '1' : '0';
    }

    buffer[16] = '\0';
}

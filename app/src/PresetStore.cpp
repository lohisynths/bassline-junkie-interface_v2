#include "PresetStore.h"

#include <errno.h>
#include <string.h>

#include <zephyr/logging/log.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/sys/crc.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(PresetStore, LOG_LEVEL_INF);

namespace {

constexpr uint32_t preset_magic = 0x31525042U;
constexpr uint16_t preset_format_version = 2U;
constexpr size_t valid_mask_size = PresetStore::preset_count / 8U;
constexpr size_t write_scratch_size = 256U;

struct PresetImage {
    uint32_t magic = preset_magic;
    uint16_t format_version = preset_format_version;
    uint16_t reserved = 0U;
    uint8_t valid_mask[valid_mask_size] = {};
    PresetSnapshot presets[PresetStore::preset_count] = {};
    uint32_t crc32 = 0U;
};

static PresetImage preset_image_cache = {};
static uint8_t flash_write_scratch[write_scratch_size] = {};

uint32_t image_crc_(const PresetImage &image)
{
    return crc32_ieee(reinterpret_cast<const uint8_t *>(&image),
                      offsetof(PresetImage, crc32));
}

void reset_image_cache_()
{
    preset_image_cache = {};
    preset_image_cache.magic = preset_magic;
    preset_image_cache.format_version = preset_format_version;
    preset_image_cache.crc32 = image_crc_(preset_image_cache);
}

bool validate_image_(const PresetImage &image)
{
    if ((image.magic != preset_magic) ||
        (image.format_version != preset_format_version)) {
        return false;
    }

    return image_crc_(image) == image.crc32;
}

bool slot_is_valid_(uint8_t index)
{
    const size_t byte_index = index / 8U;
    const uint8_t bit_mask = (uint8_t)(1U << (index % 8U));
    return (preset_image_cache.valid_mask[byte_index] & bit_mask) != 0U;
}

void set_slot_valid_(uint8_t index)
{
    const size_t byte_index = index / 8U;
    const uint8_t bit_mask = (uint8_t)(1U << (index % 8U));
    preset_image_cache.valid_mask[byte_index] |= bit_mask;
}

int write_image_(const struct flash_area *flash_area)
{
    const uint32_t alignment = flash_area_align(flash_area);
    if ((alignment == 0U) || (alignment > write_scratch_size)) {
        return -ENOTSUP;
    }

    const size_t chunk_size = write_scratch_size - (write_scratch_size % alignment);
    if (chunk_size == 0U) {
        return -ENOTSUP;
    }

    const uint8_t erased_value = flash_area_erased_val(flash_area);
    const uint8_t *image_bytes = reinterpret_cast<const uint8_t *>(&preset_image_cache);
    const size_t image_size = sizeof(preset_image_cache);
    const size_t padded_size = ROUND_UP(image_size, alignment);

    for (size_t offset = 0U; offset < padded_size; offset += chunk_size) {
        const size_t remaining = padded_size - offset;
        const size_t write_size = (remaining < chunk_size) ? remaining : chunk_size;
        const size_t available = (offset < image_size) ? (image_size - offset) : 0U;
        const size_t copy_size = (available < write_size) ? available : write_size;

        (void)memset(flash_write_scratch, erased_value, write_size);
        if (copy_size > 0U) {
            (void)memcpy(flash_write_scratch, image_bytes + offset, copy_size);
        }

        const int ret = flash_area_write(flash_area, (off_t)offset, flash_write_scratch, write_size);
        if (ret < 0) {
            return ret;
        }
    }

    return 0;
}

} // namespace

int PresetStore::init()
{
    initialized_ = false;
    flash_area_ = nullptr;

    int ret = flash_area_open(FIXED_PARTITION_ID(preset_partition), &flash_area_);
    if (ret < 0) {
        LOG_ERR("Failed to open preset partition: %d", ret);
        return ret;
    }

    if (!flash_area_device_is_ready(flash_area_)) {
        LOG_ERR("Preset partition device is not ready");
        return -ENODEV;
    }

    if (sizeof(PresetImage) > flash_area_->fa_size) {
        LOG_ERR("Preset image too large for partition: %u > %u",
                (unsigned int)sizeof(PresetImage),
                (unsigned int)flash_area_->fa_size);
        return -ENOSPC;
    }

    PresetImage image = {};
    ret = flash_area_read(flash_area_, 0, &image, sizeof(image));
    if (ret < 0) {
        LOG_ERR("Failed to read preset partition: %d", ret);
        return ret;
    }

    if (validate_image_(image)) {
        preset_image_cache = image;
        LOG_INF("Loaded preset image from flash");
    } else {
        reset_image_cache_();
        LOG_WRN("Preset image missing or invalid, using defaults");
    }

    initialized_ = true;
    return 0;
}

int PresetStore::load_preset(uint8_t index,
                             PresetSnapshot &snapshot,
                             bool &slot_was_saved) const
{
    if (!initialized_) {
        return -EACCES;
    }

    if (index >= preset_count) {
        return -EINVAL;
    }

    if (slot_is_valid_(index)) {
        snapshot = preset_image_cache.presets[index];
        slot_was_saved = true;
    } else {
        snapshot = default_preset_snapshot();
        slot_was_saved = false;
    }

    return 0;
}

int PresetStore::save_preset(uint8_t index, const PresetSnapshot &snapshot)
{
    if (!initialized_) {
        return -EACCES;
    }

    if (index >= preset_count) {
        return -EINVAL;
    }

    preset_image_cache.magic = preset_magic;
    preset_image_cache.format_version = preset_format_version;
    preset_image_cache.presets[index] = snapshot;
    set_slot_valid_(index);
    preset_image_cache.crc32 = image_crc_(preset_image_cache);

    int ret = flash_area_erase(flash_area_, 0, flash_area_->fa_size);
    if (ret < 0) {
        LOG_ERR("Failed to erase preset partition: %d", ret);
        return ret;
    }

    ret = write_image_(flash_area_);
    if (ret < 0) {
        LOG_ERR("Failed to write preset partition: %d", ret);
        return ret;
    }

    return 0;
}

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
constexpr uint16_t preset_format_version = 3U;
constexpr size_t valid_mask_size = PresetStore::preset_count / 8U;
constexpr size_t write_scratch_size = 256U;

struct PresetCache {
    uint8_t valid_mask[valid_mask_size] = {};
    PresetSnapshot presets[PresetStore::preset_count] = {};
};

struct PresetRecord {
    uint32_t magic = preset_magic;
    uint16_t format_version = preset_format_version;
    uint8_t preset_index = 0U;
    uint8_t reserved = 0U;
    PresetSnapshot snapshot = {};
    uint32_t crc32 = 0U;
};

static PresetCache preset_cache = {};
static uint8_t flash_write_scratch[write_scratch_size] = {};
static size_t next_record_offset = 0U;
static bool needs_compaction = false;

uint32_t record_crc_(const PresetRecord &record)
{
    return crc32_ieee(reinterpret_cast<const uint8_t *>(&record),
                      offsetof(PresetRecord, crc32));
}

void reset_cache_()
{
    preset_cache = {};
    next_record_offset = 0U;
    needs_compaction = false;
}

bool validate_record_(const PresetRecord &record)
{
    if ((record.magic != preset_magic) ||
        (record.format_version != preset_format_version) ||
        (record.preset_index >= PresetStore::preset_count)) {
        return false;
    }

    return record_crc_(record) == record.crc32;
}

bool slot_is_valid_(uint8_t index)
{
    const size_t byte_index = index / 8U;
    const uint8_t bit_mask = (uint8_t)(1U << (index % 8U));
    return (preset_cache.valid_mask[byte_index] & bit_mask) != 0U;
}

void set_slot_valid_(uint8_t index)
{
    const size_t byte_index = index / 8U;
    const uint8_t bit_mask = (uint8_t)(1U << (index % 8U));
    preset_cache.valid_mask[byte_index] |= bit_mask;
}

size_t record_storage_size_(const struct flash_area *flash_area)
{
    return ROUND_UP(sizeof(PresetRecord), flash_area_align(flash_area));
}

bool is_erased_record_(const PresetRecord &record, uint8_t erased_value)
{
    const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&record);
    for (size_t i = 0U; i < sizeof(record); ++i) {
        if (bytes[i] != erased_value) {
            return false;
        }
    }

    return true;
}

int write_record_(const struct flash_area *flash_area,
                  size_t offset,
                  const PresetRecord &record)
{
    const uint32_t alignment = flash_area_align(flash_area);
    const size_t storage_size = record_storage_size_(flash_area);
    if ((alignment == 0U) ||
        (alignment > write_scratch_size) ||
        (storage_size > write_scratch_size)) {
        return -ENOTSUP;
    }

    (void)memset(flash_write_scratch, flash_area_erased_val(flash_area), storage_size);
    (void)memcpy(flash_write_scratch, &record, sizeof(record));

    return flash_area_write(flash_area,
                            (off_t)offset,
                            flash_write_scratch,
                            storage_size);
}

int append_record_(const struct flash_area *flash_area,
                   uint8_t index,
                   const PresetSnapshot &snapshot)
{
    PresetRecord record = {};
    record.magic = preset_magic;
    record.format_version = preset_format_version;
    record.preset_index = index;
    record.snapshot = snapshot;
    record.crc32 = record_crc_(record);

    const int ret = write_record_(flash_area, next_record_offset, record);
    if (ret < 0) {
        return ret;
    }

    next_record_offset += record_storage_size_(flash_area);
    return 0;
}

int compact_log_(const struct flash_area *flash_area)
{
    int ret = flash_area_erase(flash_area, 0, flash_area->fa_size);
    if (ret < 0) {
        return ret;
    }

    next_record_offset = 0U;
    needs_compaction = false;

    for (uint16_t index = 0U; index < PresetStore::preset_count; ++index) {
        if (!slot_is_valid_((uint8_t)index)) {
            continue;
        }

        ret = append_record_(flash_area,
                             (uint8_t)index,
                             preset_cache.presets[index]);
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

    const size_t storage_size = record_storage_size_(flash_area_);
    if ((storage_size == 0U) || (storage_size > flash_area_->fa_size)) {
        LOG_ERR("Preset record size is invalid for partition");
        return -ENOSPC;
    }

    reset_cache_();

    const uint8_t erased_value = flash_area_erased_val(flash_area_);
    const size_t max_offset = flash_area_->fa_size - storage_size;

    for (size_t offset = 0U; offset <= max_offset; offset += storage_size) {
        PresetRecord record = {};

        ret = flash_area_read(flash_area_, (off_t)offset, &record, sizeof(record));
        if (ret < 0) {
            LOG_ERR("Failed to read preset record at offset %u: %d",
                    (unsigned int)offset,
                    ret);
            return ret;
        }

        if (is_erased_record_(record, erased_value)) {
            next_record_offset = offset;
            initialized_ = true;
            LOG_INF("Loaded preset log from flash");
            return 0;
        }

        if (!validate_record_(record)) {
            needs_compaction = true;
            next_record_offset = flash_area_->fa_size;
            LOG_WRN("Preset log contains invalid data, compacting on next save");
            initialized_ = true;
            return 0;
        }

        preset_cache.presets[record.preset_index] = record.snapshot;
        set_slot_valid_(record.preset_index);
    }

    next_record_offset = flash_area_->fa_size;
    needs_compaction = true;
    LOG_INF("Preset log is full, compaction will run on next save");
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
        snapshot = preset_cache.presets[index];
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

    preset_cache.presets[index] = snapshot;
    set_slot_valid_(index);

    const size_t storage_size = record_storage_size_(flash_area_);
    const bool has_space = (next_record_offset + storage_size) <= flash_area_->fa_size;

    int ret = 0;
    if (needs_compaction || !has_space) {
        ret = compact_log_(flash_area_);
        if (ret < 0) {
            LOG_ERR("Failed to compact preset log: %d", ret);
            return ret;
        }
    }

    ret = append_record_(flash_area_, index, snapshot);
    if (ret < 0) {
        LOG_ERR("Failed to append preset record: %d", ret);
        return ret;
    }

    return 0;
}

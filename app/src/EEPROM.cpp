#include "EEPROM.h"

#include <errno.h>
#include <stddef.h>
#include <string.h>

#include <array>

#include <zephyr/logging/log.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/sys/crc.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(EEPROM, LOG_LEVEL_INF);

namespace {

constexpr uint32_t record_magic = 0x42535052U;
constexpr uint16_t record_version = PresetSnapshot::version;
constexpr uint8_t record_kind_preset = 0U;
constexpr uint8_t record_kind_startup_slot = 1U;
constexpr size_t record_header_size = sizeof(uint32_t) + sizeof(uint16_t) + sizeof(uint8_t) +
                                      sizeof(uint8_t) + sizeof(uint8_t);
constexpr size_t record_prefix_size = record_header_size + sizeof(PresetSnapshot);
constexpr size_t record_startup_prefix_size = record_header_size;
constexpr size_t min_record_storage_size = record_prefix_size + sizeof(uint32_t);
constexpr size_t max_record_storage_size = 1024U;

struct SlotCache {
    std::array<PresetSnapshot, EEPROM::preset_count> snapshots = {};
    std::array<uint8_t, EEPROM::preset_count / 8U> valid_mask = {};
    uint8_t startup_slot = 0U;
    bool startup_slot_valid = false;
};

static SlotCache cache = {};
static size_t next_record_offset = 0U;
static bool needs_compaction = false;

bool slot_is_valid(uint8_t index)
{
    const size_t byte_index = index / 8U;
    const uint8_t bit_mask = static_cast<uint8_t>(1U << (index % 8U));
    return (cache.valid_mask[byte_index] & bit_mask) != 0U;
}

void set_slot_valid(uint8_t index)
{
    const size_t byte_index = index / 8U;
    const uint8_t bit_mask = static_cast<uint8_t>(1U << (index % 8U));
    cache.valid_mask[byte_index] |= bit_mask;
}

void clear_cache()
{
    cache = {};
    next_record_offset = 0U;
    needs_compaction = false;
}

size_t record_storage_size(const struct flash_area *flash_area)
{
    return ROUND_UP(min_record_storage_size, flash_area_align(flash_area));
}

bool is_erased_bytes(const uint8_t *bytes, size_t length, uint8_t erased_value)
{
    for (size_t i = 0U; i < length; ++i) {
        if (bytes[i] != erased_value) {
            return false;
        }
    }

    return true;
}

uint32_t record_crc(const uint8_t *bytes, size_t length)
{
    return crc32_ieee(bytes, length);
}

bool read_record(const struct flash_area *flash_area,
                 size_t offset,
                 size_t storage_size,
                 uint8_t erased_value,
                 uint8_t *buffer)
{
    if (flash_area_read(flash_area, static_cast<off_t>(offset), buffer, storage_size) < 0) {
        return false;
    }

    return !is_erased_bytes(buffer, storage_size, erased_value);
}

bool validate_record(const uint8_t *buffer, size_t storage_size, uint8_t &kind, uint8_t &slot)
{
    uint32_t magic = 0U;
    uint16_t version = 0U;
    uint8_t read_kind = 0U;
    uint8_t read_slot = 0U;
    uint8_t reserved = 0U;
    uint32_t stored_crc = 0U;

    size_t offset = 0U;
    memcpy(&magic, buffer + offset, sizeof(magic));
    offset += sizeof(magic);
    memcpy(&version, buffer + offset, sizeof(version));
    offset += sizeof(version);
    memcpy(&read_kind, buffer + offset, sizeof(read_kind));
    offset += sizeof(read_kind);
    memcpy(&read_slot, buffer + offset, sizeof(read_slot));
    offset += sizeof(read_slot);
    memcpy(&reserved, buffer + offset, sizeof(reserved));
    offset += sizeof(reserved);

    if ((magic != record_magic) || (version != record_version) ||
        (read_slot >= EEPROM::preset_count)) {
        return false;
    }

    const size_t crc_offset = (read_kind == record_kind_preset)
        ? record_prefix_size
        : (read_kind == record_kind_startup_slot)
              ? record_startup_prefix_size
              : 0U;
    if ((crc_offset == 0U) || (reserved != 0U) ||
        (storage_size < (crc_offset + sizeof(stored_crc)))) {
        return false;
    }
    memcpy(&stored_crc, buffer + crc_offset, sizeof(stored_crc));

    kind = read_kind;
    slot = read_slot;

    const uint32_t computed_crc = record_crc(buffer, crc_offset);
    return computed_crc == stored_crc;
}

void write_record_header(uint8_t *buffer, uint8_t kind, uint8_t slot, size_t &write_offset)
{
    const uint32_t magic = record_magic;
    const uint16_t version = record_version;
    const uint8_t reserved = 0U;

    memcpy(buffer + write_offset, &magic, sizeof(magic));
    write_offset += sizeof(magic);
    memcpy(buffer + write_offset, &version, sizeof(version));
    write_offset += sizeof(version);
    memcpy(buffer + write_offset, &kind, sizeof(kind));
    write_offset += sizeof(kind);
    memcpy(buffer + write_offset, &slot, sizeof(slot));
    write_offset += sizeof(slot);
    memcpy(buffer + write_offset, &reserved, sizeof(reserved));
    write_offset += sizeof(reserved);
}

void unpack_snapshot(const uint8_t *buffer, PresetSnapshot &snapshot)
{
    const size_t offset = record_header_size;
    memcpy(&snapshot, buffer + offset, sizeof(PresetSnapshot));
}

int write_preset_record(const struct flash_area *flash_area,
                        size_t offset,
                        uint8_t slot,
                        const PresetSnapshot &snapshot)
{
    const size_t storage_size = record_storage_size(flash_area);
    if (storage_size > max_record_storage_size) {
        return -ENOTSUP;
    }

    std::array<uint8_t, max_record_storage_size> buffer = {};
    buffer.fill(flash_area_erased_val(flash_area));

    size_t write_offset = 0U;
    const uint8_t kind = record_kind_preset;
    write_record_header(buffer.data(), kind, slot, write_offset);
    memcpy(buffer.data() + write_offset, &snapshot, sizeof(snapshot));

    const uint32_t crc = record_crc(buffer.data(), record_prefix_size);
    memcpy(buffer.data() + record_prefix_size, &crc, sizeof(crc));

    return flash_area_write(flash_area,
                            static_cast<off_t>(offset),
                            buffer.data(),
                            storage_size);
}

int write_startup_record(const struct flash_area *flash_area, size_t offset, uint8_t slot)
{
    const size_t storage_size = record_storage_size(flash_area);
    if (storage_size > max_record_storage_size) {
        return -ENOTSUP;
    }

    std::array<uint8_t, max_record_storage_size> buffer = {};
    buffer.fill(flash_area_erased_val(flash_area));

    size_t write_offset = 0U;
    const uint8_t kind = record_kind_startup_slot;
    write_record_header(buffer.data(), kind, slot, write_offset);

    const uint32_t crc = record_crc(buffer.data(), record_startup_prefix_size);
    memcpy(buffer.data() + record_startup_prefix_size, &crc, sizeof(crc));

    return flash_area_write(flash_area,
                            static_cast<off_t>(offset),
                            buffer.data(),
                            storage_size);
}

int compact_log(const struct flash_area *flash_area)
{
    int ret = flash_area_erase(flash_area, 0, flash_area->fa_size);
    if (ret < 0) {
        return ret;
    }

    const size_t storage_size = record_storage_size(flash_area);
    size_t offset = 0U;

    for (uint8_t slot = 0U; slot < EEPROM::preset_count; ++slot) {
        if (!slot_is_valid(slot)) {
            continue;
        }

        ret = write_preset_record(flash_area, offset, slot, cache.snapshots[slot]);
        if (ret < 0) {
            return ret;
        }

        offset += storage_size;
    }

    if (cache.startup_slot_valid) {
        ret = write_startup_record(flash_area, offset, cache.startup_slot);
        if (ret < 0) {
            return ret;
        }

        offset += storage_size;
    }

    next_record_offset = offset;
    needs_compaction = false;
    return 0;
}

int ensure_append_space(const struct flash_area *flash_area, size_t storage_size)
{
    const bool has_space = (next_record_offset + storage_size) <= flash_area->fa_size;
    if (!needs_compaction && has_space) {
        return 0;
    }

    const int ret = compact_log(flash_area);
    if (ret < 0) {
        LOG_ERR("Failed to compact preset log: %d", ret);
    }
    return ret;
}

} // namespace

int EEPROM::init()
{
    initialized_ = false;
    flash_area_ = nullptr;

    int ret = flash_area_open(FIXED_PARTITION_ID(preset_partition), &flash_area_);
    if (ret < 0) {
        LOG_ERR("Failed to open preset partition: %d", ret);
        clear_cache();
        return ret;
    }

    if (!flash_area_device_is_ready(flash_area_)) {
        LOG_ERR("Preset partition device is not ready");
        clear_cache();
        return -ENODEV;
    }

    const size_t storage_size = record_storage_size(flash_area_);
    if ((storage_size == 0U) || (storage_size > max_record_storage_size) ||
        (storage_size * (EEPROM::preset_count + 1U) > flash_area_->fa_size)) {
        LOG_ERR("Preset record size is invalid for partition");
        clear_cache();
        return -ENOSPC;
    }

    clear_cache();

    std::array<uint8_t, max_record_storage_size> buffer = {};
    const uint8_t erased_value = flash_area_erased_val(flash_area_);

    for (size_t offset = 0U; (offset + storage_size) <= flash_area_->fa_size; offset += storage_size) {
        if (!read_record(flash_area_, offset, storage_size, erased_value, buffer.data())) {
            continue;
        }

        uint8_t kind = 0U;
        uint8_t slot = 0U;
        if (!validate_record(buffer.data(), storage_size, kind, slot)) {
            needs_compaction = true;
            continue;
        }

        if (kind == record_kind_preset) {
            unpack_snapshot(buffer.data(), cache.snapshots[slot]);
            set_slot_valid(slot);
        } else if (kind == record_kind_startup_slot) {
            cache.startup_slot = slot;
            cache.startup_slot_valid = true;
        }
        next_record_offset = offset + storage_size;
    }

    initialized_ = true;
    LOG_INF("Loaded preset log from flash");
    return 0;
}

int EEPROM::load(uint8_t index, PresetSnapshot &snapshot) const
{
    if (index >= EEPROM::preset_count) {
        return -EINVAL;
    }

    if (!initialized_ || !slot_is_valid(index)) {
        snapshot = default_preset_snapshot();
        return 0;
    }

    snapshot = cache.snapshots[index];
    return 0;
}

int EEPROM::save(uint8_t index, const PresetSnapshot &snapshot)
{
    if (index >= EEPROM::preset_count) {
        return -EINVAL;
    }

    if (!initialized_) {
        return -EACCES;
    }

    const size_t storage_size = record_storage_size(flash_area_);
    const int ret = ensure_append_space(flash_area_, storage_size);
    if (ret < 0) {
        return ret;
    }

    const int write_ret = write_preset_record(flash_area_, next_record_offset, index, snapshot);
    if (write_ret < 0) {
        LOG_ERR("Failed to append preset record: %d", write_ret);
        return write_ret;
    }

    cache.snapshots[index] = snapshot;
    set_slot_valid(index);
    next_record_offset += storage_size;
    return 0;
}

int EEPROM::load_startup_slot(uint8_t &slot) const
{
    slot = cache.startup_slot_valid ? cache.startup_slot : 0U;
    if (cache.startup_slot_valid) {
        LOG_INF("Startup preset slot %u loaded from flash", static_cast<unsigned int>(slot));
    } else {
        LOG_INF("Startup preset slot not stored yet, using preset 0");
    }
    return 0;
}

int EEPROM::save_startup_slot(uint8_t slot)
{
    if (slot >= EEPROM::preset_count) {
        return -EINVAL;
    }

    if (!initialized_) {
        return -EACCES;
    }

    if (cache.startup_slot_valid && (cache.startup_slot == slot)) {
        return 0;
    }

    const size_t storage_size = record_storage_size(flash_area_);
    const int ret = ensure_append_space(flash_area_, storage_size);
    if (ret < 0) {
        return ret;
    }

    const int write_ret = write_startup_record(flash_area_, next_record_offset, slot);
    if (write_ret < 0) {
        LOG_ERR("Failed to append startup slot record: %d", write_ret);
        return write_ret;
    }

    cache.startup_slot = slot;
    cache.startup_slot_valid = true;
    next_record_offset += storage_size;
    return 0;
}

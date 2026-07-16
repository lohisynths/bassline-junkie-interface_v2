#include "PresetStorage.h"

#include "SDCard.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <array>
#include <type_traits>

#include <zephyr/fs/fs.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/crc.h>

LOG_MODULE_REGISTER(PresetStorage, LOG_LEVEL_INF);

namespace {

constexpr uint32_t preset_magic = 0x42535052U; // "RPSB" in little-endian byte order.
constexpr uint32_t startup_magic = 0x42535452U;
constexpr uint16_t format_version = PresetSnapshot::version;
constexpr size_t preset_header_size = 8U;
constexpr size_t startup_prefix_size = 8U;
constexpr size_t preset_prefix_size = preset_header_size + sizeof(PresetSnapshot);
constexpr size_t preset_record_size = preset_prefix_size + sizeof(uint32_t);
constexpr size_t startup_record_size = startup_prefix_size + sizeof(uint32_t);
constexpr const char *startup_path = "/SD:/presets/startup.bin";
constexpr const char *startup_tmp_path = "/SD:/presets/startup.tmp";

static_assert(std::is_trivially_copyable_v<PresetSnapshot>);

void put_u16_le(uint8_t *output, uint16_t value)
{
    output[0] = static_cast<uint8_t>(value);
    output[1] = static_cast<uint8_t>(value >> 8U);
}

void put_u32_le(uint8_t *output, uint32_t value)
{
    output[0] = static_cast<uint8_t>(value);
    output[1] = static_cast<uint8_t>(value >> 8U);
    output[2] = static_cast<uint8_t>(value >> 16U);
    output[3] = static_cast<uint8_t>(value >> 24U);
}

uint16_t get_u16_le(const uint8_t *input)
{
    return static_cast<uint16_t>(input[0]) |
           static_cast<uint16_t>(static_cast<uint16_t>(input[1]) << 8U);
}

uint32_t get_u32_le(const uint8_t *input)
{
    return static_cast<uint32_t>(input[0]) |
           (static_cast<uint32_t>(input[1]) << 8U) |
           (static_cast<uint32_t>(input[2]) << 16U) |
           (static_cast<uint32_t>(input[3]) << 24U);
}

void slot_path(uint8_t slot, bool temporary, char *path, size_t path_size)
{
    (void)snprintf(path, path_size, "%s/slot-%03u.%s", PresetStorage::preset_directory,
                   static_cast<unsigned int>(slot), temporary ? "tmp" : "bin");
}

PresetLoadResult read_exact_file(const char *path, uint8_t *buffer, size_t expected_size)
{
    struct fs_dirent entry = {};
    const int stat_ret = fs_stat(path, &entry);
    if (stat_ret == -ENOENT) {
        return PresetLoadResult::not_found;
    }
    if (stat_ret < 0) {
        LOG_ERR("Failed to stat %s: %d", path, stat_ret);
        return PresetLoadResult::io_error;
    }
    if ((entry.type != FS_DIR_ENTRY_FILE) || (entry.size != expected_size)) {
        return PresetLoadResult::incompatible;
    }

    struct fs_file_t file;
    fs_file_t_init(&file);
    int ret = fs_open(&file, path, FS_O_READ);
    if (ret < 0) {
        LOG_ERR("Failed to open %s: %d", path, ret);
        return PresetLoadResult::io_error;
    }

    const ssize_t read = fs_read(&file, buffer, expected_size);
    uint8_t extra = 0U;
    const ssize_t extra_read = (read == static_cast<ssize_t>(expected_size))
        ? fs_read(&file, &extra, sizeof(extra)) : 0;
    const int close_ret = fs_close(&file);
    if ((read < 0) || (extra_read < 0) || (close_ret < 0)) {
        LOG_ERR("Failed to read %s", path);
        return PresetLoadResult::io_error;
    }
    if ((read != static_cast<ssize_t>(expected_size)) || (extra_read != 0)) {
        return PresetLoadResult::incompatible;
    }
    return PresetLoadResult::loaded;
}

int unlink_if_exists(const char *path)
{
    struct fs_dirent entry = {};
    const int stat_ret = fs_stat(path, &entry);
    if (stat_ret == -ENOENT) {
        return 0;
    }
    if (stat_ret < 0) {
        return stat_ret;
    }
    return fs_unlink(path);
}

int replace_file(const char *temporary_path, const char *path, const uint8_t *data, size_t size)
{
    int ret = unlink_if_exists(temporary_path);
    if (ret < 0) {
        return ret;
    }
    struct fs_file_t file;
    fs_file_t_init(&file);
    ret = fs_open(&file, temporary_path, FS_O_CREATE | FS_O_WRITE | FS_O_TRUNC);
    if (ret < 0) {
        return ret;
    }

    const ssize_t written = fs_write(&file, data, size);
    const int close_ret = fs_close(&file);
    if (written != static_cast<ssize_t>(size)) {
        ret = (written < 0) ? static_cast<int>(written) : -EIO;
    } else if (close_ret < 0) {
        ret = close_ret;
    } else {
        ret = fs_rename(temporary_path, path);
    }
    if (ret < 0) {
        (void)unlink_if_exists(temporary_path);
    }
    return ret;
}

} // namespace

int PresetStorage::init(SDCard &sd_card)
{
    initialized_ = false;
    if (!sd_card.is_mounted()) {
        return -ENODEV;
    }
    const int ret = fs_mkdir(preset_directory);
    if ((ret < 0) && (ret != -EEXIST)) {
        LOG_ERR("Failed to create %s: %d", preset_directory, ret);
        return ret;
    }
    initialized_ = true;
    return 0;
}

PresetLoadResult PresetStorage::load(uint8_t slot, PresetSnapshot &snapshot) const
{
    snapshot = default_preset_snapshot();
    if (!initialized_ || (slot >= preset_count)) {
        return PresetLoadResult::io_error;
    }

    char path[40] = {};
    slot_path(slot, false, path, sizeof(path));
    std::array<uint8_t, preset_record_size> record = {};
    const PresetLoadResult read_result = read_exact_file(path, record.data(), record.size());
    if (read_result != PresetLoadResult::loaded) {
        return read_result;
    }

    const bool header_valid = (get_u32_le(record.data()) == preset_magic) &&
        (get_u16_le(record.data() + 4U) == format_version) &&
        (get_u16_le(record.data() + 6U) == sizeof(PresetSnapshot));
    const uint32_t stored_crc = get_u32_le(record.data() + preset_prefix_size);
    if (!header_valid || (stored_crc != crc32_ieee(record.data(), preset_prefix_size))) {
        return PresetLoadResult::incompatible;
    }
    memcpy(&snapshot, record.data() + preset_header_size, sizeof(snapshot));
    return PresetLoadResult::loaded;
}

int PresetStorage::save(uint8_t slot, const PresetSnapshot &snapshot)
{
    if (!initialized_) return -EACCES;
    if (slot >= preset_count) return -EINVAL;

    std::array<uint8_t, preset_record_size> record = {};
    put_u32_le(record.data(), preset_magic);
    put_u16_le(record.data() + 4U, format_version);
    put_u16_le(record.data() + 6U, sizeof(PresetSnapshot));
    memcpy(record.data() + preset_header_size, &snapshot, sizeof(snapshot));
    put_u32_le(record.data() + preset_prefix_size, crc32_ieee(record.data(), preset_prefix_size));

    char path[40] = {};
    char temporary_path[40] = {};
    slot_path(slot, false, path, sizeof(path));
    slot_path(slot, true, temporary_path, sizeof(temporary_path));
    return replace_file(temporary_path, path, record.data(), record.size());
}

PresetLoadResult PresetStorage::load_startup_slot(uint8_t &slot) const
{
    slot = 0U;
    if (!initialized_) return PresetLoadResult::io_error;
    std::array<uint8_t, startup_record_size> record = {};
    const PresetLoadResult result = read_exact_file(startup_path, record.data(), record.size());
    if (result != PresetLoadResult::loaded) return result;

    const uint32_t stored_crc = get_u32_le(record.data() + startup_prefix_size);
    const bool valid = (get_u32_le(record.data()) == startup_magic) &&
        (get_u16_le(record.data() + 4U) == format_version) &&
        (record[6U] < preset_count) && (record[7U] == 0U) &&
        (stored_crc == crc32_ieee(record.data(), startup_prefix_size));
    if (!valid) return PresetLoadResult::incompatible;
    slot = record[6U];
    return PresetLoadResult::loaded;
}

int PresetStorage::save_startup_slot(uint8_t slot)
{
    if (!initialized_) return -EACCES;
    if (slot >= preset_count) return -EINVAL;
    std::array<uint8_t, startup_record_size> record = {};
    put_u32_le(record.data(), startup_magic);
    put_u16_le(record.data() + 4U, format_version);
    record[6U] = slot;
    put_u32_le(record.data() + startup_prefix_size, crc32_ieee(record.data(), startup_prefix_size));
    return replace_file(startup_tmp_path, startup_path, record.data(), record.size());
}

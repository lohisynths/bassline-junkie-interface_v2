/*
 * SDCard.cpp
 *
 *  Created on: Apr 13, 2026
 *      Author: alax
 */

#include "SDCard.h"

#include <errno.h>
#include <zephyr/logging/log.h>
#include <zephyr/storage/disk_access.h>

LOG_MODULE_REGISTER(app_sdcard, LOG_LEVEL_INF);

namespace {

constexpr const char *sd_test_file_path = "/SD:/sdmmc-test.txt";

} // namespace

int SDCard::init()
{
    uint32_t block_count = 0U;
    uint32_t block_size = 0U;

    mounted_ = false;

    if (disk_access_ioctl(drive_name, DISK_IOCTL_CTRL_INIT, nullptr) != 0) {
        LOG_ERR("SD card init failed");
        return -EIO;
    }

    if (disk_access_ioctl(drive_name, DISK_IOCTL_GET_SECTOR_COUNT, &block_count) != 0) {
        LOG_ERR("Failed to read SD sector count");
        return -EIO;
    }

    if (disk_access_ioctl(drive_name, DISK_IOCTL_GET_SECTOR_SIZE, &block_size) != 0) {
        LOG_ERR("Failed to read SD sector size");
        return -EIO;
    }

    mount_.type = FS_FATFS;
    mount_.fs_data = &fat_fs_;
    mount_.mnt_point = mount_point;

    const int ret = fs_mount(&mount_);
    if (ret != FR_OK) {
        LOG_ERR("Failed to mount SD card at %s: %d", mount_point, ret);
        return -EIO;
    }

    const uint64_t size_bytes = static_cast<uint64_t>(block_count) * block_size;
    LOG_INF("SD card mounted at %s", mount_point);
    LOG_INF("SD geometry: %u blocks x %u bytes (%u MiB)",
            block_count,
            block_size,
            static_cast<uint32_t>(size_bytes >> 20));

    const int exercise_ret = exercise();
    if (exercise_ret < 0) {
        return exercise_ret;
    }
    mounted_ = true;
    return 0;
}

bool SDCard::is_mounted() const
{
    return mounted_;
}

int SDCard::exercise()
{
    static constexpr char test_payload[] = "bassline-junkie sdmmc test\n";
    char readback[sizeof(test_payload)] = {};
    struct fs_file_t file;

    fs_file_t_init(&file);

    int ret = fs_open(&file, sd_test_file_path, FS_O_CREATE | FS_O_RDWR | FS_O_TRUNC);
    if (ret != 0) {
        LOG_ERR("Failed to open SD test file %s: %d", sd_test_file_path, ret);
        return ret;
    }

    const ssize_t written = fs_write(&file, test_payload, sizeof(test_payload) - 1U);
    if (written != static_cast<ssize_t>(sizeof(test_payload) - 1U)) {
        LOG_ERR("Failed to write SD test file %s: %d", sd_test_file_path, static_cast<int>(written));
        fs_close(&file);
        return -EIO;
    }

    ret = fs_close(&file);
    if (ret != 0) {
        LOG_ERR("Failed to close SD test file after write: %d", ret);
        return ret;
    }

    fs_file_t_init(&file);
    ret = fs_open(&file, sd_test_file_path, FS_O_READ);
    if (ret != 0) {
        LOG_ERR("Failed to reopen SD test file %s: %d", sd_test_file_path, ret);
        return ret;
    }

    const ssize_t read = fs_read(&file, readback, sizeof(readback) - 1U);
    if (read != static_cast<ssize_t>(sizeof(test_payload) - 1U)) {
        LOG_ERR("Failed to read SD test file %s: %d", sd_test_file_path, static_cast<int>(read));
        fs_close(&file);
        return -EIO;
    }

    ret = fs_close(&file);
    if (ret != 0) {
        LOG_ERR("Failed to close SD test file after read: %d", ret);
        return ret;
    }

    for (size_t i = 0U; i < sizeof(test_payload); ++i) {
        if (readback[i] != test_payload[i]) {
            LOG_ERR("SD test file verification failed at byte %u",
                    static_cast<unsigned int>(i));
            return -EIO;
        }
    }

    LOG_INF("SD file test passed: %s", sd_test_file_path);
    return 0;
}

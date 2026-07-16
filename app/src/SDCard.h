/*
 * SDCard.h
 *
 *  Created on: Apr 13, 2026
 *      Author: alax
 */

#ifndef SRC_SDCARD_H_
#define SRC_SDCARD_H_

#include <zephyr/fs/fs.h>

#include <ff.h>

/**
 * @brief FATFS-backed SD card helper for startup bring-up checks.
 */
class SDCard {
public:
    /** @brief Disk-access name of the SD card device. */
    static constexpr const char *drive_name = "SD";
    /** @brief FATFS mount point used by application storage. */
    static constexpr const char *mount_point = "/SD:";
    /** @brief Constructs an uninitialized SD card helper. */
    SDCard() = default;

    /**
     * @brief Initializes, mounts, and verifies writable SD-card storage.
     *
     * @retval 0 The card was mounted and the file round-trip test passed.
     * @retval negative SDMMC, FATFS, or file I/O error.
     */
    int init();

    /**
     * @brief Reports whether the SD card is currently mounted.
     *
     * @retval true The card was mounted successfully.
     * @retval false Mounting has not completed successfully.
     */
    bool is_mounted() const;

private:
    /**
     * @brief Verifies the mounted card with a temporary-file round trip.
     *
     * @retval 0 The write, read, comparison, and cleanup succeeded.
     * @retval negative File-system error or data-verification failure.
     */
    int exercise();

    /** @brief Whether mounting and the writable-storage check succeeded. */
    bool mounted_ = false;
    /** @brief FatFs state owned for the lifetime of the mount. */
    FATFS fat_fs_ = {};
    /** @brief Zephyr file-system mount descriptor for the SD card. */
    struct fs_mount_t mount_ = {};
};

#endif /* SRC_SDCARD_H_ */

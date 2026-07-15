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
    static constexpr const char *drive_name = "SD";
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
    int exercise();

    bool mounted_ = false;
    FATFS fat_fs_ = {};
    struct fs_mount_t mount_ = {};
};

#endif /* SRC_SDCARD_H_ */

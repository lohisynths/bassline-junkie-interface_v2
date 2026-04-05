#ifndef SRC_EEPROM_H_
#define SRC_EEPROM_H_

#include "PresetSnapshot.h"

#include <cstdint>

struct flash_area;

/**
 * @brief Flash-backed preset storage helper.
 */
class EEPROM {
public:
    /** @brief Number of preset slots stored in flash. */
    static constexpr uint8_t preset_count = 128U;

    /** @brief Constructs an uninitialized flash backend. */
    EEPROM() = default;

    /**
     * @brief Opens the preset flash partition and rebuilds the in-RAM cache.
     *
     * @retval 0 The flash backend is ready.
     * @retval negative Error propagated from the flash map or flash driver.
     */
    int init();

    /**
     * @brief Loads one preset slot from the cache.
     *
     * Unsaved slots return the canonical all-zero snapshot.
     *
     * @param index Preset slot in the range `[0, preset_count)`.
     * @param snapshot Receives the loaded snapshot.
     *
     * @retval 0 The slot was loaded successfully.
     * @retval -EINVAL The requested slot is out of range.
     */
    int load(uint8_t index, PresetSnapshot &snapshot) const;

    /**
     * @brief Saves one preset slot by appending one flash record.
     *
     * @param index Preset slot in the range `[0, preset_count)`.
     * @param snapshot Snapshot to persist.
     *
     * @retval 0 The preset slot was written successfully.
     * @retval negative Error propagated from the flash driver.
     */
    int save(uint8_t index, const PresetSnapshot &snapshot);

    /**
     * @brief Loads the preset slot that should be restored at startup.
     *
     * If no startup slot has been stored yet, this returns slot `0`.
     *
     * @param slot Receives the stored startup slot.
     *
     * @retval 0 The startup slot was returned successfully.
     */
    int load_startup_slot(uint8_t &slot) const;

    /**
     * @brief Stores the preset slot that should be restored at startup.
     *
     * This appends a small metadata record to the same flash journal used by
     * preset snapshots.
     *
     * @param slot Preset slot to restore on the next boot.
     *
     * @retval 0 The startup slot was written successfully.
     * @retval negative Error propagated from the flash driver.
     */
    int save_startup_slot(uint8_t slot);

    /**
     * @brief Loads the persisted master volume value.
     *
     * If no volume has been stored yet, this returns `0`.
     *
     * @param volume Receives the stored master volume.
     *
     * @retval 0 The master volume was returned successfully.
     */
    int load_master_volume(uint8_t &volume) const;

    /**
     * @brief Stores the master volume value independently of preset snapshots.
     *
     * This appends a metadata record to the same flash journal used by
     * preset snapshots and the startup preset slot.
     *
     * @param volume Master volume value to persist.
     *
     * @retval 0 The volume was written successfully.
     * @retval negative Error propagated from the flash driver.
     */
    int save_master_volume(uint8_t volume);

private:
    /** @brief Open flash area backing the preset image. */
    const struct flash_area *flash_area_ = nullptr;

    /** @brief Tracks whether @ref init completed successfully. */
    bool initialized_ = false;
};

#endif /* SRC_EEPROM_H_ */

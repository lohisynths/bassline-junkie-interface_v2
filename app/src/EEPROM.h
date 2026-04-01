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

private:
    /** @brief Open flash area backing the preset image. */
    const struct flash_area *flash_area_ = nullptr;

    /** @brief Tracks whether @ref init completed successfully. */
    bool initialized_ = false;
};

#endif /* SRC_EEPROM_H_ */

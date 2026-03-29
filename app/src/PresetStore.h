#ifndef SRC_PRESETSTORE_H_
#define SRC_PRESETSTORE_H_

#include "PresetSnapshot.h"

#include <stdint.h>

struct flash_area;

/**
 * @brief Persists one 128-slot preset image in the dedicated flash partition.
 */
class PresetStore {
public:
    /**
     * @brief Number of preset slots exposed by the store.
     */
    static const uint8_t preset_count = 128U;

    /**
     * @brief Initializes the flash-backed preset image cache.
     *
     * @retval 0 The store is ready and the flash image was loaded or reset.
     * @retval negative Error propagated from the flash map or flash driver.
     */
    int init();

    /**
     * @brief Loads one preset snapshot from the cached flash image.
     *
     * Empty slots return the canonical default snapshot.
     *
     * @param index Preset slot in the range `[0, preset_count)`.
     * @param snapshot Receives the loaded or default snapshot.
     * @param slot_was_saved Receives `true` when the slot contains saved data.
     *
     * @retval 0 The request completed successfully.
     * @retval -EACCES The store has not been initialized.
     * @retval -EINVAL The requested preset index is out of range.
     */
    int load_preset(uint8_t index, PresetSnapshot &snapshot, bool &slot_was_saved) const;

    /**
     * @brief Saves one preset snapshot and rewrites the preset flash partition.
     *
     * @param index Preset slot in the range `[0, preset_count)`.
     * @param snapshot Snapshot to store in the selected slot.
     *
     * @retval 0 The preset image was updated successfully.
     * @retval -EACCES The store has not been initialized.
     * @retval -EINVAL The requested preset index is out of range.
     * @retval negative Error propagated from the flash driver.
     */
    int save_preset(uint8_t index, const PresetSnapshot &snapshot);

private:
    /** @brief Open flash area backing the preset image. */
    const struct flash_area *flash_area_ = nullptr;

    /** @brief Tracks whether @ref init completed successfully. */
    bool initialized_ = false;
};

#endif /* SRC_PRESETSTORE_H_ */

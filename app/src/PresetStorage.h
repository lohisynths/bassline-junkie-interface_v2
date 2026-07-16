#ifndef APP_SRC_PRESETSTORAGE_H_
#define APP_SRC_PRESETSTORAGE_H_

#include "PresetSnapshot.h"

#include <cstdint>

class SDCard;

enum class PresetLoadResult : uint8_t {
    loaded,       /**< A valid record was loaded. */
    not_found,    /**< No record exists at the requested path. */
    incompatible, /**< The record format, size, checksum, or values are invalid. */
    io_error,     /**< Storage was unavailable or a file operation failed. */
};

/** @brief SD-card-backed storage for presets and startup-slot metadata. */
class PresetStorage {
public:
    /** @brief Number of addressable preset slots. */
    static constexpr uint8_t preset_count = 128U;
    /** @brief Directory containing preset records and startup metadata. */
    static constexpr const char *preset_directory = "/SD:/presets";

    /**
     * @brief Binds storage to a mounted SD card and creates the preset directory.
     *
     * @param sd_card Mounted SD card used for all records.
     * @retval 0 Storage is ready.
     * @retval negative The card is not mounted or the directory could not be created.
     */
    int init(SDCard &sd_card);

    /**
     * @brief Loads one preset record, defaulting @p snapshot before reading.
     *
     * @param slot Zero-based preset slot.
     * @param snapshot Destination snapshot.
     * @return Detailed load result distinguishing missing, invalid, and I/O failures.
     */
    PresetLoadResult load(uint8_t slot, PresetSnapshot &snapshot) const;

    /**
     * @brief Atomically writes one versioned, CRC-protected preset record.
     *
     * @param slot Zero-based preset slot.
     * @param snapshot Snapshot to persist.
     * @retval 0 The record was replaced successfully.
     * @retval negative Storage is unavailable, the slot is invalid, or file I/O failed.
     */
    int save(uint8_t slot, const PresetSnapshot &snapshot);

    /**
     * @brief Loads the slot that should be restored during startup.
     *
     * @param slot Destination for the zero-based startup slot; initialized to zero.
     * @return Detailed load result distinguishing missing, invalid, and I/O failures.
     */
    PresetLoadResult load_startup_slot(uint8_t &slot) const;

    /**
     * @brief Atomically records the slot that should be restored during startup.
     *
     * @param slot Zero-based startup slot.
     * @retval 0 The metadata was replaced successfully.
     * @retval negative Storage is unavailable, the slot is invalid, or file I/O failed.
     */
    int save_startup_slot(uint8_t slot);

private:
    /** @brief Whether initialization completed successfully. */
    bool initialized_ = false;
};

#endif /* APP_SRC_PRESETSTORAGE_H_ */

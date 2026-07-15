#ifndef APP_SRC_PRESETSTORAGE_H_
#define APP_SRC_PRESETSTORAGE_H_

#include "PresetSnapshot.h"

#include <cstdint>

class SDCard;

enum class PresetLoadResult : uint8_t {
    loaded,
    not_found,
    incompatible,
    io_error,
};

/** @brief SD-card-backed storage for presets and startup-slot metadata. */
class PresetStorage {
public:
    static constexpr uint8_t preset_count = 128U;
    static constexpr const char *preset_directory = "/SD:/presets";

    int init(SDCard &sd_card);
    PresetLoadResult load(uint8_t slot, PresetSnapshot &snapshot) const;
    int save(uint8_t slot, const PresetSnapshot &snapshot);
    PresetLoadResult load_startup_slot(uint8_t &slot) const;
    int save_startup_slot(uint8_t slot);

private:
    bool initialized_ = false;
};

#endif /* APP_SRC_PRESETSTORAGE_H_ */

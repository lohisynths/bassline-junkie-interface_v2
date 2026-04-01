#ifndef SRC_PRESETSNAPSHOT_H_
#define SRC_PRESETSNAPSHOT_H_

#include "blocks/ADSR.h"
#include "blocks/FLT.h"
#include "blocks/LFO.h"
#include "blocks/OSC.h"

#include <cstdint>

/**
 * @brief Captures the durable state of one full surface preset.
 *
 * The snapshot stores the banked values for ADSR, FLT, LFO, and OSC, plus
 * the OSC and FLT modulation matrices edited through the MOD block. Live bank
 * selections remain outside the durable snapshot and are intentionally not
 * stored here.
 */
struct PresetSnapshot {
    /** @brief Snapshot format version for compatibility checks. */
    static constexpr uint16_t version = 1U;

    /** @brief Durable ADSR banked values. */
    ADSR::preset adsr = {};

    /** @brief Durable FLT values. */
    FLT::preset flt = {};

    /** @brief Durable LFO banked values. */
    LFO::preset lfo = {};

    /** @brief Durable OSC banked values. */
    OSC::preset osc = {};

    /** @brief Durable OSC modulation matrix edited through MOD. */
    OSC::mod_preset osc_mod = {};

    /** @brief Durable FLT modulation matrix edited through MOD. */
    FLT::mod_preset flt_mod = {};
};

/** @brief Returns the canonical all-zero default snapshot. */
inline constexpr PresetSnapshot default_preset_snapshot()
{
    return {};
}

#endif /* SRC_PRESETSNAPSHOT_H_ */

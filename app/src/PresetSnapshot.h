#ifndef SRC_PRESETSNAPSHOT_H_
#define SRC_PRESETSNAPSHOT_H_

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Captures the durable UI state for one ADSR block preset.
 */
struct ADSRState {
    static constexpr size_t bank_count = 3U;
    static constexpr size_t knob_count = 4U;

    uint8_t knob_values[bank_count][knob_count] = {};
    uint8_t button3_values[bank_count] = {};
};

/**
 * @brief Captures the durable UI state for one FLT block preset.
 */
struct FLTState {
    static constexpr size_t knob_count = 3U;

    uint8_t selected_button = 0U;
    uint8_t knob_values[knob_count] = {};
};

/**
 * @brief Captures the durable UI state for one LFO block preset.
 */
struct LFOState {
    static constexpr size_t bank_count = 3U;
    static constexpr size_t knob_count = 1U;

    uint8_t knob_values[bank_count][knob_count] = {};
    uint8_t radio_selection[bank_count] = {};
};

/**
 * @brief Captures the durable UI state for one MOD block preset.
 */
struct MODState {
    static constexpr size_t bank_count = 6U;
    static constexpr size_t knob_count = 1U;

    uint8_t knob_values[bank_count][knob_count] = {};
};

/**
 * @brief Captures the durable UI state for one OSC block preset.
 */
struct OSCState {
    static constexpr size_t bank_count = 3U;
    static constexpr size_t knob_count = 5U;

    uint8_t knob_values[bank_count][knob_count] = {};
};

/**
 * @brief Aggregates the durable state of every control-surface block.
 */
struct PresetSnapshot {
    ADSRState adsr = {};
    FLTState flt = {};
    LFOState lfo = {};
    MODState mod = {};
    OSCState osc = {};
};

/**
 * @brief Returns the canonical all-zero default preset snapshot.
 */
inline constexpr PresetSnapshot default_preset_snapshot()
{
    return {};
}

#endif /* SRC_PRESETSNAPSHOT_H_ */

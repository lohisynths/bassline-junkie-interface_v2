#ifndef APP_SRC_BLOCKS_MODMATRIXCAPTURE_H_
#define APP_SRC_BLOCKS_MODMATRIXCAPTURE_H_

#include <stdint.h>

/**
 * @brief Sink interface for temporarily repurposing OSC and FLT knobs to edit
 *        MOD matrix routes.
 */
class ModMatrixCapture {
public:
    /**
     * @brief Returns whether the MOD viewer overlay is currently active.
     *
     * @retval true The OSC/FLT knobs are being borrowed by the MOD viewer.
     * @retval false Normal OSC/FLT LED rendering is active.
     */
    virtual bool is_mod_viewer_active() const = 0;

    /**
     * @brief Reapplies the current MOD viewer overlay immediately.
     *
     * This is used when another block changes visible context, such as an OSC
     * bank switch, and the borrowed knob LEDs must be repainted in the same
     * update cycle to avoid showing the underlying preset state.
     */
    virtual void refresh_mod_viewer() = 0;

    /**
     * @brief Attempts to consume one OSC knob value for MOD-matrix editing.
     *
     * @param osc_bank Active OSC bank whose visible knob changed.
     * @param knob_index OSC knob index within the active bank.
     * @param value New clamped knob value.
     *
     * @retval true The event was consumed by MOD viewer-edit mode.
     * @retval false The caller should keep normal OSC parameter behavior.
     */
    virtual bool capture_osc_route_value(uint8_t osc_bank, uint8_t knob_index, uint8_t value) = 0;

    /**
     * @brief Attempts to consume one FLT knob value for MOD-matrix editing.
     *
     * @param knob_index FLT knob index that changed.
     * @param value New clamped knob value.
     *
     * @retval true The event was consumed by MOD viewer-edit mode.
     * @retval false The caller should keep normal FLT parameter behavior.
     */
    virtual bool capture_flt_route_value(uint8_t knob_index, uint8_t value) = 0;
};

#endif /* APP_SRC_BLOCKS_MODMATRIXCAPTURE_H_ */

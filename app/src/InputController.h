/*
 * InputController.h
 *
 *  Created on: Mar 27, 2026
 *      Author: alax
 */

#ifndef SRC_INPUTCONTROLLER_H_
#define SRC_INPUTCONTROLLER_H_

#include "MUX.h"

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Aggregates all configured CD4067 inputs.
 */
class InputController {
public:
    /** @brief Total number of cached input state masks. */
    static const size_t input_count = MUX::mux_count;

    /** @brief Constructs an input controller facade. */
    InputController() = default;

    /**
     * @brief Initializes the CD4067 input subsystem.
     *
     * @retval 0 Initialization completed successfully.
     * @retval negative Error propagated from @ref MUX::init.
     */
    int init();

    /**
     * @brief Reads all configured mux inputs into cached bitmasks.
     *
     * @retval 0 All inputs were read successfully.
     * @retval -EACCES The input controller has not been initialized.
     * @retval negative Error propagated from @ref MUX::read_state.
     */
    int update();

    /**
     * @brief Logs the current mux input states.
     *
     * @retval 0 All input states were logged successfully.
     * @retval -EACCES The input controller has not been initialized.
     * @retval negative Error propagated from @ref MUX::log_state.
     */
    int log_state();

    /**
     * @brief Logs mux-bit transitions between the previous and current mux snapshots.
     */
    void log_mux_changes();

    /**
     * @brief Returns one cached raw input-state mask.
     *
     * @param state_index Index in the cached input state table.
     *
     * @return Cached 16-bit raw state mask, or `0` if @p state_index is invalid.
     */
    uint16_t state(size_t state_index) const;

private:
    /** @brief Total number of configured CD4067 instances. */
    static const size_t mux_count_ = MUX::mux_count;

    /** @brief Tracks whether @ref init completed successfully. */
    bool initialized_ = false;

    /** @brief CD4067 input facade. */
    MUX mux_;

    /** @brief Cached raw masks for all mux inputs. */
    uint16_t active_masks_[input_count] = {};

    /** @brief Previous cached input masks used for change logging. */
    uint16_t previous_masks_[input_count] = {};
};

#endif /* SRC_INPUTCONTROLLER_H_ */

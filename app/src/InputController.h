/*
 * InputController.h
 *
 *  Created on: Mar 27, 2026
 *      Author: alax
 */

#ifndef SRC_INPUTCONTROLLER_H_
#define SRC_INPUTCONTROLLER_H_

#include "GPIO.h"
#include "MUX.h"

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Aggregates all configured GPIO and CD4067 inputs.
 */
class InputController {
public:
    /** @brief Total number of cached input state masks. */
    static const size_t state_count = MUX::mux_count + 1U;

    /** @brief Constructs an input controller facade. */
    InputController() = default;

    /**
     * @brief Initializes the GPIO and CD4067 input subsystems.
     *
     * @retval 0 Initialization completed successfully.
     * @retval negative Error propagated from @ref MUX::init or @ref GPIO::init.
     */
    int init();

    /**
     * @brief Reads all configured mux and GPIO inputs into cached bitmasks.
     *
     * @retval 0 All inputs were read successfully.
     * @retval -EACCES The input controller has not been initialized.
     * @retval negative Error propagated from @ref MUX::read_state or
     *         @ref GPIO::read_state.
     */
    int update();

    /**
     * @brief Logs the current mux and GPIO input states.
     *
     * @retval 0 All input states were logged successfully.
     * @retval -EACCES The input controller has not been initialized.
     * @retval negative Error propagated from @ref MUX::log_state or
     *         @ref GPIO::log_state.
     */
    int log_state();

    /**
     * @brief Logs mux-bit transitions between the previous and current mux snapshots.
     */
    void log_mux_changes();

    /**
     * @brief Returns one cached active mask.
     *
     * @param state_index Index in the cached input state table.
     *
     * @return Cached 16-bit active mask, or `0` if @p state_index is invalid.
     */
    uint16_t state(size_t state_index) const;

private:
    /** @brief Total number of configured CD4067 instances. */
    static const size_t mux_count_ = MUX::mux_count;

    /** @brief Mux-style index of the cached discrete GPIO state in @ref active_masks_. */
    static const size_t gpio_mux_index_ = mux_count_;

    /** @brief Tracks whether @ref init completed successfully. */
    bool initialized_ = false;

    /** @brief GPIO input facade. */
    GPIO gpio_;

    /** @brief CD4067 input facade. */
    MUX mux_;

    /** @brief Cached active masks for all mux inputs plus the GPIO input mask. */
    uint16_t active_masks_[state_count] = {};

    /** @brief Previous cached mux masks used for change logging. */
    uint16_t previous_mux_masks_[mux_count_] = {};
};

#endif /* SRC_INPUTCONTROLLER_H_ */

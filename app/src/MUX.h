/*
 * MUX.h
 *
 *  Created on: Mar 26, 2026
 *      Author: alax
 */

#ifndef SRC_MUX_H_
#define SRC_MUX_H_

#include <stdint.h>

/**
 * @brief Controls the CD4067 GPIO multiplexer and logs its active inputs.
 */
class MUX {
public:
    /** @brief Constructs a CD4067 facade. */
    MUX() = default;

    /**
     * @brief Verifies that the configured CD4067 device is ready.
     *
     * @retval 0 The device is ready.
     * @retval -ENODEV The configured CD4067 device is not ready.
     */
    int init();

    /**
     * @brief Scans all CD4067 channels and logs the active bitmask.
     *
     * @retval 0 The scan completed successfully.
     * @retval -EACCES The multiplexer has not been initialized.
     * @retval negative Driver-specific error returned by the CD4067 driver.
     */
    int log_state();

private:
    /** @brief Tracks whether @ref init completed successfully. */
    bool initialized_ = false;
};

#endif /* SRC_MUX_H_ */

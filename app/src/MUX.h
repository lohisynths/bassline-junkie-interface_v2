/*
 * MUX.h
 *
 *  Created on: Mar 26, 2026
 *      Author: alax
 */

#ifndef SRC_MUX_H_
#define SRC_MUX_H_

#include <zephyr/drivers/gpio.h>

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Controls the configured CD4067 multiplexers and logs their active inputs.
 */
class MUX {
public:
    /** @brief Total number of configured CD4067 devices. */
    static const size_t mux_count = 4U;

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
     * @brief Scans one CD4067 instance and returns its active bitmask.
     *
     * @param mux_index Index in @ref mux_devices.
     * @param active_mask Output bitmask for the selected mux.
     *
     * @retval 0 The scan completed successfully.
     * @retval -EACCES The multiplexer has not been initialized.
     * @retval -EINVAL The mux index is invalid or @p active_mask is null.
     * @retval negative Driver-specific error returned by the CD4067 driver.
     */
    int read_state(size_t mux_index, uint16_t *active_mask);

    /**
     * @brief Scans all CD4067 channels and logs the active bitmask.
     *
     * @retval 0 The scan completed successfully.
     * @retval -EACCES The multiplexer has not been initialized.
     * @retval negative Driver-specific error returned by the CD4067 driver.
     */
    int log_state();

private:
    /** @brief Describes one configured CD4067 instance. */
    struct mux_device {
        /** @brief Zephyr device handle for the configured CD4067 instance. */
        const struct device *dev;

        /** @brief GPIO specification for the mux SIG input. */
        gpio_dt_spec sig;
    };

    /** @brief Tracks whether @ref init completed successfully. */
    bool initialized_ = false;

    /** @brief Static table of CD4067 devices managed by this class. */
    static const mux_device mux_devices[];
};

#endif /* SRC_MUX_H_ */

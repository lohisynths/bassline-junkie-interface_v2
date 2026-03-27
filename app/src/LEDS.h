/*
 * LEDS.h
 *
 *  Created on: Mar 26, 2026
 *      Author: alax
 */

#ifndef SRC_LEDS_H_
#define SRC_LEDS_H_

#include <zephyr/devicetree.h>
#include <zephyr/drivers/pwm.h>

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Controls LED outputs exposed through multiple PCA9685 PWM controllers.
 */
class LEDSController {
public:
    /** @brief Total number of addressable LED channels across all controllers. */
    static const size_t led_count;

    /** @brief Constructs an LED controller facade. */
    LEDSController() = default;

    /**
     * @brief Verifies controller availability and clears all LED outputs.
     *
     * @retval 0 Initialization completed successfully.
     * @retval -ENODEV At least one PCA9685 controller is not ready.
     * @retval negative Driver-specific error returned while clearing outputs.
     */
    int init();

    /**
     * @brief Turns off every LED channel on every controller.
     *
     * @retval 0 All channels were cleared successfully.
     * @retval -EACCES The LED subsystem has not been initialized.
     * @retval negative Driver-specific error returned by the PWM driver.
     */
    int clear_all();

    /**
     * @brief Sets one LED channel using a brightness percentage.
     *
     * @param channel Global LED channel index in the range `[0, led_count)`.
     * @param percent Brightness percentage in the range `[0, 100]`.
     *
     * @retval 0 The channel was updated successfully.
     * @retval -EACCES The LED subsystem has not been initialized.
     * @retval -EINVAL The channel index or percentage is invalid.
     * @retval negative Driver-specific error returned by the PWM driver.
     */
    int set_channel_percent(size_t channel, uint8_t percent);

    /**
     * @brief Sets one LED channel using a raw PWM pulse width.
     *
     * @param channel Global LED channel index in the range `[0, led_count)`.
     * @param pulse Pulse width in PWM ticks, limited to `pca9685_period`.
     *
     * @retval 0 The channel was updated successfully.
     * @retval -EACCES The LED subsystem has not been initialized.
     * @retval -EINVAL The channel index or pulse width is invalid.
     * @retval negative Driver-specific error returned by the PWM driver.
     */
    int set_channel(size_t channel, uint32_t pulse);

private:
    /** @brief PWM period used for all PCA9685 channels. */
    static const uint32_t pca9685_period = PWM_MSEC(5);

    /** @brief Number of PWM channels exposed by a single PCA9685 device. */
    static const size_t pca9685_channel_count = 16U;

    /** @brief Number of PCA9685 controllers described by @ref pca9685_controllers. */
    static const size_t pca9685_count;

    /**
     * @brief Logs readiness information for each configured PCA9685 controller.
     *
     * @retval 0 All controllers are ready.
     * @retval -ENODEV At least one controller is not ready.
     */
    int report_status();

    /** @brief Describes one PCA9685 controller instance. */
    struct pca9685_controller {
        /** @brief Zephyr device handle for the PCA9685 instance. */
        const struct device *dev;

        /** @brief I2C address assigned to the PCA9685 instance. */
        uint8_t address;
    };

    /** @brief Tracks whether @ref init completed successfully. */
    bool initialized_ = false;

    /** @brief Static table of PCA9685 controllers managed by this class. */
    static const pca9685_controller pca9685_controllers[];
};

#endif /* SRC_LEDS_H_ */

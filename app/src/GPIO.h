/*
 * GPIO.h
 *
 *  Created on: Mar 27, 2026
 *      Author: alax
 */

#ifndef SRC_GPIO_H_
#define SRC_GPIO_H_

#include <zephyr/drivers/gpio.h>

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Reads the discrete encoder GPIO inputs described in devicetree.
 */
class GPIO {
public:
    /** @brief Total number of configured GPIO inputs. */
    static const size_t input_count;

    /** @brief Constructs a GPIO input facade. */
    GPIO() = default;

    /**
     * @brief Verifies GPIO availability and configures each pin as an input.
     *
     * @retval 0 Initialization completed successfully.
     * @retval -ENODEV At least one GPIO controller is not ready.
     * @retval negative Driver-specific error returned while configuring a pin.
     */
    int init();

    /**
     * @brief Reads one configured input.
     *
     * @param input_index Index in the configured GPIO table.
     * @param active Output raw physical pin level, where `true` means high.
     *
     * @retval 0 The input was read successfully.
     * @retval -EACCES The GPIO subsystem has not been initialized.
     * @retval -EINVAL The input index is invalid or @p active is null.
     * @retval negative Driver-specific error returned by the GPIO driver.
     */
    int read_pin(size_t input_index, bool *active);

    /**
     * @brief Reads all configured inputs and returns one raw-high bitmask.
     *
     * @param active_mask Output bitmask for all configured GPIO inputs, where
     *        each set bit represents one raw high level.
     *
     * @retval 0 All inputs were read successfully.
     * @retval negative Error propagated from @ref read_pin.
     */
    int read_state(uint16_t *active_mask);

    /**
     * @brief Reads all configured inputs and logs their current states.
     *
     * @retval 0 All inputs were read successfully.
     * @retval negative Error propagated from @ref read_state.
     */
    int log_state();

    /**
     * @brief Reads all configured inputs and logs their current states in binary form.
     *
     * @retval 0 All inputs were read successfully.
     * @retval negative Error propagated from @ref read_state.
     */
    int log_state_binary();

private:
    /** @brief Describes one configured GPIO input. */
    struct input_pin {
        /** @brief Human-readable signal name used for logs. */
        const char *name;

        /** @brief GPIO specification for the configured input. */
        gpio_dt_spec spec;
    };

    /** @brief Tracks whether @ref init completed successfully. */
    bool initialized_ = false;

    /** @brief Static table of GPIO inputs managed by this class. */
    static const input_pin input_pins[];
};

#endif /* SRC_GPIO_H_ */

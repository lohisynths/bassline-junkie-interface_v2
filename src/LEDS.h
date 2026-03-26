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

#define PCA9685_CTRL(node_label)                           \
    {                                                      \
        .dev = DEVICE_DT_GET(DT_NODELABEL(node_label)),    \
        .address = DT_REG_ADDR(DT_NODELABEL(node_label)),  \
    }

class LEDS {
public:
    static const uint32_t pca9685_period = PWM_MSEC(5);
    static const size_t pca9685_channel_count = 16U;

    LEDS() = default;

    int init();
    int clear_all();
    int set_channel(size_t channel, uint32_t pulse);
    size_t led_count();

private:
    int report_pca9685_status();

    struct pca9685_controller {
        const struct device *dev;
        uint8_t address;
    };

    static const pca9685_controller pca9685_controllers[];
};

#endif /* SRC_LEDS_H_ */

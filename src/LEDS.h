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

class LEDS {
public:
    static const uint32_t pca9685_period = PWM_MSEC(5);
    static const size_t pca9685_channel_count = 16U;
    static const size_t controller_count;
    static const size_t led_count;

    LEDS() = default;

    int init();
    int clear_all();
    int set_channel(size_t channel, uint32_t pulse);

private:
    int report_status();

    struct pca9685_controller {
        const struct device *dev;
        uint8_t address;
    };

    bool initialized_ = false;

    static const pca9685_controller pca9685_controllers[];
};

#endif /* SRC_LEDS_H_ */

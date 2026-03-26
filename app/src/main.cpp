#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/mux/cd4067.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "LEDS.h"

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

#define LED0_NODE DT_ALIAS(led0)
#define CD4067_NODE DT_NODELABEL(cd4067_0)

#if !DT_NODE_HAS_STATUS(LED0_NODE, okay)
#error "This board does not define a usable led0 alias"
#endif

#if !DT_NODE_HAS_STATUS(CD4067_NODE, okay)
#error "This build expects a usable cd4067_0 devicetree node"
#endif

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static const struct device *const mux = DEVICE_DT_GET(CD4067_NODE);

static int log_mux_state()
{
    uint16_t active_mask = 0U;

    for (uint8_t channel = 0U; channel < CD4067_CHANNEL_COUNT; ++channel) {
        int value = 0;
        const int ret = cd4067_read_channel(mux, channel, &value);

        if (ret < 0) {
            return ret;
        }

        if (value != 0) {
            active_mask |= (uint16_t)(1U << channel);
        }
    }

    LOG_INF("CD4067 active mask: 0x%04x", active_mask);

    return 0;
}

int main(void)
{
    int ret;
    uint32_t blink_count = 0U;
    size_t chase_step = 0U;


    if (!gpio_is_ready_dt(&led)) {
        LOG_ERR("LED GPIO device is not ready");
        return 0;
    }

    ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        LOG_ERR("Failed to configure LED pin: %d", ret);
        return 0;
    }

    LOG_INF("Bassline Junkie Interface");
    LOG_INF("Console TX ready on ttyACM0");

    if (!device_is_ready(mux)) {
        LOG_ERR("CD4067 device is not ready");
        return 0;
    }

    LEDS leds;
    ret = leds.init();
    if (ret < 0) {
        LOG_ERR("Failed to initialize LED controllers: %d", ret);
        return 0;
    }

    while (1) {
        ret = gpio_pin_toggle_dt(&led);
        if (ret < 0) {
            LOG_ERR("Failed to toggle LED: %d", ret);
            return 0;
        }

        ret = leds.clear_all();
        if (ret < 0) {
            LOG_ERR("Failed to clear LED channels: %d", ret);
            return 0;
        }

        ret = leds.set_channel_percent(chase_step, 50U);
        if (ret < 0) {
            LOG_ERR("Failed to set LED channel: %d", ret);
            return 0;
        }

        chase_step = (chase_step + 1U) % LEDS::led_count;

        blink_count++;
        if ((blink_count % 10U) == 0U) {
            ret = log_mux_state();
            if (ret < 0) {
                LOG_ERR("Failed to scan CD4067 channels: %d", ret);
                return 0;
            }

            LOG_INF("Heartbeat: LED blink running, chase step %u", (unsigned int)chase_step);
        }

        k_msleep(100);
    }
}

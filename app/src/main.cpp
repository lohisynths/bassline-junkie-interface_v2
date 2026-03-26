#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "LEDS.h"
#include "MUX.h"

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

#define LED0_NODE DT_ALIAS(led0)

#if !DT_NODE_HAS_STATUS(LED0_NODE, okay)
#error "This board does not define a usable led0 alias"
#endif

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

int main(void)
{
    int ret;
    uint32_t blink_count = 0U;
    size_t chase_step = 0U;
    MUX mux;

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

    ret = mux.init();
    if (ret < 0) {
        LOG_ERR("Failed to initialize CD4067: %d", ret);
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
            ret = mux.log_state();
            if (ret < 0) {
                LOG_ERR("Failed to scan CD4067 channels: %d", ret);
                return 0;
            }

            LOG_INF("Heartbeat: LED blink running, chase step %u", (unsigned int)chase_step);
        }

        k_msleep(100);
    }
}

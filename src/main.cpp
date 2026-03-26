#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>
#include <stdio.h>
#include "LEDS.h"

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


    if (!gpio_is_ready_dt(&led)) {
        printf("Error: LED GPIO device is not ready\n");
        return 0;
    }

    ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        printf("Error: failed to configure LED pin (%d)\n", ret);
        return 0;
    }

    printf("\r\nBassline Junkie Interface\r\n");
    printf("Console TX ready on ttyACM0.\r\n");

    LEDS leds;

    while (1) {
        ret = gpio_pin_toggle_dt(&led);
        if (ret < 0) {
            printf("Error: failed to toggle LED (%d)\n", ret);
            return 0;
        }

        leds.run_pca9685_chase_step(chase_step);
        chase_step = (chase_step + 1U) % leds.get_leds_count();

        blink_count++;
        if ((blink_count % 10U) == 0U) {
            printf("Heartbeat: LED blink running, chase step %u\r\n", (unsigned int)chase_step);
        }

        k_msleep(100);
    }
}

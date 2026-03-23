#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>
#include <stdio.h>

#define LED0_NODE DT_ALIAS(led0)
#define PCA9685_PERIOD PWM_MSEC(5)
#define PCA9685_CHANNEL_COUNT 16U

#if !DT_NODE_HAS_STATUS(LED0_NODE, okay)
#error "This board does not define a usable led0 alias"
#endif

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

struct pca9685_controller {
    const struct device *dev;
    uint8_t address;
};

#define PCA9685_CTRL(node_label, addr)                  \
    {                                                   \
        .dev = DEVICE_DT_GET(DT_NODELABEL(node_label)), \
        .address = (addr),                              \
    }

static const struct pca9685_controller pca9685_controllers[] = {
    PCA9685_CTRL(pca9685_40, 0x40),
    PCA9685_CTRL(pca9685_41, 0x41),
    PCA9685_CTRL(pca9685_42, 0x42),
    PCA9685_CTRL(pca9685_43, 0x43),
    PCA9685_CTRL(pca9685_44, 0x44),
    PCA9685_CTRL(pca9685_45, 0x45),
    PCA9685_CTRL(pca9685_46, 0x46),
    PCA9685_CTRL(pca9685_47, 0x47),
    PCA9685_CTRL(pca9685_48, 0x48),
    PCA9685_CTRL(pca9685_49, 0x49),
    PCA9685_CTRL(pca9685_4a, 0x4A),
    PCA9685_CTRL(pca9685_4b, 0x4B),
    PCA9685_CTRL(pca9685_4c, 0x4C),
};

static void set_all_pca9685_channels(uint32_t pulse)
{
    for (size_t ctrl = 0; ctrl < ARRAY_SIZE(pca9685_controllers); ctrl++) {
        for (uint32_t channel = 0; channel < PCA9685_CHANNEL_COUNT; channel++) {
            (void)pwm_set(pca9685_controllers[ctrl].dev, channel,
                          PCA9685_PERIOD, pulse, 0U);
        }
    }
}

static void report_pca9685_status(void)
{
    for (size_t ctrl = 0; ctrl < ARRAY_SIZE(pca9685_controllers); ctrl++) {
        if (device_is_ready(pca9685_controllers[ctrl].dev)) {
            printf("PCA9685 ready at 0x%02X\r\n",
                   pca9685_controllers[ctrl].address);
        } else {
            printf("PCA9685 not ready at 0x%02X\r\n",
                   pca9685_controllers[ctrl].address);
        }
    }
}

static void run_pca9685_chase_step(size_t step)
{
    const size_t controller_index = step / PCA9685_CHANNEL_COUNT;
    const uint32_t channel = step % PCA9685_CHANNEL_COUNT;

    set_all_pca9685_channels(0U);

    if (controller_index < ARRAY_SIZE(pca9685_controllers) &&
        device_is_ready(pca9685_controllers[controller_index].dev)) {
        (void)pwm_set(pca9685_controllers[controller_index].dev, channel,
                      PCA9685_PERIOD, PCA9685_PERIOD / 2U, 0U);
    }
}

static void set_one_pca9685_channel(uint8_t channel, uint32_t pulse) {
    uint8_t device = channel / ARRAY_SIZE(pca9685_controllers);
    uint8_t channel_internal = channel % PCA9685_CHANNEL_COUNT;
    int err = pwm_set(pca9685_controllers[device].dev, channel_internal,
            PCA9685_PERIOD, pulse, 0U);
    if (err != 0) {
        printf("Setting PWM value failed with %d error code.", err);
    }
}

int main(void)
{
    int ret;
    uint32_t blink_count = 0U;
    size_t chase_step = 0U;

    setvbuf(stdout, NULL, _IONBF, 0);

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
    report_pca9685_status();
    set_all_pca9685_channels(0U);

    while (1) {
        ret = gpio_pin_toggle_dt(&led);
        if (ret < 0) {
            printf("Error: failed to toggle LED (%d)\n", ret);
            return 0;
        }

        run_pca9685_chase_step(chase_step);
        chase_step = (chase_step + 1U) %
                     (ARRAY_SIZE(pca9685_controllers) * PCA9685_CHANNEL_COUNT);

        blink_count++;
        if ((blink_count % 10U) == 0U) {
            printf("Heartbeat: LED blink running, chase step %u\r\n", (unsigned int)chase_step);
        }

        k_msleep(100);
    }
}

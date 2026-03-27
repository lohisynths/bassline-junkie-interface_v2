#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "Encoder.h"
#include "InputController.h"
#include "LEDS.h"

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

#define LED0_NODE DT_ALIAS(led0)

#if !DT_NODE_HAS_STATUS(LED0_NODE, okay)
#error "This board does not define a usable led0 alias"
#endif

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

static constexpr size_t input_thread_stack_size = 2048U;
static constexpr int input_thread_priority = -1;
static constexpr int input_poll_interval_ms = 5;

K_THREAD_STACK_DEFINE(input_thread_stack, input_thread_stack_size);
static struct k_thread input_thread_data;
static K_SEM_DEFINE(input_thread_started, 0, 1);

static InputController inputs;
static Encoder encoder;
static int input_thread_status = 0;

static void input_thread(void *, void *, void *)
{
    int ret = inputs.init();
    if (ret == 0) {
        ret = encoder.init(inputs, 0U, 1U, 2U);
    }

    input_thread_status = ret;
    k_sem_give(&input_thread_started);

    if (ret < 0) {
        LOG_ERR("Failed to initialize input thread: %d", ret);
        return;
    }

    int32_t previous_encoder_position = 0;

    while (1) {
        ret = inputs.update();
        if (ret < 0) {
            LOG_ERR("Failed to read inputs: %d", ret);
            return;
        }

        ret = encoder.update();
        if (ret < 0) {
            LOG_ERR("Failed to update encoder: %d", ret);
            return;
        }

        const int32_t current_encoder_delta = encoder.delta();
        const int32_t current_encoder_position = encoder.position();

        if ((current_encoder_delta != 0) ||
            (current_encoder_position != previous_encoder_position)) {
            LOG_INF("Encoder delta=%d position=%d",
                    (int)current_encoder_delta,
                    (int)current_encoder_position);
            previous_encoder_position = current_encoder_position;
        }

        k_msleep(input_poll_interval_ms);
    }
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

    LEDS leds;
    ret = leds.init();
    if (ret < 0) {
        LOG_ERR("Failed to initialize LED controllers: %d", ret);
        return 0;
    }

    k_thread_create(&input_thread_data,
                    input_thread_stack,
                    K_THREAD_STACK_SIZEOF(input_thread_stack),
                    input_thread,
                    nullptr,
                    nullptr,
                    nullptr,
                    input_thread_priority,
                    0,
                    K_NO_WAIT);

    k_sem_take(&input_thread_started, K_FOREVER);
    if (input_thread_status < 0) {
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
            LOG_INF("Heartbeat: LED blink running, chase step %u", (unsigned int)chase_step);
        }

        k_msleep(100);
    }
}

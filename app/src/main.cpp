#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "Button.h"
#include "InputController.h"
#include "Knob.h"
#include "LEDS.h"

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

#define LED0_NODE DT_ALIAS(led0)

#if !DT_NODE_HAS_STATUS(LED0_NODE, okay)
#error "This board does not define a usable led0 alias"
#endif

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

static const size_t input_thread_stack_size = 2048U;
static const int input_thread_priority = -1;
static const int input_poll_interval_ms = 5;
static const Button::Config button_configs[] = {
    {
        .mux_index = 0U,
        .pin = 12U,
        .led_number = 40U,
    },
    {
        .mux_index = 0U,
        .pin = 13U,
        .led_number = 41U,
    },
    {
        .mux_index = 0U,
        .pin = 14U,
        .led_number = 42U,
    },
    {
        .mux_index = 0U,
        .pin = 15U,
        .led_number = 43U,
    },
};
static const size_t button_count = ARRAY_SIZE(button_configs);

static const size_t knob_led_count = 10U;
static const Knob::Config knob_configs[] = {
    {
        .button_mux_index = 0U,
        .button_pin = 0U,
        .encoder_mux_index = 0U,
        .encoder_pin_a = 1U,
        .encoder_pin_b = 2U,
        .first_led = 0U,
        .led_count = knob_led_count,
    },
    {
        .button_mux_index = 0U,
        .button_pin = 3U,
        .encoder_mux_index = 0U,
        .encoder_pin_a = 4U,
        .encoder_pin_b = 5U,
        .first_led = 10U,
        .led_count = knob_led_count,
    },
    {
        .button_mux_index = 0U,
        .button_pin = 6U,
        .encoder_mux_index = 0U,
        .encoder_pin_a = 7U,
        .encoder_pin_b = 8U,
        .first_led = 20U,
        .led_count = knob_led_count,
    },
    {
        .button_mux_index = 0U,
        .button_pin = 9U,
        .encoder_mux_index = 0U,
        .encoder_pin_a = 10U,
        .encoder_pin_b = 11U,
        .first_led = 30U,
        .led_count = knob_led_count,
    },
};
static const size_t knob_count = ARRAY_SIZE(knob_configs);

K_THREAD_STACK_DEFINE(input_thread_stack, input_thread_stack_size);
static struct k_thread input_thread_data;
static K_SEM_DEFINE(input_thread_started, 0, 1);

static int input_thread_status = 0;

static void input_thread(void *, void *, void *) {
    InputController inputs;
    LEDSController leds;
    Button buttons[button_count];
    Knob knobs[knob_count];

    int ret = inputs.init();
    if (ret == 0) {
        ret = leds.init();
    }
    for (size_t i = 0U; (i < button_count) && (ret == 0); ++i) {
        ret = buttons[i].init(inputs, button_configs[i], leds);
    }
    for (size_t i = 0U; (i < knob_count) && (ret == 0); ++i) {
        ret = knobs[i].init(inputs, knob_configs[i], leds);
    }

    input_thread_status = ret;
    k_sem_give(&input_thread_started);

    if (ret < 0) {
        LOG_ERR("Failed to initialize input thread: %d", ret);
        return;
    }

    while (1) {
        ret = inputs.update();
        if (ret < 0) {
            LOG_ERR("Failed to read inputs: %d", ret);
            return;
        }
        inputs.log_mux_changes();

        for (size_t i = 0U; i < button_count; ++i) {
            Button::button_msg button_msg;
            ret = buttons[i].update(button_msg);
            if (ret < 0) {
                LOG_ERR("Failed to update button %u: %d", (unsigned int)i, ret);
                return;
            }
            if (button_msg.switch_changed) {
                const bool button_state = buttons[i].get_state();
                ret = buttons[i].set_led_val(button_state ? 100U : 0U);
                if (ret < 0) {
                    LOG_ERR("Failed to set button %u LED: %d", (unsigned int)i, ret);
                    return;
                }

                LOG_INF("Button %u mux=%u bit=%u %s",
                        (unsigned int)i,
                        (unsigned int)button_configs[i].mux_index,
                        (unsigned int)button_configs[i].pin,
                        button_state ? "pressed" : "released");
            }
        }

        for (size_t i = 0U; i < knob_count; ++i) {
            Knob::knob_msg msg;

            ret = knobs[i].update(msg);
            if (ret < 0) {
                LOG_ERR("Failed to update knob %u: %d", (unsigned int)i, ret);
                return;
            }

            if (msg.switch_changed) {
                LOG_INF("Knob %u button %s",
                        (unsigned int)i,
                        knobs[i].get_state() ? "pressed" : "released");
            }

            if (msg.value_changed) {
                LOG_INF("Knob %u position=%d",
                        (unsigned int)i,
                        (int)knobs[i].get_value());
            }
        }

        k_msleep(input_poll_interval_ms);
    }
}

int main(void)
{
    int ret;
    uint32_t blink_count = 0U;

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

        blink_count++;
        if ((blink_count % 10U) == 0U) {
            LOG_INF("Heartbeat: LED blink running");
        }

        k_msleep(100);
    }
}

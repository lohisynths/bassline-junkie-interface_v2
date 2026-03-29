#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "blocks/ADSR.h"
#include "blocks/FLT.h"
#include "blocks/LED_DISP.h"
#include "blocks/LFO.h"
#include "blocks/MOD.h"
#include "blocks/OSC.h"
#include "InputController.h"
#include "LEDS.h"
#include "MIDI.h"
#include "PresetStore.h"
#include "UART.h"

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

#define LED0_NODE DT_ALIAS(led0)

#if !DT_NODE_HAS_STATUS(LED0_NODE, okay)
#error "This board does not define a usable led0 alias"
#endif

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

static const size_t input_thread_stack_size = 10240U;
static const int input_thread_priority = -1;
static const int input_poll_interval_ms = 5;
K_THREAD_STACK_DEFINE(input_thread_stack, input_thread_stack_size);
static struct k_thread input_thread_data;
static K_SEM_DEFINE(input_thread_started, 0, 1);

static int input_thread_status = 0;

static void input_thread(void *p1, void *, void *) {
    MIDI *midi = static_cast<MIDI *>(p1);
    InputController inputs;
    LEDSController leds;
    ADSR adsr;
    FLT flt;
    LED_DISP led_disp;
    LFO lfo;
    MOD mod;
    OSC osc;
    PresetStore preset_store;

    int ret = inputs.init();
    if (ret == 0) {
        ret = leds.init();
    }
    if (ret == 0) {
        ret = adsr.init(inputs, leds, midi);
    }
    if (ret == 0) {
        ret = flt.init(inputs, leds, midi);
    }
    if (ret == 0) {
        ret = lfo.init(inputs, leds);
    }
    if (ret == 0) {
        ret = mod.init(inputs, leds);
    }
    if (ret == 0) {
        ret = osc.init(inputs, leds, midi);
    }
    if (ret == 0) {
        ret = preset_store.init();
    }
    if (ret == 0) {
        ret = led_disp.init(inputs, leds, preset_store, adsr, flt, lfo, mod, osc);
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

        ret = adsr.update();
        if (ret < 0) {
            return;
        }

        ret = flt.update();
        if (ret < 0) {
            return;
        }

        ret = lfo.update();
        if (ret < 0) {
            return;
        }

        ret = mod.update();
        if (ret < 0) {
            return;
        }

        ret = osc.update();
        if (ret < 0) {
            return;
        }

        ret = led_disp.update();
        if (ret < 0) {
            return;
        }

        if (mod.mod_knob_pressed()) {
            size_t knob_index = 0U;

            if (flt.take_newly_pressed_knob(knob_index)) {
                mod.report_link_target("FLT", knob_index, 0);
            }

            if (osc.take_newly_pressed_knob(knob_index)) {
                mod.report_link_target("OSC", knob_index, osc.selected_bank());
            }
        }

        k_msleep(input_poll_interval_ms);
    }
}

int main(void)
{
    int ret;
    uint32_t blink_count = 0U;
    UART uart1;
    MIDI midi;

    if (!gpio_is_ready_dt(&led)) {
        LOG_ERR("LED GPIO device is not ready");
        return 0;
    }

    ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        LOG_ERR("Failed to configure LED pin: %d", ret);
        return 0;
    }

    ret = uart1.init();
    if (ret < 0) {
        LOG_ERR("Failed to initialize UART1: %d", ret);
    } else {
        ret = uart1.write("Bassline Junkie Interface UART1 ready\r\n");
        if (ret < 0) {
            LOG_ERR("Failed to write UART1 startup banner: %d", ret);
        }

        ret = midi.init(uart1);
        if (ret < 0) {
            LOG_ERR("Failed to initialize MIDI transport: %d", ret);
        }
    }

    LOG_INF("Bassline Junkie Interface");
    LOG_INF("Console TX ready on ttyACM0");

    k_thread_create(&input_thread_data,
                    input_thread_stack,
                    K_THREAD_STACK_SIZEOF(input_thread_stack),
                    input_thread,
                    &midi,
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

        k_msleep(1000);
    }
}

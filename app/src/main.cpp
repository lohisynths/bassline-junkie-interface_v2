#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "blocks/OSC.h"
#include "blocks/ADSR.h"
#include "blocks/LFO.h"
#include "blocks/FLT.h"
#include "blocks/MOD.h"
#include "blocks/LED_DISP.h"
#include "blocks/VOL.h"
#include "wait_for_dsp.h"
#include "PresetStorage.h"
#include "blocks/Preset.h"
#include "MUX.h"
#include "LEDS.h"
#include "MIDI.h"
#include "UART.h"
#include "USB_MIDI.h"
#include "SDCard.h"

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
    UART uart1;
    MIDI midi;
    USB_MIDI usb_midi;
    MUX mux;
    LEDSController leds;
    OSC osc;
    ADSR adsr;
    LFO lfo;
    FLT flt;
    MOD mod;
    LED_DISP led_disp;
    VOL vol;
    SDCard sd_card;
    PresetStorage preset_storage;
    Preset preset;

    int ret = mux.init();
    if (ret < 0) {
        LOG_ERR("Failed to initialize input mux: %d", ret);
        input_thread_status = ret;
        k_sem_give(&input_thread_started);
        return;
    }
    ret = leds.init();
    if (ret < 0) {
        LOG_ERR("Failed to initialize LED controller: %d", ret);
        input_thread_status = ret;
        k_sem_give(&input_thread_started);
        return;
    }
    led_disp.init(leds);

    ret = sd_card.init();
    if (ret == 0) ret = preset_storage.init(sd_card);
    if (ret < 0) {
        LOG_ERR("Required SD preset storage is unavailable: %d", ret);
        led_disp.show_error_latched();
        input_thread_status = ret;
        k_sem_give(&input_thread_started);
        while (1) k_msleep(1000);
    }

    ret = uart1.init();
    if (ret < 0) {
        LOG_ERR("Failed to initialize UART1: %d", ret);
        input_thread_status = ret;
        k_sem_give(&input_thread_started);
        return;
    }
    ret = midi.init(uart1);
    if (ret < 0) {
        LOG_ERR("Failed to initialize MIDI transport: %d", ret);
        input_thread_status = ret;
        k_sem_give(&input_thread_started);
        return;
    }

    ret = usb_midi.init();
    if (ret < 0) {
        LOG_ERR("Failed to initialize USB MIDI: %d", ret);
        input_thread_status = ret;
        k_sem_give(&input_thread_started);
        return;
    }

    wait_for_dsp(uart1, leds);

    osc.init(midi, leds, mux, led_disp);
    adsr.init(midi, leds, mux, led_disp);
    lfo.init(midi, leds, mux, led_disp);
    flt.init(midi, leds, mux, led_disp);
    osc.bind_mod_capture(mod);
    flt.bind_mod_capture(mod);
    mod.init(midi, leds, mux, osc, flt, led_disp);
    vol.init(midi, leds, mux, led_disp);
    ret = preset.init(preset_storage, midi, leds, mux, led_disp, adsr, flt, lfo, mod, osc, vol);
    if (ret < 0) {
        LOG_ERR("Failed to initialize presets: %d", ret);
        led_disp.show_error_latched();
        input_thread_status = ret;
        k_sem_give(&input_thread_started);
        while (1) k_msleep(1000);
    }

    input_thread_status = 0;
    k_sem_give(&input_thread_started);

    while (1) {
        ret = mux.update();
        if (ret < 0) {
            LOG_ERR("Failed to read inputs: %d", ret);
            return;
        }

        mod.update();
        osc.update();
        adsr.update();
        lfo.update();
        flt.update();
        vol.update();
        mod.poll_mod_destination_selection();
        led_disp.update();
        preset.update();

        USB_MIDI::Message usb_msg;
        while (usb_midi.receive(&usb_msg) == 0) {
            switch (usb_msg.command) {
            case 0x9U:
                midi.send_note_on(usb_msg.data1, usb_msg.data2,
                                  usb_msg.channel);
                break;
            case 0x8U:
                midi.send_note_off(usb_msg.data1, usb_msg.data2,
                                   usb_msg.channel);
                break;
            case 0xBU:
                midi.send_cc(usb_msg.data1, usb_msg.data2,
                             usb_msg.channel);
                break;
            default:
                break;
            }
        }

        k_msleep(input_poll_interval_ms);
    }
}

int main(void)
{
    int ret;
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

        k_msleep(1000);
    }
}

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
#include "EEPROM.h"
#include "Preset.h"
#include "InputController.h"
#include "LEDS.h"
#include "MIDI.h"
#include "UART.h"
#include "USB_MIDI.h"

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

/**
 * @brief Blocks until the DSP engine sends its ready signal over UART.
 *
 * Polls UART for the 0xFE byte (MIDI Active Sensing) that the DSP engine
 * transmits as soon as its Engine constructor returns.  While waiting,
 * animates a bouncing dash on the 3-digit display: the middle segment (g)
 * of each digit is lit one at a time in a ping-pong sequence
 * (digit 2 → 1 → 0 → 1 → 2 …).
 *
 * The display is cleared on exit so that the subsequent preset load can
 * render the correct slot number cleanly.
 *
 * @param uart1 Initialised UART transport used to receive the signal.
 * @param leds  Initialised LED controller used to drive the display directly.
 */
static void wait_for_dsp_ready(UART &uart1, LEDSController &leds)
{
    static constexpr uint8_t  bounce_digits[] = {2U, 1U, 0U, 1U};
    static constexpr size_t   bounce_len      = 4U;
    static constexpr uint32_t frame_ms        = 200U;
    static constexpr uint8_t  seg_g_offset    = 6U;

    LOG_INF("Waiting for DSP engine ready signal (0xFE)...");

    // Clear all display segments.
    for (size_t i = 0U; i < (LED_DISP_SEGMENTS * LED_DISP_DIGIT_COUNT); ++i) {
        leds.set_channel_percent(LED_DISP_FIRST_LED + i, 100U);
    }

    // Light the first frame immediately so the display is not blank at entry.
    leds.set_channel_percent(
        LED_DISP_FIRST_LED + (bounce_digits[0U] * LED_DISP_SEGMENTS) + seg_g_offset,
        0U);

    size_t   bounce_frame  = 0U;
    uint32_t last_frame_at = k_uptime_get_32();

    uint8_t signal_byte = 0U;
    for (;;) {
        const int ret = uart1.read_byte(&signal_byte);
        if (ret == 0) {
            if (signal_byte == 0xFE) {
                break;
            }
            LOG_WRN("Unexpected byte while waiting for DSP ready: 0x%02X", signal_byte);
        }

        // Advance the animation frame every frame_ms milliseconds.
        const uint32_t now = k_uptime_get_32();
        if ((int32_t)(now - last_frame_at) >= (int32_t)frame_ms) {
            last_frame_at = now;
            bounce_frame  = (bounce_frame + 1U) % bounce_len;

            // Clear all display segments.
            for (size_t i = 0U; i < (LED_DISP_SEGMENTS * LED_DISP_DIGIT_COUNT); ++i) {
                leds.set_channel_percent(LED_DISP_FIRST_LED + i, 100U);
            }

            // Light segment g of the next bounce digit.
            leds.set_channel_percent(
                LED_DISP_FIRST_LED + (bounce_digits[bounce_frame] * LED_DISP_SEGMENTS) + seg_g_offset,
                0U);
        }

        k_msleep(10);
    }

    // Clear the display before preset.init() renders the slot number.
    for (size_t i = 0U; i < (LED_DISP_SEGMENTS * LED_DISP_DIGIT_COUNT); ++i) {
        leds.set_channel_percent(LED_DISP_FIRST_LED + i, 100U);
    }

    LOG_INF("DSP engine ready signal received (0xFE) - loading preset");
}

static void input_thread(void *p1, void *, void *) {
    UART uart1;
    MIDI midi;
    USB_MIDI usb_midi;
    InputController inputs;
    LEDSController leds;
    OSC osc;
    ADSR adsr;
    LFO lfo;
    FLT flt;
    MOD mod;
    LED_DISP led_disp;
    VOL vol;
    EEPROM eeprom;
    Preset preset;

    int ret = inputs.init();
    if (ret == 0) {
        ret = leds.init();
    }

    ret = uart1.init();
    if (ret < 0) {
        LOG_ERR("Failed to initialize UART1: %d", ret);
    } else {
        ret = midi.init(uart1);
        if (ret < 0) {
            LOG_ERR("Failed to initialize MIDI transport: %d", ret);
        }
    }

    ret = usb_midi.init();
    if (ret < 0) {
        LOG_ERR("Failed to initialize USB MIDI: %d", ret);
    }

    input_thread_status = ret;
    k_sem_give(&input_thread_started);

    if (ret < 0) {
        LOG_ERR("Failed to initialize input thread: %d", ret);
        return;
    }

    wait_for_dsp_ready(uart1, leds);

    osc.init(midi, leds, inputs);
    adsr.init(midi, leds, inputs);
    lfo.init(midi, leds, inputs);
    flt.init(midi, leds, inputs);
    mod.bind_sources(osc, flt);
    osc.bind_mod_capture(mod);
    flt.bind_mod_capture(mod);
    mod.init(midi, leds, inputs);
    led_disp.init(midi, leds, inputs);
    flt.bind_display(led_disp);
    vol.bind_display(led_disp);
    vol.bind_storage(eeprom);
    preset.init(eeprom, led_disp, adsr, flt, lfo, mod, osc);
    vol.init(midi, leds, inputs);

    while (1) {
        ret = inputs.update();
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

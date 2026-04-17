#include "wait_for_dsp.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <cstddef>
#include <cstdint>
#include <array>

#include "blocks/LED_DISP.h"
#include "LEDS.h"
#include "UART.h"

LOG_MODULE_REGISTER(wait_for_dsp, LOG_LEVEL_INF);

namespace {

using StartupDigitPattern = std::array<uint8_t, LED_DISP_SEGMENTS - 1U>;

static constexpr StartupDigitPattern kEyePattern = {{1U, 1U, 0U, 0U, 0U, 1U, 1U}};
static constexpr StartupDigitPattern kDashPattern = {{0U, 0U, 0U, 1U, 0U, 0U, 0U}};
static constexpr StartupDigitPattern kUPattern = {{0U, 0U, 1U, 1U, 1U, 0U, 0U}};

static void clear_startup_display(LEDSController &leds)
{
    for (size_t i = 0U; i < (LED_DISP_SEGMENTS * LED_DISP_DIGIT_COUNT); ++i) {
        leds.set_channel_percent(LED_DISP_FIRST_LED + i, 100U);
    }
}

static void set_startup_digit(LEDSController &leds, uint8_t digit_nr, const StartupDigitPattern &pattern)
{
    const size_t digit_base = LED_DISP_FIRST_LED + (static_cast<size_t>(digit_nr) * LED_DISP_SEGMENTS);

    for (size_t segment = 0U; segment < pattern.size(); ++segment) {
        leds.set_channel_percent(digit_base + segment, pattern[segment] ? 0U : 100U);
    }

    // Keep the decimal point off for the startup animation.
    leds.set_channel_percent(digit_base + (LED_DISP_SEGMENTS - 1U), 100U);
}

static void render_startup_face(LEDSController &leds, bool mouth_u)
{
    clear_startup_display(leds);
    set_startup_digit(leds, 0U, kEyePattern);
    set_startup_digit(leds, 1U, mouth_u ? kUPattern : kDashPattern);
    set_startup_digit(leds, 2U, kEyePattern);
}

} // namespace

void wait_for_dsp(UART &uart1, LEDSController &leds)
{
    static constexpr uint32_t frame_ms = 500U;

    LOG_INF("Waiting for DSP engine ready signal (0xFE)...");

    bool     mouth_u       = false;
    uint32_t last_frame_at = k_uptime_get_32();

    render_startup_face(leds, mouth_u);

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
            mouth_u = !mouth_u;
            render_startup_face(leds, mouth_u);
        }

        k_msleep(10);
    }

    // Clear the display before preset.init() renders the slot number.
    clear_startup_display(leds);

    LOG_INF("DSP engine ready signal received (0xFE) - loading preset");
}

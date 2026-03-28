#ifndef SRC_KNOB_H_
#define SRC_KNOB_H_

#include "Encoder.h"
#include "InputController.h"
#include "LEDS.h"

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Couples one encoder, one raw button input, and one LED segment into a knob UI.
 *
 * The knob borrows externally managed @ref InputController and @ref LEDSController
 * instances through @ref init. It owns its internal @ref Encoder object,
 * samples one configured active-low button bit directly from the cached input
 * state, updates one internal value in the range `[0, 127]` from encoder
 * movement, maps that value onto a contiguous LED range, and exposes the
 * current button state through @ref get_state.
 */
class Knob {
public:
    /**
     * @brief Reports which knob outputs changed during one @ref update call.
     */
    struct knob_msg {
        /** @brief Set when the clamped knob value changed. */
        bool value_changed = false;

        /** @brief Set when the knob push-button state changed. */
        bool switch_changed = false;
    };

    /**
     * @brief Groups the knob-specific binding and LED segment parameters.
     */
    struct Config {
        /** @brief Cached mux index containing the button bit. */
        size_t button_mux_index = 0U;

        /** @brief Channel number used for the button source bit. */
        uint8_t button_pin = 0U;

        /** @brief Cached mux index containing the encoder phases. */
        size_t encoder_mux_index = 0U;

        /** @brief Channel number used for encoder phase A. */
        uint8_t encoder_pin_a = 0U;

        /** @brief Channel number used for encoder phase B. */
        uint8_t encoder_pin_b = 0U;

        /** @brief First global LED channel reserved for this knob. */
        size_t first_led = 0U;

        /** @brief Number of contiguous LED channels reserved for this knob. */
        size_t led_count = 0U;
    };

    /**
     * @brief Constructs an unbound knob facade.
     */
    Knob() = default;

    /**
     * @brief Binds the knob to existing input and LED objects.
     *
     * @param inputs Shared input controller used by the button bit reader and encoder.
     * @param config Knob-specific channel and LED segment configuration.
     * @param leds Shared LED controller used to render the knob indicator.
     *
     * @retval 0 The knob configuration is valid and the initial LED was rendered.
     * @retval -EINVAL The button binding or LED range configuration is invalid.
     * @retval negative Error propagated from the internal @ref Encoder::init
     *         or @ref LEDSController::set_channel_percent.
     */
    int init(InputController &inputs, const Config &config, LEDSController &leds);

    /**
     * @brief Returns the current knob button state.
     *
     * @return `true` when the bound button is pressed, otherwise `false`.
     */
    bool get_state();

    /**
     * @brief Returns the current knob value.
     *
     * @return Clamped knob value in the range `[0, 127]`.
     */
    uint8_t get_value();

    /**
     * @brief Replaces the current knob value and redraws the LED indicator.
     *
     * @param value Requested knob value, clamped to the range `[0, 127]`.
     *
     * @retval 0 The stored value and LED indicator were updated successfully.
     * @retval -EACCES The knob has not been initialized.
     * @retval negative Error propagated from @ref LEDSController::set_channel_percent.
     */
    int set_value(uint8_t value);

    /**
     * @brief Refreshes the knob value and LED indicator from the encoder state.
     *
     * The internal value is advanced by the encoder delta, clamped to the range
     * `[0, 127]`, and then projected onto the configured LED range without
     * wraparound.
     *
     * @param msg Per-update change flags populated from the latest button and
     *        encoder processing.
     *
     * @retval 0 The knob indicator is up to date and @p msg was filled in.
     * @retval -EACCES The knob has not been initialized.
     * @retval negative Error propagated from @ref LEDSController::set_channel_percent.
     */
    int update(knob_msg &msg);

private:
    /**
     * @brief Renders the current knob value on the configured LED segment.
     *
     * @retval 0 The LED segment matches the current knob value.
     * @retval negative Error propagated from @ref LEDSController::set_channel_percent.
     */
    int render_current_value_();

    /**
     * @brief Maps one knob value to an LED index inside the knob segment.
     *
     * @param value Knob value to project into the configured LED range.
     *
     * @return LED index in the range `[0, led_count_)`.
     */
    size_t led_index_(uint8_t value);

    /** @brief Internal encoder owned by the knob. */
    Encoder encoder_;

    /** @brief Input controller borrowed from the caller for button reads. */
    InputController *inputs_ = nullptr;

    /** @brief Cached mux index containing the button bit. */
    size_t button_mux_index_ = 0U;

    /** @brief Channel number used for the button source bit. */
    uint8_t button_pin_ = 0U;

    /** @brief LED controller borrowed from the caller. */
    LEDSController *leds_ = nullptr;

    /** @brief First global LED channel reserved for this knob. */
    size_t first_led_ = 0U;

    /** @brief Number of contiguous LED channels reserved for this knob. */
    size_t led_count_ = 0U;

    /** @brief Tracks whether @ref init completed successfully. */
    bool initialized_ = false;

    /** @brief Current clamped knob value in the range `[0, 127]`. */
    uint8_t value_ = 0U;

    /** @brief Current sampled button state. */
    bool pressed_ = false;

    /** @brief Previously rendered LED index inside the knob segment. */
    size_t previous_led_index_ = 0U;
};

#endif /* SRC_KNOB_H_ */

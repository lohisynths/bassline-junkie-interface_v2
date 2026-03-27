#ifndef SRC_KNOB_H_
#define SRC_KNOB_H_

#include "Button.h"
#include "Encoder.h"
#include "InputController.h"
#include "LEDS.h"

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Couples one encoder, one button, and one LED segment into a knob UI.
 *
 * The knob borrows externally managed @ref InputController and @ref LEDS
 * instances through @ref init. It owns its internal @ref Button and
 * @ref Encoder objects, updates one internal value in the range `[0, 127]`
 * from encoder movement, maps that value onto a contiguous LED range, and
 * exposes the current button state through @ref get_state.
 */
class Knob {
public:
    /**
     * @brief Constructs an unbound knob facade.
     */
    Knob() = default;

    /**
     * @brief Binds the knob to existing input and LED objects.
     *
     * @param inputs Shared input controller used by the button and encoder.
     * @param button_state_index Cached input-state index containing the button bit.
     * @param button_pin Channel number used for the button source bit.
     * @param encoder_mux_index Cached mux index containing the encoder phases.
     * @param encoder_pin_a Channel number used for encoder phase A.
     * @param encoder_pin_b Channel number used for encoder phase B.
     * @param leds Shared LED controller used to render the knob indicator.
     * @param first_led First global LED channel reserved for this knob.
     * @param led_count Number of contiguous LED channels reserved for this knob.
     *
     * @retval 0 The knob configuration is valid and the initial LED was rendered.
     * @retval -EINVAL The LED range configuration is invalid.
     * @retval negative Error propagated from the internal @ref Button::init,
     *         internal @ref Encoder::init,
     *         or @ref LEDS::set_channel_percent.
     */
    int init(InputController &inputs,
             size_t button_state_index,
             uint8_t button_pin,
             size_t encoder_mux_index,
             uint8_t encoder_pin_a,
             uint8_t encoder_pin_b,
             LEDS &leds,
             size_t first_led,
             size_t led_count);

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
     * @brief Returns the most recent encoder delta observed by this knob.
     *
     * @return `-1`, `0`, or `1` from the internal encoder update.
     */
    int32_t get_delta();

    /**
     * @brief Refreshes the knob value and LED indicator from the encoder state.
     *
     * The internal value is advanced by the encoder delta, clamped to the range
     * `[0, 127]`, and then projected onto the configured LED range without
     * wraparound.
     *
     * @retval 0 The knob indicator is up to date.
     * @retval -EACCES The knob has not been initialized.
     * @retval negative Error propagated from @ref LEDS::set_channel_percent.
     */
    int update();

private:
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

    /** @brief Internal button owned by the knob. */
    Button button_;

    /** @brief LED controller borrowed from the caller. */
    LEDS *leds_ = nullptr;

    /** @brief First global LED channel reserved for this knob. */
    size_t first_led_ = 0U;

    /** @brief Number of contiguous LED channels reserved for this knob. */
    size_t led_count_ = 0U;

    /** @brief Tracks whether @ref init completed successfully. */
    bool initialized_ = false;

    /** @brief Current clamped knob value in the range `[0, 127]`. */
    uint8_t value_ = 0U;

    /** @brief Previously rendered LED index inside the knob segment. */
    size_t previous_led_index_ = 0U;
};

#endif /* SRC_KNOB_H_ */

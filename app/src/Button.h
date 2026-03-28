#ifndef SRC_BUTTON_H_
#define SRC_BUTTON_H_

#include "InputController.h"
#include "LEDS.h"

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Tracks one active-low button sourced from one cached input bit.
 *
 * The button is bound to one cached mux state inside an @ref InputController
 * and one LED channel inside an @ref LEDSController through @ref init, then
 * advanced by calling @ref update repeatedly after the controller has
 * refreshed its cached inputs.
 */
class Button {
public:
    /**
     * @brief Reports which button outputs changed during one @ref update call.
     */
    struct button_msg {
        /** @brief Set when the sampled button state changed. */
        bool switch_changed = false;
    };

    /**
     * @brief Binds the button to one cached mux state, one source channel, and one LED.
     *
     * The caller retains ownership of @p inputs and @p leds and must keep both
     * alive and initialized for the lifetime of this button instance.
     *
     * @param inputs Input controller holding the cached input masks.
     * @param mux_index Index of the cached mux state containing the button bit.
     * @param pin Channel number used for the button source bit.
     * @param leds LED controller used to mirror the button state.
     * @param led_number Global LED channel assigned to this button.
     *
     * @retval 0 The button configuration is valid and the LED was cleared.
     * @retval -EINVAL The mux index, channel number, or LED channel is out of range.
     * @retval negative Error propagated from @ref LEDSController::set_channel_percent.
     */
    int init(InputController &inputs,
             size_t mux_index,
             uint8_t pin,
             LEDSController &leds,
             size_t led_number);

    /**
     * @brief Reads the configured cached mux state and advances the button state.
     *
     * @param msg Per-update change flags populated from the latest sampled
     *        button state.
     *
     * @retval 0 The cached mux state was read and @p msg was filled in.
     * @retval -EACCES The button has not been initialized.
     */
    int update(button_msg &msg);

    /**
     * @brief Returns the current sampled button state.
     *
     * @return `true` when the configured raw input bit is `0` (pressed),
     *         otherwise `false`.
     */
    bool get_state();

private:
    /**
     * @brief Drives the LED assigned to this button.
     *
     * @param percent Brightness percentage in the range `[0, 100]`.
     *
     * @retval 0 The LED was updated successfully.
     * @retval -EACCES The button LED binding has not been initialized.
     * @retval negative Error propagated from @ref LEDSController::set_channel_percent.
     */
    int set_led_val(uint8_t percent);

    /** @brief Borrowed input controller used to read cached input states. */
    InputController *inputs_ = nullptr;

    /** @brief Borrowed LED controller used to mirror the button state. */
    LEDSController *leds_ = nullptr;

    /** @brief Index of the configured cached mux state inside @ref inputs_. */
    size_t mux_index_ = 0U;

    /** @brief InputController channel number carrying the button bit. */
    uint8_t pin_ = 0U;

    /** @brief Global LED channel assigned to this button. */
    size_t led_number_ = 0U;

    /** @brief Current sampled button state. */
    bool pressed_ = false;
};

#endif /* SRC_BUTTON_H_ */

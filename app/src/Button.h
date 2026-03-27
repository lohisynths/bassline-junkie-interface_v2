#ifndef SRC_BUTTON_H_
#define SRC_BUTTON_H_

#include "InputController.h"

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Tracks one active-low button sourced from one cached input bit.
 *
 * The button is bound to one cached state inside an @ref InputController
 * through @ref init and then advanced by calling @ref update repeatedly after
 * the controller has refreshed its cached inputs.
 */
class Button {
public:
    /**
     * @brief Binds the button to one cached state and one source channel.
     *
     * The caller retains ownership of @p inputs and must keep it alive and
     * initialized for the lifetime of this button instance.
     *
     * @param inputs Input controller holding the cached input masks.
     * @param state_index Index of the cached state containing the button bit.
     * @param pin Channel number used for the button source bit.
     *
     * @retval 0 The button configuration is valid.
     * @retval -EINVAL The state index or channel number is out of range.
     */
    int init(InputController &inputs, size_t state_index, uint8_t pin);

    /**
     * @brief Reads the configured cached state and advances the button state.
     *
     * @retval 0 The cached state was read and the button state was updated.
     * @retval -EACCES The button has not been initialized.
     */
    int update();

    /**
     * @brief Returns the current sampled button state.
     *
     * @return `true` when the configured raw input bit is `0` (pressed),
     *         otherwise `false`.
     */
    bool get_state();

private:
    /** @brief Borrowed input controller used to read cached input states. */
    InputController *inputs_ = nullptr;

    /** @brief Index of the configured cached state inside @ref inputs_. */
    size_t state_index_ = 0U;

    /** @brief InputController channel number carrying the button bit. */
    uint8_t pin_ = 0U;

    /** @brief Current sampled button state. */
    bool pressed_ = false;
};

#endif /* SRC_BUTTON_H_ */

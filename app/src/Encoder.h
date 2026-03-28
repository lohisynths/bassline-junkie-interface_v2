#ifndef SRC_ENCODER_H_
#define SRC_ENCODER_H_

#include "InputController.h"

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Decodes a quadrature encoder sourced from any cached input-state bits.
 *
 * The encoder is bound to one cached input-state entry inside an
 * @ref InputController through @ref init and then advanced by calling
 * @ref update repeatedly after the controller has refreshed its cached inputs.
 * One reported step in @ref delta or @ref position corresponds to one valid
 * quadrature edge.
 */
class Encoder {
public:
    /**
     * @brief Binds the decoder to one cached input-state entry and two source channels.
     *
     * The caller retains ownership of @p inputs and must keep it alive and
     * initialized for the lifetime of this encoder instance.
     *
     * @param inputs Input controller holding the cached input masks.
     * @param mux_index Index of the cached input-state entry containing the encoder.
     * @param pin_a Channel number used for encoder phase A.
     * @param pin_b Channel number used for encoder phase B.
     *
     * @retval 0 The encoder configuration is valid.
     * @retval -EINVAL The cached state index or one or more channel numbers are
     *         out of range, or the two channels are duplicated.
     */
    int init(InputController &inputs, size_t mux_index, uint8_t pin_a, uint8_t pin_b);

    /**
     * @brief Reads the configured cached input state and advances the decoder.
     *
     * The most recent quarter-step movement can then be read with @ref delta.
     * If no valid edge was observed since the previous @ref update call,
     * @ref delta returns `0`.
     *
     * @retval 0 The cached input state was read and the decoder was updated.
     * @retval -EACCES The encoder has not been initialized.
     */
    int update();

    /**
     * @brief Returns the most recent quarter-step movement.
     *
     * @return `-1`, `0`, or `1` for the movement observed during the previous
     *         @ref update call.
     */
    int32_t delta() const;

    /**
     * @brief Returns the accumulated full-detent position.
     *
     * @return Signed position counter advanced by one per valid quadrature edge.
     */
    int32_t position() const;

private:
    /**
     * @brief Maps one AB transition to a quarter-step delta.
     *
     * @param previous_ab Previously sampled AB state packed into bits 1:0.
     * @param current_ab Currently sampled AB state packed into bits 1:0.
     *
     * @return `-1`, `0`, or `1` for reverse, invalid/no movement, or forward.
     */
    static int8_t transition_(uint8_t previous_ab, uint8_t current_ab);

    /** @brief Borrowed input controller used to read cached input states. */
    InputController *inputs_ = nullptr;

    /** @brief Index of the configured cached input state inside @ref inputs_. */
    size_t mux_index_ = 0U;

    /** @brief InputController channel number carrying encoder phase A. */
    uint8_t pin_a_ = 0U;

    /** @brief InputController channel number carrying encoder phase B. */
    uint8_t pin_b_ = 0U;

    /** @brief Tracks whether one initial AB sample has been captured. */
    bool seeded_ = false;

    /** @brief Previously sampled AB state packed into bits 1:0. */
    uint8_t previous_ab_ = 0U;

    /** @brief Most recent quarter-step movement from @ref update. */
    int32_t delta_ = 0;

    /** @brief Running quarter-step position since the last @ref init. */
    int32_t position_ = 0;
};

#endif /* SRC_ENCODER_H_ */

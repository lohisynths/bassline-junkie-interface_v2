#ifndef SRC_BLOCKS_MOD_H_
#define SRC_BLOCKS_MOD_H_

#include "Button.h"
#include "InputController.h"
#include "Knob.h"
#include "LEDS.h"
#include "PresetSnapshot.h"

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Encapsulates the MOD control surface block.
 *
 * The block owns one knob and six selector buttons. The selector buttons pick
 * one of six top-level groups, and each group stores one value for seventeen
 * virtual link-target banks chosen through @ref report_link_target.
 */
class MOD {
public:
    /**
     * @brief Initializes all buttons and the knob in the MOD block.
     *
     * @param inputs Shared input controller used by all block controls.
     * @param leds Shared LED controller used by all block controls.
     *
     * @retval 0 All controls were initialized successfully.
     * @retval negative Error propagated from @ref Button::init or @ref Knob::init.
     */
    int init(InputController &inputs, LEDSController &leds);

    /**
     * @brief Captures the current durable MOD block state.
     */
    void capture_state(MODState &state) const;

    /**
     * @brief Applies one durable MOD block state snapshot.
     *
     * @retval 0 The requested state was applied successfully.
     * @retval negative Error propagated from LED or knob updates.
     */
    int apply_state(const MODState &state);

    /**
     * @brief Updates all controls and emits the current transition logs.
     *
     * @retval 0 All controls were updated successfully.
     * @retval negative Error propagated from a contained control update.
     */
    int update();

    /**
     * @brief Returns the current pressed state of the MOD knob button.
     */
    bool mod_knob_pressed() const;

    /**
     * @brief Selects one MOD link target inside the active group.
     *
     * @param block_name Name of the block that owns the pressed knob.
     * @param knob_index Zero-based knob index inside that block.
     * @param bank_index Active bank index for the owning block.
     */
    void report_link_target(const char *block_name, size_t knob_index, uint8_t bank_index);

    /**
     * @brief Returns whether OSC/FLT LED preview mode is currently active.
     */
    bool osc_flt_led_preview_active() const;

    /**
     * @brief Returns the MOD group currently being previewed.
     */
    uint8_t preview_group_index() const;

    /**
     * @brief Returns one stored MOD value for the requested preview target.
     *
     * @param group_index MOD group index in the range `[0, selector_group_count_)`.
     * @param target_offset Target offset in the range `[0, targets_per_group_)`.
     *
     * @return Stored MOD value for that virtual bank, or `0` when out of range.
     */
    uint8_t preview_value_for_target_offset(uint8_t group_index, uint8_t target_offset) const;

private:
    /** @brief Number of LEDs reserved for the knob segment. */
    static const size_t knob_led_count_ = 10U;

    /** @brief Hold time required to enable OSC/FLT LED preview. */
    static const int64_t preview_hold_ms_ = 1000;

    /** @brief Number of selector groups exposed by the buttons. */
    static const size_t selector_group_count_ = 6U;

    /** @brief Number of virtual link targets stored per selector group. */
    static const size_t targets_per_group_ = 17U;

    /** @brief Total number of stored virtual MOD banks. */
    static const size_t virtual_bank_count_ = selector_group_count_ * targets_per_group_;

    /** @brief Static configuration for the selector buttons. */
    static constexpr Button::Config button_configs_[] = {
        {
            .mux_index = 2U,
            .pin = 8U,
            .led_number = 127U,
        },
        {
            .mux_index = 2U,
            .pin = 7U,
            .led_number = 126U,
        },
        {
            .mux_index = 2U,
            .pin = 6U,
            .led_number = 125U,
        },
        {
            .mux_index = 2U,
            .pin = 5U,
            .led_number = 124U,
        },
        {
            .mux_index = 2U,
            .pin = 4U,
            .led_number = 123U,
        },
        {
            .mux_index = 2U,
            .pin = 3U,
            .led_number = 122U,
        },
    };

    /** @brief Static configuration for the MOD knob control. */
    static constexpr Knob::Config knob_configs_[] = {
        {
            .button_mux_index = 2U,
            .button_pin = 0U,
            .encoder_mux_index = 2U,
            .encoder_pin_a = 1U,
            .encoder_pin_b = 2U,
            .first_led = 112U,
            .led_count = knob_led_count_,
        },
    };

    /** @brief Number of selector buttons in this block. */
    static const size_t button_count_ = ARRAY_SIZE(button_configs_);

    /** @brief Number of knobs in this block. */
    static const size_t knob_count_ = ARRAY_SIZE(knob_configs_);

    /**
     * @brief Selects one top-level MOD group and recalls its remembered target.
     *
     * @param group_index Group index in the range `[0, selector_group_count_)`.
     *
     * @retval 0 The group was selected and its remembered target was recalled successfully.
     * @retval -EINVAL The requested group index is out of range.
     * @retval negative Error propagated from LED or knob updates.
     */
    int select_group_(size_t group_index);

    /**
     * @brief Updates the selector-button LEDs to show the currently active group.
     *
     * @retval 0 All selector LEDs match the selected group.
     * @retval negative Error propagated from @ref Button::set_led_val.
     */
    int update_selector_leds_();

    /**
     * @brief Loads one stored virtual bank into the live knob object.
     *
     * @param virtual_bank_index Virtual bank index in the range `[0, virtual_bank_count_)`.
     *
     * @retval 0 The knob value was recalled successfully.
     * @retval -EINVAL The requested virtual bank index is out of range.
     * @retval negative Error propagated from @ref Knob::set_value.
     */
    int recall_virtual_bank_to_knobs_(size_t virtual_bank_index);

    /**
     * @brief Maps one reported link target onto a per-group target offset.
     *
     * @param block_name Name of the source block.
     * @param knob_index Zero-based knob index inside the source block.
     * @param bank_index Active bank index of the source block.
     * @param target_offset Receives the resolved target offset in `[0, targets_per_group_)`.
     *
     * @retval true The source maps onto one valid MOD target offset.
     * @retval false The source is unsupported and should be ignored.
     */
    bool target_offset_for_link_(const char *block_name,
                                 size_t knob_index,
                                 uint8_t bank_index,
                                 uint8_t &target_offset) const;

    /**
     * @brief Converts one group/target pair into the stored virtual-bank index.
     *
     * @param group_index Top-level MOD group index.
     * @param target_offset Per-group target offset.
     *
     * @return Virtual bank index in `[0, virtual_bank_count_)`.
     */
    size_t virtual_bank_index_(size_t group_index, uint8_t target_offset) const;

    /**
     * @brief Returns the currently visible virtual bank index.
     */
    size_t current_virtual_bank_index_() const;

    /** @brief Selector buttons owned by the block. */
    Button buttons_[button_count_];

    /** @brief Knob control owned by the block. */
    Knob knobs_[knob_count_];

    /** @brief Currently selected top-level MOD group. */
    uint8_t selected_group_ = 0U;

    /** @brief Timestamp when the currently tracked preview hold began. */
    int64_t preview_started_at_ms_ = 0;

    /** @brief Selector button currently tracked for preview hold timing. */
    uint8_t preview_button_index_ = 0U;

    /** @brief Set when OSC/FLT LED preview mode is currently active. */
    bool preview_active_ = false;

    /** @brief Set while the tracked preview button is still physically pressed. */
    bool preview_button_still_pressed_ = false;

    /** @brief Remembers the last linked target offset for each selector group. */
    uint8_t selected_target_offset_[selector_group_count_] = {};

    /** @brief Stored knob values for each virtual bank. */
    uint8_t knob_values_[virtual_bank_count_][knob_count_] = {};
};

#endif /* SRC_BLOCKS_MOD_H_ */

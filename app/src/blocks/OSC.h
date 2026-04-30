/**
 * @file OSC.h
 * @brief Oscillator control-surface block.
 *
 * Created on: Mar 29, 2026
 *     Author: alax
 */

#ifndef APP_SRC_BLOCKS_OSC_H_
#define APP_SRC_BLOCKS_OSC_H_

#include "LED_DISP.h"
#include "ModMatrixCapture.h"

/** @brief Number of encoder knobs in the OSC block. */
static constexpr uint8_t OSC_KNOB_COUNT = 5U;

/** @brief Number of bank-selector buttons in the OSC block. */
static constexpr uint8_t OSC_BUTTON_COUNT = 3U;

/** @brief Number of banked instances (selector positions) in the OSC block. */
static constexpr uint8_t OSC_COUNT = 3U;

/** @brief MIDI offset */
static constexpr uint8_t OSC_MIDI_OFFSET = 0U;

/**
 * @brief Logical parameter indices for one OSC bank.
 */
enum OSC_PARAMS {
    OSC_PITCH,
    OSC_SIN,
    OSC_SAW,
    OSC_SQR,
    OSC_RND,
    OSC_PARAM_COUNT
};

/**
 * @brief Oscillator block owning five knobs and three bank-selector buttons.
 *
 * Hardware wiring is declared in the two public constexpr config tables and
 * consumed automatically by @ref UI_BLOCK::init through CRTP.
 *
 * All three buttons are bank selectors (`BUTTON_COUNT == COUNT`), so
 * @ref select_function is never reached at runtime. MIDI CC numbers are
 * laid out as `instance * OSC_PARAM_COUNT + index`, giving CC `0..14`
 * across the three banks on MIDI channel `0`.
 */
class OSC : public UI_BLOCK<OSC, OSC_KNOB_COUNT, OSC_BUTTON_COUNT, OSC_PARAM_COUNT, OSC_COUNT> {
public:
    /** @brief Static block name used by the shared CRTP base logging. */
    static constexpr const char *block_name_ = "OSC";

    /** @brief MIDI CC offset for the OSC block. */
    static constexpr uint8_t midi_offset_ = OSC_MIDI_OFFSET;

    /** @brief MIDI channel used for OSC control-change messages. */
    static constexpr uint8_t midi_channel_ = 1U;

    /**
     * @brief Binds the MOD viewer-edit sink used to repurpose OSC knobs while
     *        the MOD viewer is active.
     *
     * @param capture MOD-owned route-capture sink.
     */
    void bind_mod_capture(ModMatrixCapture &capture) {
        mod_capture_ = &capture;
    }

    /**
     * @brief Initializes the block and binds the shared preview display.
     *
     * @param midi MIDI backend borrowed by the CRTP base block.
     * @param leds LED controller passed through to the CRTP base block.
     * @param inputs Input controller used to initialize the block hardware.
     * @param display Display block used for temporary value previews.
     */
    void init(MIDI &midi, LEDSController &leds, InputController &inputs, LED_DISP &display) {
        UI_BLOCK<OSC, OSC_KNOB_COUNT, OSC_BUTTON_COUNT, OSC_PARAM_COUNT, OSC_COUNT>::init(midi, leds, inputs);
        display_ = &display;
    }

    /** @brief Mux/LED bindings for the three bank-selector buttons. */
    static constexpr std::array button_configs_ = {
        Button::Config{ .mux_index = 3U, .pin = 3U, .led_number = 110U },
        Button::Config{ .mux_index = 3U, .pin = 2U, .led_number = 109U },
        Button::Config{ .mux_index = 3U, .pin = 1U, .led_number = 108U },
    };

    /** @brief Encoder/button/LED bindings for the five knobs. */
    static constexpr std::array knob_configs_ = {
        Knob::Config{
            .button_mux_index = 1U,
            .button_pin = 12U,
            .encoder_mux_index = 1U,
            .encoder_pin_a = 13U,
            .encoder_pin_b = 14U,
            .first_led = 96U,
            .led_count = 10U,
        },
        Knob::Config{
            .button_mux_index = 1U,
            .button_pin = 9U,
            .encoder_mux_index = 1U,
            .encoder_pin_a = 10U,
            .encoder_pin_b = 11U,
            .first_led = 78U,
            .led_count = 10U,
        },
        Knob::Config{
            .button_mux_index = 1U,
            .button_pin = 6U,
            .encoder_mux_index = 1U,
            .encoder_pin_a = 7U,
            .encoder_pin_b = 8U,
            .first_led = 68U,
            .led_count = 10U,
        },
        Knob::Config{
            .button_mux_index = 1U,
            .button_pin = 3U,
            .encoder_mux_index = 1U,
            .encoder_pin_a = 4U,
            .encoder_pin_b = 5U,
            .first_led = 58U,
            .led_count = 10U,
        },
        Knob::Config{
            .button_mux_index = 1U,
            .button_pin = 0U,
            .encoder_mux_index = 1U,
            .encoder_pin_a = 1U,
            .encoder_pin_b = 2U,
            .first_led = 48U,
            .led_count = 10U,
        },
    };

    /**
     * @brief Returns the currently selected oscillator bank index.
     *
     * @return Active OSC bank index.
     */
    uint8_t get_current_osc() { return get_current_instance(); }

    /**
     * @brief Handles OSC bank-selector presses while the MOD viewer borrows
     *        the OSC knobs.
     *
     * The bank switch updates the visible OSC destination range, so during MOD
     * viewer mode the new bank is recalled silently and the viewer overlay is
     * repainted immediately instead of briefly rendering the underlying preset
     * values.
     *
     * @param index Raw selector button index.
     *
     * @retval true The selector press was handled here.
     * @retval false The base class should run the normal selector path.
     */
    bool handle_selector_press(uint8_t index) {
        if ((mod_capture_ == nullptr) || !mod_capture_->is_mod_viewer_active()) {
            return false;
        }

        recall_instance(index, false);
        mod_capture_->refresh_mod_viewer();
        return true;
    }

    /**
     * @brief Stores a knob change or forwards it into the active MOD viewer.
     *
     * @param index Knob index that changed.
     * @param value_scaled New clamped knob value.
     */
    void knob_val_changed(uint8_t index, uint8_t value_scaled) {
        if ((mod_capture_ != nullptr) &&
            mod_capture_->capture_osc_route_value(get_current_osc(), index, value_scaled)) {
            if (display_ != nullptr) {
                display_->show_preview_value(value_scaled);
            }
            return;
        }

        UI_BLOCK<OSC, OSC_KNOB_COUNT, OSC_BUTTON_COUNT, OSC_PARAM_COUNT, OSC_COUNT>::knob_val_changed(index,
                                                                                                       value_scaled);

        if (display_ != nullptr) {
            display_->show_preview_value(value_scaled);
        }
    }

    /**
     * @brief Shows one MOD routing row on the five OSC knobs during MOD viewer
     *        edit mode.
     *
     * The viewer uses the currently selected OSC bank as the destination range
     * and repurposes the visible OSC knobs to edit route amounts instead of OSC
     * parameters until the viewer is released.
     *
     * @param source MOD source index to preview.
     */
    void show_mod_view(uint8_t source) {
        const uint8_t bank = get_current_instance();
        auto &knobs = get_knobs();

        for (uint8_t i = 0U; i < OSC_KNOB_COUNT; i++) {
            const uint8_t dst = static_cast<uint8_t>(bank * OSC_KNOB_COUNT + i);
            knobs[i].show_preview_value(get_preset_mod_value(source, dst));
        }
    }

    /**
     * @brief Restores the OSC knob LEDs to the block's real preset values.
     */
    void clear_mod_view() {
        reset();
    }

private:
    /** @brief Borrowed display used to preview knob changes. */
    LED_DISP *display_ = nullptr;

    /** @brief Borrowed MOD viewer-edit sink, or `nullptr` when unbound. */
    ModMatrixCapture *mod_capture_ = nullptr;
};

#endif /* APP_SRC_BLOCKS_OSC_H_ */

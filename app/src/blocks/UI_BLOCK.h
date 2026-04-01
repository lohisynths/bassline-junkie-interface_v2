/**
 * @file UI_BLOCK.h
 * @brief CRTP base class for banked control-surface blocks.
 *
 * Created on: Mar 29, 2026
 *     Author: alax
 */

#ifndef APP_SRC_BLOCKS_UI_BLOCK_H_
#define APP_SRC_BLOCKS_UI_BLOCK_H_

#include <cstdint>
#include <cstring>
#include <array>

#include "Button.h"
#include "Knob.h"
#include "MIDI.h"
#include "InputController.h"
#include "LEDS.h"

#include <zephyr/logging/log.h>

/** @brief Number of modulation source groups used by the mod preset layout. */
static constexpr uint8_t MOD_SRC_COUNT = 6U;

/**
 * @brief CRTP base class providing banked knob/button wiring, preset storage,
 *        mod-routing overlay, and MIDI dump logic.
 *
 * Each derived class supplies its own hardware configuration by defining two
 * public static constexpr arrays named @c knob_configs_ and
 * @c button_configs_. The base class pulls those tables at compile time
 * through the @p Derived template parameter, so the public @ref init call
 * needs only the shared runtime objects.
 *
 * ## Required derived-class methods (CRTP dispatch, compile error if missing)
 *
 * | Signature                                                       | Purpose                                     |
 * |-----------------------------------------------------------------|---------------------------------------------|
 * | `const char *get_name()`                                        | Block name used in log output               |
 * | `uint8_t get_midi_nr(uint8_t instance, uint8_t index)`          | Maps bank + param index to MIDI CC number   |
 * | `uint8_t get_midi_ch()`                                         | MIDI channel used for CC emission           |
 *
 * ## Optional derived-class overrides (shadowed via CRTP, defaults provided)
 *
 * | Signature                                                       | Default behaviour                                            |
 * |-----------------------------------------------------------------|--------------------------------------------------------------|
 * | `void knob_val_changed(uint8_t index, uint8_t value_scaled)`    | Stores value in current bank and sends MIDI CC               |
 * | `void knob_sw_changed(uint8_t index, bool state)`               | Empty                                                        |
 * | `void select_function(uint8_t index)`                           | Empty                                                        |
 * | `void force_function(uint8_t value)`                            | Empty                                                        |
 *
 * @tparam Derived      Concrete block type (CRTP), must expose
 *                      @c knob_configs_ and @c button_configs_.
 * @tparam KNOB_COUNT   Number of knobs owned by the block.
 * @tparam BUTTON_COUNT Number of selector/radio buttons owned by the block.
 * @tparam PARAM_COUNT  Number of logical parameters exposed per bank.
 * @tparam COUNT        Number of banked instances (selector positions).
 */
template<typename Derived, uint8_t KNOB_COUNT, uint8_t BUTTON_COUNT,
         uint8_t PARAM_COUNT, uint8_t COUNT>
class UI_BLOCK {
public:
    /** @brief Banked preset storage: @c COUNT banks of @c PARAM_COUNT parameters each. */
    using preset = std::array<std::array<uint8_t, PARAM_COUNT>, COUNT>;

    /** @brief Mod-routing preset storage indexed by source group and destination. */
    using mod_preset = std::array<std::array<uint8_t, PARAM_COUNT * MOD_SRC_COUNT>, MOD_SRC_COUNT>;

    /** @brief Button array - alias for the block's owned button storage**/
    using button_array = std::array<Button, BUTTON_COUNT>;

    /** @brief Knob array - alias for the block's owned knob storage**/
    using knob_array = std::array<Knob, KNOB_COUNT>;

    /**
     * @brief Reports which outputs changed during one @ref update call.
     */
    struct ret_value {
        /** @brief Set when any button state changed. */
        bool buttons_changed = false;

        /** @brief Per-button pressed state sampled during this update. */
        std::array<bool, BUTTON_COUNT> buttons = {};

        /** @brief Set when any knob push-button state changed. */
        bool knobs_sw_changed = false;

        /** @brief Per-knob push-button pressed state sampled during this update. */
        std::array<bool, KNOB_COUNT> knobs_sw = {};

        /** @brief Set when any knob value changed. */
        bool knobs_changed = false;

        /** @brief Per-knob push-button change flags for this update. */
        std::array<bool, KNOB_COUNT> knobs_sw_changed_array = {};

        /** @brief Per-knob value sampled during this update. */
        std::array<uint8_t, KNOB_COUNT> knobs_val = {};

        /**
         * @brief Returns the index of the first knob whose push-button
         *        transitioned to pressed during this update.
         *
         * @return Knob index, or @c -1 if no knob push-button was pressed.
         */
        uint8_t get_first_pushed_knob_sw() {
            for (uint8_t i = 0; i < KNOB_COUNT; i++) {
                if (knobs_sw_changed_array[i] && knobs_sw[i]) {
                    return i;
                }
            }
            return -1;
        }
    };

    /** @brief Mod-routing preset values (public for direct access by MOD block). */
    mod_preset mod_preset_values = {};

    /** @brief When @c true, preset accessors route through mod-routing storage. */
    bool mod_viewer_mode = false;

    UI_BLOCK() = default;

    /**
     * @brief Binds every knob and button to the shared input and LED objects,
     *        then selects bank 0.
     *
     * Hardware configuration is read from @c Derived::knob_configs_ and
     * @c Derived::button_configs_ at compile time, so no config arrays
     * need to be passed at the call site.
     *
     * @param midi   MIDI transport used for control-change emission (may be @c nullptr).
     * @param leds   Shared LED controller for indicator output.
     * @param inputs Shared input controller holding the cached input masks.
     */
    void init(MIDI &midi, LEDSController &leds, InputController &inputs) {
        midi_ = &midi;

        for (uint8_t i = 0; i < KNOB_COUNT; i++) {
            knobs_[i].init(inputs, Derived::knob_configs_[i], leds);
        }
        for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
            buttons_[i].init(inputs, Derived::button_configs_[i], leds);
        }

        select_instance(current_instance_);
    }

    /**
     * @brief Polls all buttons and knobs, dispatches change callbacks.
     *
     * @return Change flags from this update cycle.
     */
    ret_value update() {
        last_ret_ = {};
        update_buttons(last_ret_);
        update_knobs(last_ret_);
        return last_ret_;
    }

    /* ------------------------------------------------------------------ */
    /*  Bank / instance accessors                                         */
    /* ------------------------------------------------------------------ */

    /** @brief Returns the currently selected bank index. */
    uint8_t get_current_instance() const {
        return current_instance_;
    }

    /** @brief Turns off the LED of the button at @p index. */
    void turn_off_sw(uint8_t index) {
        buttons_[index].set_led_val(0);
    }

    /** @brief Turns on the LED of the button at @p index to full brightness. */
    void turn_on_sw(uint8_t index) {
        buttons_[index].set_led_val(100);
    }

    /* ------------------------------------------------------------------ */
    /*  Preset value accessors                                            */
    /* ------------------------------------------------------------------ */

    /** @brief Returns the value of parameter @p index in the current bank. */
    uint8_t get_current_preset_value(uint8_t index) {
        return get_preset_value(current_instance_, index);
    }

    /**
     * @brief Returns one parameter value from the specified bank.
     *
     * When @ref mod_viewer_mode is active the read is redirected to the
     * mod-routing storage.
     */
    uint8_t get_preset_value(uint8_t instance, uint8_t index) {
        if (!mod_viewer_mode) {
            return preset_values_[instance][index];
        }
        return self()->get_preset_mod_value(instance, index);
    }

    /** @brief Stores one parameter value in the current bank. */
    void set_current_preset_value(uint8_t index, uint8_t value) {
        set_preset_value(current_instance_, index, value);
    }

    /**
     * @brief Stores one parameter value in the specified bank.
     *
     * When @ref mod_viewer_mode is active the write is redirected to the
     * mod-routing storage.
     */
    void set_preset_value(uint8_t instance, uint8_t index, uint8_t value) {
        if (!mod_viewer_mode) {
            preset_values_[instance][index] = value;
        } else {
            self()->set_preset_mod_value(instance, index, value);
        }
    }

    /* ------------------------------------------------------------------ */
    /*  Mod-routing value accessors                                       */
    /* ------------------------------------------------------------------ */

    /** @brief Returns one mod-routing value addressed by @p src and @p dst. */
    int16_t get_preset_mod_value(uint8_t src, uint8_t dst) {
        return mod_preset_values[src][dst];
    }

    /** @brief Stores one mod-routing value addressed by @p src and @p dst. */
    void set_preset_mod_value(uint8_t src, uint8_t dst, uint8_t value) {
        mod_preset_values[src][dst] = value;
    }

    /* ------------------------------------------------------------------ */
    /*  Component accessors                                               */
    /* ------------------------------------------------------------------ */

    /** @brief Returns a reference to the owned button array. */
    button_array &get_buttons() { return buttons_; }

    /** @brief Returns a reference to the owned knob array. */
    std::array<Knob, KNOB_COUNT> &get_knobs() { return knobs_; }

    /** @brief Returns the borrowed MIDI transport pointer. */
    MIDI *get_midi() { return midi_; }

    /* ------------------------------------------------------------------ */
    /*  Preset bulk operations                                            */
    /* ------------------------------------------------------------------ */

    /**
     * @brief Loads a full preset, re-selects the current bank, and dumps
     *        all parameters as MIDI CC.
     */
    void set_preset(const preset &input) {
        preset_values_ = input;
        select_instance(current_instance_);
        dump_midi();
    }

    /**
     * @brief Loads a full mod-routing preset, re-selects the current bank,
     *        and dumps all parameters as MIDI CC.
     */
    void set_mod_preset(const mod_preset &input) {
        mod_preset_values = input;
        select_instance(current_instance_);
        dump_midi();
    }

    /** @brief Returns a mutable reference to the full banked preset storage. */
    preset &get_preset() { return preset_values_; }

    /** @brief Returns a mutable reference to the full mod-routing preset. */
    mod_preset &get_mod_preset() { return mod_preset_values; }

    /* ------------------------------------------------------------------ */
    /*  Update result helpers                                             */
    /* ------------------------------------------------------------------ */

    /**
     * @brief Returns the index of the first knob push-button pressed during
     *        the most recent @ref update, or @c -1 if none.
     */
    uint8_t get_first_knob_sw_pushed() {
        if (last_ret_.knobs_sw_changed) {
            return last_ret_.get_first_pushed_knob_sw();
        }
        return -1;
    }

    /** @brief Returns @c true if any knob value changed in the last @ref update. */
    bool get_knob_changed() const { return last_ret_.knobs_changed; }

    /* ------------------------------------------------------------------ */
    /*  Viewer mode                                                       */
    /* ------------------------------------------------------------------ */

    /**
     * @brief Enables or disables mod-viewer mode and re-selects the current
     *        bank so that knob LEDs reflect the new storage source.
     */
    void set_viewer_mode(bool enable) {
        mod_viewer_mode = enable;
        select_instance(current_instance_);
    }

    /** @brief Re-applies the current bank selection without changing it. */
    void reset() {
        select_instance(current_instance_);
    }

    /* ------------------------------------------------------------------ */
    /*  MIDI helpers                                                      */
    /* ------------------------------------------------------------------ */

    /** @brief Maps parameter @p index in the current bank to a MIDI CC number. */
    uint8_t get_current_instance_midi_nr(uint8_t index) {
        return self()->get_midi_nr(get_current_instance(), index);
    }

    /* ------------------------------------------------------------------ */
    /*  CRTP overridable hooks (default implementations)                  */
    /* ------------------------------------------------------------------ */

    /**
     * @brief Called when a knob value changes during @ref update.
     *
     * The default implementation stores the value in the current bank
     * preset and sends a MIDI CC message. Derived classes may shadow
     * this method through CRTP to change or extend the behaviour.
     *
     * @param index        Knob index in the range `[0, KNOB_COUNT)`.
     * @param value_scaled Clamped knob value in the range `[0, 127]`.
     */
    void knob_val_changed(uint8_t index, uint8_t value_scaled) {
        set_current_preset_value(index, value_scaled);
        if (midi_) {
            midi_->send_cc(self()->get_current_instance_midi_nr(index),
                           static_cast<uint8_t>(value_scaled),
                           self()->get_midi_ch());
        }
    }

    /**
     * @brief Called when a knob push-button state changes during @ref update.
     *
     * The default implementation is intentionally empty.
     *
     * @param index Knob index in the range `[0, KNOB_COUNT)`.
     * @param state @c true when the button transitioned to pressed.
     */
    void knob_sw_changed(uint8_t /*index*/, bool /*state*/) {}

    /**
     * @brief Called when a non-selector button is pressed.
     *
     * Buttons whose index is `>= COUNT` are routed here with
     * `index - COUNT` as the argument. The default implementation
     * is intentionally empty.
     *
     * @param index Function button index (offset from @c COUNT).
     */
    void select_function(uint8_t /*index*/) {}

    /**
     * @brief Called during bank recall for each special parameter.
     *
     * Special parameters are those whose index is `>= KNOB_COUNT`
     * (i.e. non-knob parameters stored in the preset). The default
     * implementation is intentionally empty.
     *
     * @param value Recalled parameter value.
     */
    void force_function(uint8_t /*value*/) {}

private:
    /** @brief CRTP self-cast to the concrete derived type. */
    Derived       *self()       { return static_cast<Derived *>(this); }
    const Derived *self() const { return static_cast<const Derived *>(this); }

    /* ------------------------------------------------------------------ */
    /*  Knob polling                                                      */
    /* ------------------------------------------------------------------ */

    /**
     * @brief Polls every knob and dispatches change callbacks.
     *
     * Value changes are forwarded to the CRTP-dispatched
     * @c knob_val_changed and button-state changes to @c knob_sw_changed.
     */
    void update_knobs(ret_value &ret) {
        LOG_MODULE_DECLARE(UI_BLOCK, LOG_LEVEL_INF);
        for (uint8_t i = 0; i < KNOB_COUNT; i++) {
            Knob::knob_msg msg;
            knobs_[i].update(msg);

            if (msg.switch_changed) {
                const bool state = knobs_[i].get_state();
                LOG_INF("%s %d encoder switch %d %s",
                        self()->get_name(), current_instance_, i,
                        state ? "pushed" : "released");
                self()->knob_sw_changed(i, state);
                ret.knobs_sw[i] = state;
                ret.knobs_sw_changed = true;
                ret.knobs_sw_changed_array[i] = true;
            }

            if (msg.value_changed) {
                LOG_INF("%s %d knob %d changed %d",
                       self()->get_name(), current_instance_, i,
                       knobs_[i].get_value());
                self()->knob_val_changed(i, knobs_[i].get_value());
                ret.knobs_val[i] = knobs_[i].get_value();
                ret.knobs_changed = true;
            }
        }
    }

    /* ------------------------------------------------------------------ */
    /*  Button polling                                                    */
    /* ------------------------------------------------------------------ */

    /**
     * @brief Polls every button and routes pressed events through
     *        @ref button_changed.
     */
    void update_buttons(ret_value &ret) {
        LOG_MODULE_DECLARE(UI_BLOCK, LOG_LEVEL_INF);
        for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
            Button::button_msg msg;
            buttons_[i].update(msg);

            if (msg.switch_changed) {
                const bool pushed = buttons_[i].get_state();
                LOG_INF("%s %d button %d %s",
                        self()->get_name(), current_instance_, i,
                        pushed ? "pushed" : "released");
                button_changed(i, pushed);
                ret.buttons[i] = pushed;
                ret.buttons_changed = true;
            }
        }
    }

    /**
     * @brief Routes a pressed button to either @ref select_instance
     *        (for selector buttons) or the CRTP-dispatched
     *        @c select_function (for function buttons).
     *
     * When @c COUNT is 1 every button is a function button.
     * Otherwise, buttons @c [0, COUNT) are selectors and the rest
     * are function buttons.
     */
    void button_changed(uint8_t index, bool state) {
        if (!state) {
            return;
        }

        if constexpr (COUNT != 1) {
            if (index < COUNT) {
                select_instance(index);
            } else {
                self()->select_function(index - COUNT);
            }
        } else {
            self()->select_function(index);
        }
    }

    /* ------------------------------------------------------------------ */
    /*  Bank selection                                                    */
    /* ------------------------------------------------------------------ */

    /**
     * @brief Recalls a knob value from the current bank without emitting
     *        MIDI. Used during bank switches to restore the LED state.
     */
    void force_knob_update(uint8_t index, uint8_t value_scaled) {
        set_current_preset_value(index, value_scaled);
        knobs_[index].set_value(static_cast<uint8_t>(value_scaled));
    }

    /**
     * @brief Switches to bank @p index, updates selector LEDs, recalls
     *        all knob values, and applies special parameters.
     */
    void select_instance(uint8_t index) {
        LOG_MODULE_DECLARE(UI_BLOCK, LOG_LEVEL_INF);
        if constexpr (BUTTON_COUNT > 0) {
            turn_off_sw(current_instance_);
            turn_on_sw(index);
        }

        current_instance_ = index;

        for (uint8_t j = 0; j < KNOB_COUNT; j++) {
            const uint8_t val = static_cast<uint8_t>(
                    get_current_preset_value(j));
            force_knob_update(j, val);
        }

        if (!mod_viewer_mode) {
            if constexpr (PARAM_COUNT > KNOB_COUNT) {
                constexpr uint8_t special_count = PARAM_COUNT - KNOB_COUNT;
                for (uint8_t i = 0; i < special_count; i++) {
                    const uint8_t val = get_current_preset_value(KNOB_COUNT + i);
                    self()->force_function(static_cast<uint8_t>(val));
                    LOG_INF("%s %d special param %d %d",
                            self()->get_name(), current_instance_,
                            KNOB_COUNT + i, val);
                }
            }
        }

        LOG_INF("%s %d selected", self()->get_name(), index);
    }

    /* ------------------------------------------------------------------ */
    /*  MIDI dump                                                         */
    /* ------------------------------------------------------------------ */

    /**
     * @brief Sends every parameter in every bank as a MIDI CC message.
     */
    void dump_midi() {
        LOG_MODULE_DECLARE(UI_BLOCK, LOG_LEVEL_INF);
        if (!midi_) {
            return;
        }

        LOG_INF("%s dump midi", self()->get_name());
        for (uint8_t i = 0; i < COUNT; i++) {
            for (uint8_t j = 0; j < PARAM_COUNT; j++) {
                const uint8_t value = preset_values_[i][j];
                const uint8_t midi_nr = self()->get_midi_nr(i, j);
                midi_->send_cc(midi_nr,
                               static_cast<uint8_t>(value),
                               self()->get_midi_ch());
            }
        }
    }

    /* ------------------------------------------------------------------ */
    /*  Data members                                                      */
    /* ------------------------------------------------------------------ */

    /** @brief Banked parameter storage. */
    preset preset_values_ = {};

    /** @brief Borrowed MIDI transport for CC emission (may be @c nullptr). */
    MIDI *midi_ = nullptr;

    /** @brief Change flags from the most recent @ref update call. */
    ret_value last_ret_;

    /** @brief Selector/radio buttons owned by the block. */
    button_array buttons_;

    /** @brief Encoder-backed knobs owned by the block. */
    knob_array knobs_;

    /** @brief Currently selected bank index. */
    uint8_t current_instance_ = 0;
};

#endif /* APP_SRC_BLOCKS_UI_BLOCK_H_ */

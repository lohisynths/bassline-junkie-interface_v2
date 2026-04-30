/**
 * @file UI_BLOCK.h
 * @brief CRTP base template providing banked knob/button wiring,
 *        preset storage, and MIDI dump for control-surface blocks.
 *
 * Created on: Mar 29, 2026
 *     Author: alax
 */

#ifndef APP_SRC_BLOCKS_UI_BLOCK_H_
#define APP_SRC_BLOCKS_UI_BLOCK_H_

#include <cstdint>
#include <array>

#include "Button.h"
#include "Knob.h"
#include "MIDI.h"
#include "InputController.h"
#include "LEDS.h"
#include "LED_DISP.h"

#include <zephyr/logging/log.h>

/**
 * @brief CRTP base class providing banked knob/button wiring, preset storage,
 *        and MIDI dump logic.
 *
 * Each derived class supplies its own hardware configuration by defining two
 * public static constexpr arrays named @c knob_configs_ and
 * @c button_configs_, plus a few small metadata constants. The base class
 * pulls those definitions at compile time through the @p Derived template
 * parameter, so the public @ref init call needs only the shared runtime
 * objects.
 *
 * ## Required derived-class members (compile error if missing)
 *
 * | Member                                   | Purpose                                     |
 * |------------------------------------------|---------------------------------------------|
 * | `static constexpr const char *block_name_` | Block name used in log output             |
 * | `static constexpr uint8_t midi_offset_`    | MIDI CC base offset                        |
 * | `static constexpr uint8_t midi_channel_`   | MIDI channel used for CC emission          |
 *
 * The default @ref get_midi_nr implementation derives the layout from the
 * template parameters:
 * - `PARAM_COUNT == 0` uses source-major matrix numbering
 * - `COUNT == 1` uses unbanked linear numbering
 * - otherwise numbering is banked linear
 *
 * ## Optional derived-class overrides (shadowed via CRTP, defaults provided)
 *
 * | Signature                                                       | Default behaviour                                            |
 * |-----------------------------------------------------------------|--------------------------------------------------------------|
 * | `const char *get_name()`                                        | Returns `Derived::block_name_`                               |
 * | `uint8_t get_midi_nr(uint8_t instance, uint8_t index)`          | Derived from `COUNT` and `PARAM_COUNT`                       |
 * | `uint8_t get_midi_ch()`                                         | Returns `Derived::midi_channel_`                             |
 * | `void knob_val_changed(uint8_t index, uint8_t value_scaled)`    | Stores value in current bank and sends MIDI CC               |
 * | `void knob_sw_changed(uint8_t index, bool state)`               | Empty                                                        |
 * | `void select_function(uint8_t index)`                           | Empty                                                        |
 * | `bool handle_selector_press(uint8_t index)`                     | Returns `false` so bank buttons use the default recall path  |
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
    /** @brief Number of modulation source groups used by the mod preset layout. */
    static constexpr uint8_t MOD_SRC_COUNT = 6U;

    /** @brief Banked preset storage: @c COUNT banks of @c PARAM_COUNT parameters each. */
    using preset = std::array<std::array<uint8_t, PARAM_COUNT>, COUNT>;

    /** @brief Mod-routing preset storage indexed by source group and destination. */
    using mod_preset = std::array<std::array<uint8_t, PARAM_COUNT * MOD_SRC_COUNT>, MOD_SRC_COUNT>;

    /** @brief Alias for the block's owned button storage. */
    using button_array = std::array<Button, BUTTON_COUNT>;

    /** @brief Alias for the block's owned knob storage. */
    using knob_array = std::array<Knob, KNOB_COUNT>;

    /**
     * @brief Reports knob changes detected during one @ref update call.
     */
    struct ret_value {
        /** @brief Per-knob push-button pressed state sampled during this update. */
        std::array<bool, KNOB_COUNT> knobs_sw = {};

        /** @brief Set when any knob value changed. */
        bool knobs_changed = false;

        /** @brief Per-knob push-button change flags for this update. */
        std::array<bool, KNOB_COUNT> knobs_sw_changed = {};

        /**
         * @brief Returns @c true when the knob push-button at @p index
         *        transitioned to pressed during this update.
         *
         * @param index Knob index to query.
         * @retval true The knob switch was newly pressed this cycle.
         * @retval false No press transition was recorded for the knob.
         */
        bool knob_sw_just_pressed(uint8_t index) const {
            return (index < KNOB_COUNT) && knobs_sw_changed[index] && knobs_sw[index];
        }

        /**
         * @brief Returns the index of the first knob whose push-button
         *        transitioned to pressed during this update.
         *
         * @return Knob index in the range `[0, KNOB_COUNT)`, or `-1` if no
         *         knob push-button was pressed during this update.
         */
        int8_t get_first_pushed_knob_sw() const {
            for (uint8_t i = 0; i < KNOB_COUNT; i++) {
                if (knob_sw_just_pressed(i)) {
                    return i;
                }
            }
            return -1;
        }
    };

    /** @brief Mod-routing preset values (public for direct access by MOD block). */
    mod_preset mod_preset_values = {};

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
     * @param display Optional display block used for temporary value previews.
     */
    void init(MIDI &midi,
              LEDSController &leds,
              InputController &inputs,
              LED_DISP *display = nullptr) {
        midi_ = &midi;
        display_ = display;

        for (uint8_t i = 0; i < KNOB_COUNT; i++) {
            knobs_[i].init(inputs, Derived::knob_configs_[i], leds);
        }
        for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
            buttons_[i].init(inputs, Derived::button_configs_[i], leds);
        }

        select_instance_(current_instance_, true);
    }

    /**
     * @brief Initializes the block and binds a shared preview display.
     *
     * @param midi MIDI transport used for control-change emission.
     * @param leds Shared LED controller for indicator output.
     * @param inputs Shared input controller holding the cached input masks.
     * @param display Display block used for temporary value previews.
     */
    void init(MIDI &midi,
              LEDSController &leds,
              InputController &inputs,
              LED_DISP &display) {
        init(midi, leds, inputs, &display);
    }

    /**
     * @brief Polls all buttons and knobs, dispatches change callbacks.
     *
     * Button transitions are handled immediately through callbacks. The
     * returned summary keeps only the knob-related data that other blocks
     * consume after the update finishes.
     *
     * @return Knob-change summary from this update cycle.
     */
    ret_value update() {
        last_ret_ = {};
        update_buttons();
        update_knobs(last_ret_);
        return last_ret_;
    }

    /* ------------------------------------------------------------------ */
    /*  Bank / instance accessors                                         */
    /* ------------------------------------------------------------------ */

    /**
     * @brief Returns the currently selected bank index.
     *
     * @return Active bank index.
     */
    uint8_t get_current_instance() const {
        return current_instance_;
    }

    /**
     * @brief Turns off the LED of the button at @p index.
     *
     * @param index Button index whose LED should be turned off.
     */
    void turn_off_sw(uint8_t index) {
        buttons_[index].set_led_val(0);
    }

    /**
     * @brief Turns on the LED of the button at @p index to full brightness.
     *
     * @param index Button index whose LED should be turned on.
     */
    void turn_on_sw(uint8_t index) {
        buttons_[index].set_led_val(100);
    }

    /**
     * @brief Sets one button LED to either off or full brightness.
     *
     * @param index Button index to update.
     * @param state @c true turns the LED on, @c false turns it off.
     */
    void set_sw_state(uint8_t index, bool state) {
        buttons_[index].set_led_val(state ? 100 : 0);
    }

    /* ------------------------------------------------------------------ */
    /*  Preset value accessors                                            */
    /* ------------------------------------------------------------------ */

    /**
     * @brief Returns the value of parameter @p index in the current bank.
     *
     * @param index Parameter index within the current bank.
     * @return Stored parameter value.
     */
    uint8_t get_current_preset_value(uint8_t index) {
        return get_preset_value(current_instance_, index);
    }

    /**
     * @brief Returns one parameter value from the specified bank.
     *
     * @param instance Bank index to read from.
     * @param index Parameter index within the selected bank.
     * @return Stored parameter value.
     */
    uint8_t get_preset_value(uint8_t instance, uint8_t index) {
        return preset_values_[instance][index];
    }

    /**
     * @brief Stores one parameter value in the current bank.
     *
     * @param index Parameter index within the current bank.
     * @param value Parameter value to store.
     */
    void set_current_preset_value(uint8_t index, uint8_t value) {
        set_preset_value(current_instance_, index, value);
    }

    /**
     * @brief Stores one parameter value in the specified bank.
     *
     * @param instance Bank index to update.
     * @param index Parameter index within the selected bank.
     * @param value Parameter value to store.
     */
    void set_preset_value(uint8_t instance, uint8_t index, uint8_t value) {
        preset_values_[instance][index] = value;
    }

    /* ------------------------------------------------------------------ */
    /*  Mod-routing value accessors                                       */
    /* ------------------------------------------------------------------ */

    /**
     * @brief Returns one mod-routing value addressed by @p src and @p dst.
     *
     * @param src Modulation source group index in `[0, MOD_SRC_COUNT)`.
     * @param dst Destination parameter index in `[0, PARAM_COUNT * MOD_SRC_COUNT)`.
     * @return    The stored mod-routing value in the range `[0, 255]`.
     */
    uint8_t get_preset_mod_value(uint8_t src, uint8_t dst) {
        return mod_preset_values[src][dst];
    }

    /**
     * @brief Stores one mod-routing value addressed by @p src and @p dst.
     *
     * @param src Modulation source group index.
     * @param dst Destination parameter index.
     * @param value Mod-routing value to store.
     */
    void set_preset_mod_value(uint8_t src, uint8_t dst, uint8_t value) {
        mod_preset_values[src][dst] = value;
    }

    /* ------------------------------------------------------------------ */
    /*  Component accessors                                               */
    /* ------------------------------------------------------------------ */

    /**
     * @brief Returns a reference to the owned button array.
     *
     * @return Mutable button array owned by the block.
     */
    button_array &get_buttons() { return buttons_; }

    /**
     * @brief Returns a reference to the owned knob array.
     *
     * @return Mutable knob array owned by the block.
     */
    knob_array &get_knobs() { return knobs_; }

    /**
     * @brief Returns the borrowed MIDI transport pointer.
     *
     * @return MIDI transport pointer, or `nullptr` when unbound.
     */
    MIDI *get_midi() { return midi_; }

    /* ------------------------------------------------------------------ */
    /*  Preset bulk operations                                            */
    /* ------------------------------------------------------------------ */

    /**
     * @brief Loads a full preset, re-selects the current bank, and dumps
     *        all parameters as MIDI CC.
     *
     * @param input Full banked preset to load.
     */
    void set_preset(const preset &input) {
        preset_values_ = input;
        select_instance_(current_instance_, true);
        dump_midi();
    }

    /**
     * @brief Loads a full mod-routing preset, re-selects the current bank,
     *        and dumps all parameters as MIDI CC.
     *
     * @param input Full mod-routing preset to load.
     */
    void set_mod_preset(const mod_preset &input) {
        mod_preset_values = input;
        select_instance_(current_instance_, true);
        dump_midi();
    }

    /**
     * @brief Returns a mutable reference to the full banked preset storage.
     *
     * @return Mutable preset storage array.
     */
    preset &get_preset() { return preset_values_; }

    /**
     * @brief Returns a mutable reference to the full mod-routing preset.
     *
     * @return Mutable mod-routing preset storage array.
     */
    mod_preset &get_mod_preset() { return mod_preset_values; }

    /* ------------------------------------------------------------------ */
    /*  Update result helpers                                             */
    /* ------------------------------------------------------------------ */

    /**
     * @brief Returns the index of the first knob push-button pressed during
     *        the most recent @ref update, or `-1` if none.
     *
     * @return Index of the first pressed knob push-button in the range
     *         `[0, KNOB_COUNT)`, or `-1` if none was pressed.
     */
    int8_t get_first_knob_sw_pushed() {
        return last_ret_.get_first_pushed_knob_sw();
    }

    /**
     * @brief Returns @c true if any knob value changed in the last @ref update.
     *
     * @retval true At least one knob value changed.
     * @retval false No knob values changed.
     */
    bool get_knob_changed() const { return last_ret_.knobs_changed; }

    /* ------------------------------------------------------------------ */
    /*  Viewer mode                                                       */
    /* ------------------------------------------------------------------ */

    /** @brief Re-applies the current bank selection without changing it. */
    void reset() {
        select_instance_(current_instance_, true);
    }

    /* ------------------------------------------------------------------ */
    /*  MIDI helpers                                                      */
    /* ------------------------------------------------------------------ */

    /**
     * @brief Maps parameter @p index in the current bank to a MIDI CC number.
     *
     * @param index Parameter index within the current bank.
     * @return MIDI CC number for the selected parameter.
     */
    uint8_t get_current_instance_midi_nr(uint8_t index) {
        return self()->get_midi_nr(get_current_instance(), index);
    }

    /**
     * @brief Returns the block name used in log output.
     *
     * Derived classes may shadow this when they need custom behavior, but the
     * default implementation simply exposes the compile-time metadata string.
     *
     * @return Static block name string.
     */
    const char *get_name() {
        return Derived::block_name_;
    }

    /**
     * @brief Maps one logical parameter to a MIDI CC number.
     *
     * The default implementation selects a layout from the block template
     * parameters so most concrete blocks only need to expose
     * @c Derived::midi_offset_.
     *
     * @param instance Bank or source index, depending on block type.
     * @param index Parameter or destination index.
     * @return MIDI CC number for the requested value.
     */
    uint8_t get_midi_nr(uint8_t instance, uint8_t index) {
        if constexpr (PARAM_COUNT == 0U) {
            return static_cast<uint8_t>(Derived::midi_offset_ + instance + (index * COUNT));
        } else if constexpr (COUNT == 1U) {
            (void)instance;
            return static_cast<uint8_t>(Derived::midi_offset_ + index);
        } else {
            return static_cast<uint8_t>(Derived::midi_offset_ + (instance * PARAM_COUNT) + index);
        }
    }

    /**
     * @brief MIDI channel used for control-change messages emitted by the block.
     *
     * @return Fixed block MIDI channel number.
     */
    uint8_t get_midi_ch() {
        return Derived::midi_channel_;
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
        self()->set_current_preset_value(index, value_scaled);
        if (midi_) {
            midi_->send_cc(self()->get_current_instance_midi_nr(index),
                           value_scaled,
                           self()->get_midi_ch());
        }

        if (display_ != nullptr) {
            display_->show_preview_value(value_scaled);
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
    void knob_sw_changed(uint8_t index, bool state) {
        (void)index;
        (void)state;
    }

    /**
     * @brief Called when a non-selector button is pressed.
     *
     * Buttons whose index is `>= COUNT` are routed here with
     * `index - COUNT` as the argument. The default implementation
     * is intentionally empty.
     *
     * @param index Function button index (offset from @c COUNT).
     */
    void select_function(uint8_t index) {
        (void)index;
    }

    /**
     * @brief Called when a selector button is pressed before the default bank recall runs.
     *
     * Returning `true` suppresses the built-in selector handling, allowing a
     * derived block to swap context in a custom way such as a silent recall for
     * temporary overlays.
     *
     * @param index Raw selector button index in the range `[0, COUNT)`.
     *
     * @retval true The derived block fully handled the selector press.
     * @retval false The base class should run the normal bank recall path.
     */
    bool handle_selector_press(uint8_t index) {
        (void)index;
        return false;
    }

    /**
     * @brief Called during bank recall for each special parameter.
     *
     * Special parameters are those whose index is `>= KNOB_COUNT`
     * (i.e. non-knob parameters stored in the preset). The default
     * implementation is intentionally empty.
     *
     * @param value Recalled parameter value.
     */
    void force_function(uint8_t value) {
        (void)value;
    }

protected:
    /** @brief Borrowed display used to preview knob changes. */
    LED_DISP *display_ = nullptr;

private:
    /**
     * @brief CRTP self-cast to the concrete derived type.
     *
     * @return Mutable pointer to the concrete derived block type.
     */
    Derived *self() { return static_cast<Derived *>(this); }

    /**
     * @brief CRTP self-cast to the concrete derived type.
     *
     * @return Const pointer to the concrete derived block type.
     */
    const Derived *self() const { return static_cast<const Derived *>(this); }

    /* ------------------------------------------------------------------ */
    /*  Knob polling                                                      */
    /* ------------------------------------------------------------------ */

    /**
     * @brief Polls every knob and dispatches change callbacks.
     *
     * Value changes are forwarded to the CRTP-dispatched
     * @c knob_val_changed and button-state changes to @c knob_sw_changed.
     *
     * @param ret Output structure populated with this cycle's knob changes.
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
                ret.knobs_sw_changed[i] = true;
            }

            if (msg.value_changed) {
                LOG_INF("%s %d knob %d changed %d",
                        self()->get_name(), current_instance_, i,
                        knobs_[i].get_value());
                self()->knob_val_changed(i, knobs_[i].get_value());
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
    void update_buttons() {
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
     *
     * @param index Raw button index reported by the hardware layer.
     * @param state New pressed state of the button.
     */
    void button_changed(uint8_t index, bool state) {
        if (!state) {
            return;
        }

        if constexpr (COUNT != 1) {
            if (index < COUNT) {
                if (!self()->handle_selector_press(index)) {
                    select_instance_(index, true);
                }
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
     * @brief Switches to bank @p index, updates selector LEDs, recalls
     *        all knob values, and applies special parameters.
     *
     * @param index Bank index to activate.
     */
protected:
    /**
     * @brief Re-applies one bank and optionally redraws the owned knob LEDs.
     *
     * @param index Bank index to activate.
     * @param render_knobs `true` redraws knob LEDs, `false` only synchronizes
     *        the stored knob values.
     */
    void recall_instance(uint8_t index, bool render_knobs = true) {
        select_instance_(index, render_knobs);
    }

private:
    void select_instance_(uint8_t index, bool render_knobs) {
        LOG_MODULE_DECLARE(UI_BLOCK, LOG_LEVEL_INF);
        if constexpr (BUTTON_COUNT > 0) {
            set_sw_state(current_instance_, false);
            set_sw_state(index, true);
        }

        current_instance_ = index;

        for (uint8_t j = 0; j < KNOB_COUNT; j++) {
            if (render_knobs) {
                knobs_[j].set_value(self()->get_current_preset_value(j));
            } else {
                knobs_[j].set_value_silent(self()->get_current_preset_value(j));
            }
        }

        if constexpr (PARAM_COUNT > KNOB_COUNT) {
            constexpr uint8_t special_count = PARAM_COUNT - KNOB_COUNT;
            for (uint8_t i = 0; i < special_count; i++) {
                const uint8_t val = self()->get_current_preset_value(KNOB_COUNT + i);
                self()->force_function(val);
                LOG_INF("%s %d special param %d %d",
                        self()->get_name(), current_instance_,
                        KNOB_COUNT + i, val);
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
                midi_->send_cc(midi_nr, value, self()->get_midi_ch());
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

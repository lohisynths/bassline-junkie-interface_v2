/**
 * @file MOD.h
 * @brief Mod-routing control-surface block.
 *
 * Created on: Jun 5, 2018
 *     Author: alax
 */

#ifndef APP_SRC_BLOCKS_MOD_H_
#define APP_SRC_BLOCKS_MOD_H_

#include "LED_DISP.h"
#include "ModMatrixCapture.h"
#include "OSC.h"
#include "FLT.h"
#include "UI_BLOCK.h"

#include <zephyr/kernel.h>

/** @brief Number of encoder knobs in the MOD block. */
static constexpr uint8_t MOD_KNOB_COUNT = 1U;

/** @brief Number of selector buttons in the MOD block. */
static constexpr uint8_t MOD_BUTTON_COUNT = 6U;

/** @brief Number of banked instances in the MOD block. */
static constexpr uint8_t MOD_COUNT = 6U;

/** @brief MIDI CC offset used by the MOD block. */
static constexpr uint8_t MOD_MIDI_OFFSET = 0U;

/** @brief MIDI channel used for MOD control-change messages. */
static constexpr uint8_t MOD_MIDI_CHANNEL = 2U;

/** @brief Number of logical parameters exposed by the MOD block. */
static constexpr uint8_t MOD_PARAM_COUNT = 0U;

/** @brief First MOD destination routed through the filter block. */
static constexpr uint8_t MOD_FIRST_FLT_DEST = OSC_PARAM_COUNT * OSC_COUNT;

/** @brief Number of FLT destinations exposed through MOD. */
static constexpr uint8_t MOD_FLT_DEST_COUNT = 2U;

/** @brief Total number of MOD destinations. */
static constexpr uint8_t MOD_DEST_COUNT = MOD_FIRST_FLT_DEST + MOD_FLT_DEST_COUNT;

/**
 * @brief Mod-routing block with six source selectors and one amount knob.
 *
 * Each MOD source is represented as one bank. The single knob edits the routing
 * amount for the currently selected source and destination. Destinations are
 * borrowed from the OSC and FLT blocks so this block can persist modulation
 * amounts into their mod matrices and temporarily repurpose the visible OSC
 * and FLT knobs to edit the routing table for the active source.
 */
class MOD : public UI_BLOCK<MOD, MOD_KNOB_COUNT, MOD_BUTTON_COUNT, MOD_PARAM_COUNT, MOD_COUNT>,
            public ModMatrixCapture {
public:
    /** @brief Shorthand for the CRTP base class used by MOD. */
    using ui_block = UI_BLOCK<MOD, MOD_KNOB_COUNT, MOD_BUTTON_COUNT, MOD_PARAM_COUNT, MOD_COUNT>;

    /** @brief Static block name used by the shared CRTP base logging. */
    static constexpr const char *block_name_ = "MOD";

    /** @brief MIDI CC offset used by the MOD block. */
    static constexpr uint8_t midi_offset_ = MOD_MIDI_OFFSET;

    /** @brief MIDI channel used for MOD control-change messages. */
    static constexpr uint8_t midi_channel_ = MOD_MIDI_CHANNEL;

    /** @brief LED arc length for the MOD amount knob. */
    static constexpr uint8_t knob_led_count_ = 10U;

    /** @brief Hold time required to enter the MOD viewer. */
    static constexpr uint32_t viewer_hold_ms_ = 1000U;

    /** @brief Mux/LED bindings for the six MOD source selector buttons. */
    static constexpr std::array button_configs_ = {
        Button::Config{ .mux_index = 2U, .pin = 8U, .led_number = 127U },
        Button::Config{ .mux_index = 2U, .pin = 7U, .led_number = 126U },
        Button::Config{ .mux_index = 2U, .pin = 6U, .led_number = 125U },
        Button::Config{ .mux_index = 2U, .pin = 5U, .led_number = 124U },
        Button::Config{ .mux_index = 2U, .pin = 4U, .led_number = 123U },
        Button::Config{ .mux_index = 2U, .pin = 3U, .led_number = 122U },
    };

    /** @brief Encoder/button/LED bindings for the MOD amount knob. */
    static constexpr std::array knob_configs_ = {
        Knob::Config{
            .button_mux_index  = 2U,
            .button_pin        = 0U,
            .encoder_mux_index = 2U,
            .encoder_pin_a     = 1U,
            .encoder_pin_b     = 2U,
            .first_led         = 112U,
            .led_count         = knob_led_count_,
        },
    };

    /**
     * @brief Initializes the block and binds the shared preview display.
     *
     * @param midi MIDI backend borrowed by the CRTP base block.
     * @param leds LED controller passed through to the CRTP base block.
     * @param inputs Input controller used to initialize the block hardware.
     * @param osc OSC block providing oscillator destinations and mod storage.
     * @param filter FLT block providing filter destinations and mod storage.
     * @param display Display block used for temporary value previews.
     */
    void init(MIDI &midi,
              LEDSController &leds,
              InputController &inputs,
              OSC &osc,
              FLT &filter,
              LED_DISP &display) {
        ui_block::init(midi, leds, inputs, &display);
        osc_ = &osc;
        filter_ = &filter;
    }

    /**
     * @brief Polls the base block and manages the temporary MOD viewer.
     *
     * Holding any MOD source button for longer than one second overlays the
     * OSC and FLT knobs with the current MOD matrix values for the active
     * source and temporarily repurposes those visible knobs to edit route
     * amounts.
     *
     * @return Aggregated update flags from the CRTP base block.
     */
    ui_block::ret_value update() {
        const ui_block::ret_value ret = ui_block::update();
        update_destination_selection_arm_(ret);
        update_viewer_state_();
        return ret;
    }

    /* ------------------------------------------------------------------ */
    /*  CRTP hook overrides                                               */
    /* ------------------------------------------------------------------ */

    /**
     * @brief Returns the current routing amount for the selected source and destination.
     *
     * @param index Unused placeholder required by the CRTP interface.
     * @return Stored routing amount for the active source/destination pair.
     */
    uint8_t get_current_preset_value(uint8_t index) {
        (void)index;
        const uint8_t src = get_current_instance();
        const uint8_t dst = actual_mod_dest;

        if (dst < MOD_FIRST_FLT_DEST) {
            if (osc_) {
                return osc_->get_preset_mod_value(src, dst);
            }
        } else if (filter_) {
            return filter_->get_preset_mod_value(src, static_cast<uint8_t>(dst - MOD_FIRST_FLT_DEST));
        }

        return 0U;
    }

    /**
     * @brief Sends the new routing amount as MIDI when the knob changes.
     *
     * @param index Unused knob index placeholder.
     * @param value_scaled New routing amount in MIDI range.
     */
    void knob_val_changed(uint8_t index, uint8_t value_scaled) {
        (void)index;
        store_and_send_route_value_(actual_mod_dest, value_scaled);

        if (mod_viewer_active_) {
            sync_visible_destination_knob_(actual_mod_dest, value_scaled);
        }

        if (display_ != nullptr) {
            display_->show_preview_value(value_scaled);
        }
    }

    /** @brief MOD buttons are bank selectors only; no function buttons exist. */
    void select_function(uint8_t /*index*/) {}

    /** @brief MOD has no special preset parameters to restore. */
    void force_function(uint8_t /*value*/) {}

    /* ------------------------------------------------------------------ */
    /*  MOD-specific helpers                                              */
    /* ------------------------------------------------------------------ */

    /**
     * @brief Polls OSC and FLT knob push-buttons to select the active MOD destination.
     *
     * OSC destinations map to `[0, 14]` and FLT destinations map to `[15, 16]`.
     * The filter keyboard-tracking knob is intentionally excluded from MOD.
     */
    void poll_mod_destination_selection() {
        if (mod_viewer_active_ || !mod_destination_select_armed_) {
            return;
        }

        if (osc_) {
            const int ret = osc_->get_first_knob_sw_pushed();
            if (ret > -1) {
                select_MOD_dest(static_cast<uint8_t>(ret + (osc_->get_current_osc() * OSC_KNOB_COUNT)));
            }
        }

        if (filter_) {
            const int ret = filter_->get_first_knob_sw_pushed();
            if ((ret > -1) && (ret < MOD_FLT_DEST_COUNT)) {
                select_MOD_dest(static_cast<uint8_t>(ret + MOD_FIRST_FLT_DEST));
            }
        }
    }

    /**
     * @brief Selects which destination the MOD knob edits.
     *
     * @param index MOD destination index to activate.
     */
    void select_MOD_dest(uint8_t index) {
        if (index >= MOD_DEST_COUNT) {
            return;
        }

        LOG_MODULE_DECLARE(MOD, LOG_LEVEL_INF);
        actual_mod_dest = index;
        reset();
        if (osc_) {
            osc_->reset();
        }
        if (filter_) {
            filter_->reset();
        }
        LOG_INF("%s dst %d selected", get_name(), index);
    }

    /**
     * @brief Returns the currently selected MOD destination index.
     *
     * @return Active MOD destination index.
     */
    uint8_t get_current_mod_dest() const {
        return actual_mod_dest;
    }

    /**
     * @brief Returns whether the MOD viewer overlay is currently active.
     *
     * @return @c true when the viewer overlay is active.
     */
    bool is_viewer_active() const {
        return mod_viewer_active_;
    }

    bool is_mod_viewer_active() const override {
        return mod_viewer_active_;
    }

    /**
     * @brief Re-synchronizes the MOD amount knob and any active viewer LEDs
     *        from the current OSC/FLT modulation matrices.
     *
     * This is used after preset restores because MOD route state lives inside
     * the bound OSC and FLT blocks rather than inside MOD-owned preset storage.
     */
    void refresh_from_bound_presets() {
        reset();

        if (!mod_viewer_active_) {
            return;
        }

        sync_viewer_knobs_();
        refresh_viewer_();
    }

    void refresh_mod_viewer() override {
        if (!mod_viewer_active_) {
            return;
        }

        sync_viewer_knobs_();
        refresh_viewer_();
    }

    /**
     * @brief Consumes one OSC knob change as a MOD route edit when the viewer
     *        is active.
     *
     * @param osc_bank Active OSC bank that owns the visible knob.
     * @param knob_index OSC knob index within the active bank.
     * @param value New route amount.
     *
     * @retval true The viewer consumed the OSC knob event.
     * @retval false The caller should keep normal OSC behavior.
     */
    bool capture_osc_route_value(uint8_t osc_bank, uint8_t knob_index, uint8_t value) override {
        if (!mod_viewer_active_) {
            return false;
        }

        const uint8_t destination = static_cast<uint8_t>(osc_bank * OSC_KNOB_COUNT + knob_index);
        actual_mod_dest = destination;
        store_and_send_route_value_(destination, value);
        return true;
    }

    /**
     * @brief Consumes one FLT knob change as a MOD route edit when the viewer
     *        is active.
     *
     * @param knob_index FLT knob index that changed.
     * @param value New route amount.
     *
     * @retval true The viewer consumed the FLT knob event.
     * @retval false The caller should keep normal FLT behavior.
     */
    bool capture_flt_route_value(uint8_t knob_index, uint8_t value) override {
        if (!mod_viewer_active_) {
            return false;
        }

        if (knob_index >= MOD_FLT_DEST_COUNT) {
            return true;
        }

        const uint8_t destination = static_cast<uint8_t>(MOD_FIRST_FLT_DEST + knob_index);
        actual_mod_dest = destination;
        store_and_send_route_value_(destination, value);
        return true;
    }

private:
    /** @brief Sentinel used to force viewer resynchronization on first entry. */
    static constexpr uint8_t viewer_context_invalid_ = 0xFFU;

    /**
     * @brief Arms MOD destination selection only after the MOD knob has
     *        already been held across at least one update cycle.
     *
     * This prevents arbitrary OSC/FLT knob presses from changing the active
     * MOD destination and also rejects near-simultaneous MOD+destination
     * presses that do not satisfy the "hold MOD first, then press another
     * knob" interaction.
     *
     * @param ret Change summary returned by the base UI block update.
     */
    void update_destination_selection_arm_(const ui_block::ret_value &ret) {
        const bool mod_knob_pressed = get_knobs()[0].get_state();
        if (!mod_knob_pressed) {
            mod_destination_select_armed_ = false;
            return;
        }

        const bool mod_knob_just_pressed =
            ret.knob_sw_just_pressed(0);

        if (mod_knob_just_pressed) {
            mod_destination_select_armed_ = false;
            return;
        }

        mod_destination_select_armed_ = true;
    }

    /**
     * @brief Refreshes the OSC and FLT knob LEDs from the MOD matrix.
     */
    void refresh_viewer_() {
        const uint8_t source = get_current_instance();

        if (osc_) {
            osc_->show_mod_view(source);
        }
        if (filter_) {
            filter_->show_mod_view(source);
        }
    }

    /**
     * @brief Restores OSC and FLT knob LEDs to their normal preset values.
     */
    void clear_viewer_() {
        if (osc_) {
            osc_->clear_mod_view();
        }
        if (filter_) {
            filter_->clear_mod_view();
        }
    }

    /**
     * @brief Tracks any MOD selector button and toggles the viewer after a 1 second hold.
     */
    void update_viewer_state_() {
        LOG_MODULE_DECLARE(MOD, LOG_LEVEL_INF);

        const int8_t pressed_button = get_pressed_button_();
        if (pressed_button < 0) {
            if (viewer_button_pressed_) {
                viewer_button_pressed_ = false;
                viewer_button_index_ = -1;
                viewer_button_pressed_at_ms_ = 0U;
            }

            if (mod_viewer_active_) {
                mod_viewer_active_ = false;
                clear_viewer_();
                last_viewer_source_ = viewer_context_invalid_;
                last_viewer_osc_bank_ = viewer_context_invalid_;
                LOG_INF("%s viewer off", get_name());
            }
            return;
        }

        const uint32_t now = k_uptime_get_32();
        if ((viewer_button_index_ != pressed_button) || !viewer_button_pressed_) {
            viewer_button_pressed_ = true;
            viewer_button_index_ = pressed_button;
            viewer_button_pressed_at_ms_ = now;
        }

        if (!mod_viewer_active_) {
            const uint32_t held_ms = now - viewer_button_pressed_at_ms_;
            if (held_ms >= viewer_hold_ms_) {
                mod_viewer_active_ = true;
                sync_viewer_knobs_();
                LOG_INF("%s viewer on", get_name());
            }
        }

        if (mod_viewer_active_) {
            if ((last_viewer_source_ != get_current_instance()) ||
                ((osc_ != nullptr) && (last_viewer_osc_bank_ != osc_->get_current_osc()))) {
                sync_viewer_knobs_();
            }
            refresh_viewer_();
        }
    }

    /**
     * @brief Returns the first currently pressed MOD selector button, or `-1`.
     *
     * @return Index of the first pressed selector button, or `-1` if none are pressed.
     */
    int8_t get_pressed_button_() {
        const auto &buttons = get_buttons();
        for (uint8_t i = 0U; i < MOD_BUTTON_COUNT; i++) {
            if (buttons[i].get_state()) {
                return static_cast<int8_t>(i);
            }
        }
        return -1;
    }

    /**
     * @brief Persists the current routing amount into the active OSC or FLT mod table.
     *
     * @param value Routing amount to store.
     */
    void store_current_preset_value(uint8_t value) {
        const uint8_t src = get_current_instance();
        const uint8_t dst = actual_mod_dest;

        if (dst < MOD_FIRST_FLT_DEST) {
            if (osc_) {
                osc_->set_preset_mod_value(src, dst, value);
            }
            return;
        }

        if (filter_) {
            filter_->set_preset_mod_value(src, static_cast<uint8_t>(dst - MOD_FIRST_FLT_DEST), value);
        }
    }

    /**
     * @brief Synchronizes the visible OSC and FLT knobs to the current route
     *        values so viewer edits continue from the stored modulation state.
     */
    void sync_viewer_knobs_() {
        const uint8_t source = get_current_instance();
        const uint8_t osc_bank = (osc_ != nullptr) ? osc_->get_current_osc() : 0U;

        sync_osc_viewer_knobs_(source, osc_bank);
        sync_flt_viewer_knobs_(source);

        last_viewer_source_ = source;
        last_viewer_osc_bank_ = osc_bank;
    }

    /**
     * @brief Synchronizes the visible OSC knobs to the selected source row for
     *        the current OSC bank.
     *
     * @param source Active MOD source.
     * @param osc_bank Active OSC bank whose knobs are visible.
     */
    void sync_osc_viewer_knobs_(uint8_t source, uint8_t osc_bank) {
        if (osc_ == nullptr) {
            return;
        }

        auto &knobs = osc_->get_knobs();
        for (uint8_t i = 0U; i < OSC_KNOB_COUNT; ++i) {
            const uint8_t destination = static_cast<uint8_t>(osc_bank * OSC_KNOB_COUNT + i);
            (void)knobs[i].set_value_silent(osc_->get_preset_mod_value(source, destination));
        }
    }

    /**
     * @brief Synchronizes the visible FLT knobs to the selected source row.
     *
     * @param source Active MOD source.
     */
    void sync_flt_viewer_knobs_(uint8_t source) {
        if (filter_ == nullptr) {
            return;
        }

        auto &knobs = filter_->get_knobs();
        for (uint8_t i = 0U; i < MOD_FLT_DEST_COUNT; ++i) {
            (void)knobs[i].set_value_silent(filter_->get_preset_mod_value(source, i));
        }
    }

    /**
     * @brief Synchronizes one visible borrowed knob to a route value after the
     *        dedicated MOD amount knob edited the same destination.
     *
     * @param destination Absolute MOD destination index.
     * @param value Route amount written to the matrix.
     */
    void sync_visible_destination_knob_(uint8_t destination, uint8_t value) {
        if (osc_ != nullptr) {
            const uint8_t osc_bank = osc_->get_current_osc();
            const uint8_t first_visible_osc_dest = static_cast<uint8_t>(osc_bank * OSC_KNOB_COUNT);
            const uint8_t past_last_visible_osc_dest = static_cast<uint8_t>(first_visible_osc_dest + OSC_KNOB_COUNT);

            if ((destination >= first_visible_osc_dest) && (destination < past_last_visible_osc_dest)) {
                auto &knob = osc_->get_knobs()[destination - first_visible_osc_dest];
                (void)knob.set_value_silent(value);
                (void)knob.show_preview_value(value);
                return;
            }
        }

        if ((filter_ != nullptr) &&
            (destination >= MOD_FIRST_FLT_DEST) &&
            (destination < static_cast<uint8_t>(MOD_FIRST_FLT_DEST + MOD_FLT_DEST_COUNT))) {
            auto &knob = filter_->get_knobs()[destination - MOD_FIRST_FLT_DEST];
            (void)knob.set_value_silent(value);
            (void)knob.show_preview_value(value);
        }
    }

    /**
     * @brief Stores one route value, synchronizes the MOD amount knob, and
     *        sends the MOD CC.
     *
     * @param destination Absolute MOD destination index.
     * @param value Route amount to persist.
     */
    void store_and_send_route_value_(uint8_t destination, uint8_t value) {
        actual_mod_dest = destination;
        store_current_preset_value(value);
        (void)get_knobs()[0].set_value(value);

        if (get_midi()) {
            get_midi()->send_cc(get_midi_nr(get_current_instance(), destination),
                                value,
                                get_midi_ch());
        }
    }

    /** @brief Currently selected modulation destination. */
    uint8_t actual_mod_dest = 0U;

    /** @brief True once the MOD knob has been held long enough to allow destination picks. */
    bool mod_destination_select_armed_ = false;

    /** @brief True while the MOD matrix viewer overlay is active. */
    bool mod_viewer_active_ = false;

    /** @brief True while any MOD source selector button is being held. */
    bool viewer_button_pressed_ = false;

    /** @brief Index of the selector button currently being tracked for viewer hold detection. */
    int8_t viewer_button_index_ = -1;

    /** @brief Timestamp captured when the current viewer candidate button was pressed. */
    uint32_t viewer_button_pressed_at_ms_ = 0U;

    /** @brief MOD source last synchronized into viewer-edit knob state. */
    uint8_t last_viewer_source_ = viewer_context_invalid_;

    /** @brief OSC bank last synchronized into viewer-edit knob state. */
    uint8_t last_viewer_osc_bank_ = viewer_context_invalid_;

    /** @brief Borrowed OSC block used for destination previews and routing storage. */
    OSC *osc_ = nullptr;

    /** @brief Borrowed FLT block used for destination previews and routing storage. */
    FLT *filter_ = nullptr;
};

#endif /* APP_SRC_BLOCKS_MOD_H_ */

/**
 * @file MOD.h
 * @brief Mod-routing control-surface block.
 *
 * Created on: Jun 5, 2018
 *     Author: alax
 */

#ifndef APP_SRC_BLOCKS_MOD_H_
#define APP_SRC_BLOCKS_MOD_H_

#include "UI_BLOCK.h"
#include "OSC.h"
#include "FLT.h"

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

class MOD : public UI_BLOCK<MOD, MOD_KNOB_COUNT, MOD_BUTTON_COUNT, MOD_PARAM_COUNT, MOD_COUNT> {
public:
    using ui_block = UI_BLOCK<MOD, MOD_KNOB_COUNT, MOD_BUTTON_COUNT, MOD_PARAM_COUNT, MOD_COUNT>;

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

    /** @brief Attach the OSC and FLT blocks used for destination lookups. */
    void bind_sources(OSC &osc, FLT &filter) {
        osc_ = &osc;
        filter_ = &filter;
    }

    /* ------------------------------------------------------------------ */
    /*  Required CRTP hooks                                               */
    /* ------------------------------------------------------------------ */

    /** @brief Returns the block name used in log output. */
    const char *get_name() { return "MOD"; }

    /**
     * @brief Maps a source bank and destination index to a MIDI CC number.
     *
     * Layout matches the legacy MOD class: `source + destination * MOD_COUNT`.
     */
    uint8_t get_midi_nr(uint8_t instance, uint8_t index) {
        return static_cast<uint8_t>(MOD_MIDI_OFFSET + instance + (index * MOD_COUNT));
    }

    /** @brief MIDI channel used for all MOD control-change messages. */
    uint8_t get_midi_ch() { return MOD_MIDI_CHANNEL; }

    /**
     * @brief Polls the base block and manages the temporary MOD viewer.
     *
     * Holding MOD button 0 for longer than one second overlays the OSC and
     * FLT knob LEDs with the current MOD matrix values for the active source.
     */
    ui_block::ret_value update() {
        const ui_block::ret_value ret = ui_block::update();
        update_viewer_state_();
        return ret;
    }

    /* ------------------------------------------------------------------ */
    /*  CRTP hook overrides                                               */
    /* ------------------------------------------------------------------ */

    /**
     * @brief Returns the current routing amount for the selected source and destination.
     */
    uint8_t get_current_preset_value(uint8_t /*index*/) {
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
     */
    void knob_val_changed(uint8_t /*index*/, uint8_t value_scaled) {
        store_current_preset_value(value_scaled);

        if (get_midi()) {
            get_midi()->send_cc(get_midi_nr(get_current_instance(), actual_mod_dest),
                                value_scaled,
                                get_midi_ch());
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

    /** @brief Returns the currently selected MOD destination index. */
    uint8_t get_current_mod_dest() const {
        return actual_mod_dest;
    }

    /**
     * @brief Returns whether the MOD viewer overlay is currently active.
     */
    bool is_viewer_active() const {
        return mod_viewer_active_;
    }

private:
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
                LOG_INF("%s viewer on", get_name());
            }
        }

        if (mod_viewer_active_) {
            refresh_viewer_();
        }
    }

    /**
     * @brief Returns the first currently pressed MOD selector button, or `-1`.
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

    uint8_t actual_mod_dest = 0U;
    bool mod_viewer_active_ = false;
    bool viewer_button_pressed_ = false;
    int8_t viewer_button_index_ = -1;
    uint32_t viewer_button_pressed_at_ms_ = 0U;

    OSC *osc_ = nullptr;
    FLT *filter_ = nullptr;
};

#endif /* APP_SRC_BLOCKS_MOD_H_ */

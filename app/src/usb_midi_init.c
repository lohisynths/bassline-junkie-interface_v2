/*
 * usb_midi_init.c
 *
 *  Created on: Apr 4, 2026
 *      Author: alax
 */

#include "usb_midi_init.h"

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/usb/class/usbd_midi2.h>
#include <zephyr/audio/midi.h>
#include <zephyr/logging/log.h>

#include <ump_stream_responder.h>

LOG_MODULE_REGISTER(usb_midi_init, LOG_LEVEL_INF);

/* ------------------------------------------------------------------ */
/*  Message queue for received MIDI channel voice messages             */
/* ------------------------------------------------------------------ */

/** Maximum number of MIDI messages buffered between callbacks and polls. */
#define RX_QUEUE_DEPTH 32

K_MSGQ_DEFINE(rx_msgq, sizeof(struct usb_midi_msg), RX_QUEUE_DEPTH, 4);

/* ------------------------------------------------------------------ */
/*  USB device context                                                 */
/* ------------------------------------------------------------------ */

USBD_DEVICE_DEFINE(usbd_ctx,
                   DEVICE_DT_GET(DT_NODELABEL(usbotg_fs)),
                   0x1209, 0x0001);

USBD_DESC_LANG_DEFINE(usbd_lang);
USBD_DESC_MANUFACTURER_DEFINE(usbd_mfr, "Bassline Junkie");
USBD_DESC_PRODUCT_DEFINE(usbd_product, "Bassline Junkie Interface");

USBD_DESC_CONFIG_DEFINE(usbd_fs_cfg_desc, "Full-Speed Configuration");
USBD_CONFIGURATION_DEFINE(usbd_fs_config, 0, 100, &usbd_fs_cfg_desc);

/* ------------------------------------------------------------------ */
/*  USB MIDI device (from device tree)                                 */
/* ------------------------------------------------------------------ */

#define USB_MIDI_NODE DT_NODELABEL(usb_midi)

static const struct device *const midi_dev = DEVICE_DT_GET(USB_MIDI_NODE);

/* ------------------------------------------------------------------ */
/*  UMP stream responder (MIDI 2.0 endpoint/function block discovery)  */
/* ------------------------------------------------------------------ */

static const struct ump_endpoint_dt_spec ump_ep_dt =
    UMP_ENDPOINT_DT_SPEC_GET(USB_MIDI_NODE);

static const struct ump_stream_responder_cfg responder_cfg =
    UMP_STREAM_RESPONDER(midi_dev, usbd_midi_send, &ump_ep_dt);

/* ------------------------------------------------------------------ */
/*  MIDI class callbacks                                               */
/* ------------------------------------------------------------------ */

static void on_rx_packet(const struct device *dev, const struct midi_ump ump)
{
    ARG_UNUSED(dev);

    switch (UMP_MT(ump)) {
    case UMP_MT_MIDI1_CHANNEL_VOICE: {
        struct usb_midi_msg msg = {
            .command = UMP_MIDI_COMMAND(ump),
            .channel = UMP_MIDI_CHANNEL(ump),
            .data1   = UMP_MIDI1_P1(ump),
            .data2   = UMP_MIDI1_P2(ump),
        };

        LOG_DBG("RX cmd:0x%X ch:%u d1:%u d2:%u",
                msg.command, msg.channel, msg.data1, msg.data2);

        if (k_msgq_put(&rx_msgq, &msg, K_NO_WAIT) != 0) {
            LOG_WRN("USB MIDI RX queue full, message dropped");
        }
        break;
    }

    case UMP_MT_UMP_STREAM:
        ump_stream_respond(&responder_cfg, ump);
        break;

    default:
        LOG_DBG("Unhandled UMP MT=0x%X", UMP_MT(ump));
        break;
    }
}

static void on_device_ready(const struct device *dev, const bool ready)
{
    ARG_UNUSED(dev);
    LOG_INF("USB MIDI %s", ready ? "enabled by host" : "disabled by host");
}

static const struct usbd_midi_ops midi_ops = {
    .rx_packet_cb = on_rx_packet,
    .ready_cb     = on_device_ready,
};

/* ------------------------------------------------------------------ */
/*  Public API                                                         */
/* ------------------------------------------------------------------ */

int usb_midi_device_init(void)
{
    int err;

    if (!device_is_ready(midi_dev)) {
        LOG_ERR("USB MIDI device not ready");
        return -ENODEV;
    }

    usbd_midi_set_ops(midi_dev, &midi_ops);

    err = usbd_add_descriptor(&usbd_ctx, &usbd_lang);
    if (err) {
        LOG_ERR("Failed to add language descriptor: %d", err);
        return err;
    }

    err = usbd_add_descriptor(&usbd_ctx, &usbd_mfr);
    if (err) {
        LOG_ERR("Failed to add manufacturer descriptor: %d", err);
        return err;
    }

    err = usbd_add_descriptor(&usbd_ctx, &usbd_product);
    if (err) {
        LOG_ERR("Failed to add product descriptor: %d", err);
        return err;
    }

    err = usbd_add_configuration(&usbd_ctx, USBD_SPEED_FS,
                                 &usbd_fs_config);
    if (err) {
        LOG_ERR("Failed to add FS configuration: %d", err);
        return err;
    }

    err = usbd_register_all_classes(&usbd_ctx, USBD_SPEED_FS, 1, NULL);
    if (err) {
        LOG_ERR("Failed to register USB classes: %d", err);
        return err;
    }

    usbd_device_set_code_triple(&usbd_ctx, USBD_SPEED_FS,
                                USB_BCC_MISCELLANEOUS, 0x02, 0x01);

    err = usbd_init(&usbd_ctx);
    if (err) {
        LOG_ERR("Failed to initialize USB device: %d", err);
        return err;
    }

    err = usbd_enable(&usbd_ctx);
    if (err) {
        LOG_ERR("Failed to enable USB device: %d", err);
        return err;
    }

    LOG_INF("USB MIDI ready on PA11/PA12");
    return 0;
}

int usb_midi_receive(struct usb_midi_msg *msg)
{
    if (msg == NULL) {
        return -EINVAL;
    }

    return k_msgq_get(&rx_msgq, msg, K_NO_WAIT) == 0 ? 0 : -EAGAIN;
}

int usb_midi_send_msg(const struct usb_midi_msg *msg)
{
    if (msg == NULL) {
        return -EINVAL;
    }

    struct midi_ump ump = UMP_MIDI1_CHANNEL_VOICE(
        0, msg->command, msg->channel, msg->data1, msg->data2);

    return usbd_midi_send(midi_dev, ump);
}

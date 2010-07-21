/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU Lesser General Public License Version 2.1
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

/**
 * SECTION:gcm-sensor-colormunki
 * @short_description: functionality to talk to the ColorMunki colorimeter.
 *
 * This object contains all the low level logic for the ColorMunki hardware.
 */

#include "config.h"

#include <glib-object.h>
#include <libusb-1.0/libusb.h>

#include "egg-debug.h"
#include "gcm-common.h"
#include "gcm-usb.h"
#include "gcm-sensor-colormunki.h"

static void     gcm_sensor_colormunki_finalize	(GObject     *object);

#define GCM_SENSOR_COLORMUNKI_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GCM_TYPE_SENSOR_COLORMUNKI, GcmSensorColormunkiPrivate))

/**
 * GcmSensorColormunkiPrivate:
 *
 * Private #GcmSensorColormunki data
 **/
struct _GcmSensorColormunkiPrivate
{
	struct libusb_transfer		*transfer_interrupt;
	struct libusb_transfer		*transfer_state;
	GcmUsb				*usb;
};

G_DEFINE_TYPE (GcmSensorColormunki, gcm_sensor_colormunki, GCM_TYPE_SENSOR)

#define COLORMUNKI_VENDOR_ID			0x0971
#define COLORMUNKI_PRODUCT_ID			0x2007

#define COLORMUNKI_COMMAND_DIAL_ROTATE		0x00
#define COLORMUNKI_COMMAND_BUTTON_PRESSED	0x01
#define COLORMUNKI_COMMAND_BUTTON_RELEASED	0x02

#define	COLORMUNKI_BUTTON_STATE_RELEASED	0x00
#define	COLORMUNKI_BUTTON_STATE_PRESSED		0x01

#define	COLORMUNKI_DIAL_POSITION_PROJECTOR	0x00
#define	COLORMUNKI_DIAL_POSITION_SPOT		0x01
#define	COLORMUNKI_DIAL_POSITION_CALIBRATION	0x02
#define	COLORMUNKI_DIAL_POSITION_AMBIENT	0x03

/**
 * gcm_sensor_colormunki_print_data:
 **/
static void
gcm_sensor_colormunki_print_data (const gchar *title, const guchar *data, gsize length)
{
	guint i;

	if (!egg_debug_is_verbose ())
		return;

	if (g_strcmp0 (title, "request") == 0)
		g_print ("%c[%dm", 0x1B, 31);
	if (g_strcmp0 (title, "reply") == 0)
		g_print ("%c[%dm", 0x1B, 34);
	g_print ("%s\t", title);

	for (i=0; i< length; i++)
		g_print ("%02x [%c]\t", data[i], g_ascii_isprint (data[i]) ? data[i] : '?');

	g_print ("%c[%dm\n", 0x1B, 0);
}

static void
gcm_sensor_colormunki_submit_transfer (GcmSensorColormunki *sensor_colormunki);

/**
 * gcm_sensor_colormunki_refresh_state_transfer_cb:
 **/
static void
gcm_sensor_colormunki_refresh_state_transfer_cb (struct libusb_transfer *transfer)
{
//	GcmSensorColormunki *sensor_colormunki = GCM_SENSOR_COLORMUNKI (transfer->user_data);
	guint8 *reply = transfer->buffer + LIBUSB_CONTROL_SETUP_SIZE;

	if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
		egg_warning ("did not succeed");
		return;
	}

	/*
	 * Returns:  00 00
	 *           |/ ||
	 * dial pos -/  \--- button value
	 * - 00 = projector
	 * - 01 = spot
	 * - 02 = calibration
	 * - 03 = ambient
	 */
	if (reply[0] == COLORMUNKI_DIAL_POSITION_PROJECTOR)
		egg_debug ("projector");
	else if (reply[0] == COLORMUNKI_DIAL_POSITION_SPOT)
		egg_debug ("spot");
	else if (reply[0] == COLORMUNKI_DIAL_POSITION_CALIBRATION)
		egg_debug ("calibration");
	else if (reply[0] == COLORMUNKI_DIAL_POSITION_AMBIENT)
		egg_debug ("ambient");
	else
		egg_warning ("dial position unknown: 0x%02x", reply[0]);

	/* button state */
	if (reply[1] == COLORMUNKI_BUTTON_STATE_RELEASED) {
		egg_debug ("button released");
	} else if (reply[1] == COLORMUNKI_BUTTON_STATE_PRESSED) {
		egg_debug ("button pressed");
	} else {
		egg_warning ("switch state unknown: 0x%02x", reply[1]);
	}

	gcm_sensor_colormunki_print_data ("reply", reply, transfer->actual_length);
}

/**
 * gcm_sensor_colormunki_refresh_state:
 **/
static gboolean
gcm_sensor_colormunki_refresh_state (GcmSensorColormunki *sensor_colormunki, GError **error)
{
	gint retval;
	gboolean ret = FALSE;
	static guchar request[LIBUSB_CONTROL_SETUP_SIZE+2];
	libusb_device_handle *handle;
	GcmSensorColormunkiPrivate *priv = sensor_colormunki->priv;

	/* do sync request */
	handle = gcm_usb_get_device_handle (priv->usb);

	/* request new button state */
	libusb_fill_control_setup (request, LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE, 0x87, 0x00, 0, 2);
	libusb_fill_control_transfer (priv->transfer_state, handle, request, &gcm_sensor_colormunki_refresh_state_transfer_cb, sensor_colormunki, 2000);

	/* submit transfer */
	retval = libusb_submit_transfer (priv->transfer_state);
	if (retval < 0) {
		egg_warning ("failed to submit transfer: %s", libusb_strerror (retval));
		goto out;
	}

	/* success */
	ret = TRUE;
out:
	return ret;
}

/**
 * gcm_sensor_colormunki_transfer_cb:
 **/
static void
gcm_sensor_colormunki_transfer_cb (struct libusb_transfer *transfer)
{
	guint32 timestamp;
	GcmSensorColormunki *sensor_colormunki = GCM_SENSOR_COLORMUNKI (transfer->user_data);
	guint8 *reply = transfer->buffer;

	if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
		egg_warning ("did not succeed");
		return;
	}

	/*
	 *   subcmd ----\       /------------ 32 bit event time
	 *  cmd ----|\ ||       || || || ||
	 * Returns: 02 00 00 00 ac 62 07 00
	 * always zero ---||-||
	 *
	 * cmd is:
	 * 00	dial rotate
	 * 01	button pressed
	 * 02	button released
	 *
	 * subcmd is:
	 * 00	button event
	 * 01	dial rotate
	 */
	gcm_sensor_colormunki_print_data ("reply", reply, transfer->actual_length);
	timestamp = (reply[7] << 24) + (reply[6] << 16) + (reply[5] << 8) + (reply[4] << 0);
	/* we only care when the button is pressed */
	if (reply[0] == COLORMUNKI_COMMAND_BUTTON_RELEASED) {
		egg_debug ("ignoring button released");
		goto out;
	}

	if (reply[0] == COLORMUNKI_COMMAND_DIAL_ROTATE) {
		egg_warning ("dial rotate at %ims", timestamp);
	} else if (reply[0] == COLORMUNKI_COMMAND_BUTTON_PRESSED) {
		egg_debug ("button pressed at %ims", timestamp);
		gcm_sensor_button_pressed (GCM_SENSOR (sensor_colormunki));
	}

	/* get the device state */
	gcm_sensor_colormunki_refresh_state (sensor_colormunki, NULL);

out:
	/* get the next bit of data */
	gcm_sensor_colormunki_submit_transfer (sensor_colormunki);
}

/**
 * gcm_sensor_colormunki_submit_transfer:
 **/
static void
gcm_sensor_colormunki_submit_transfer (GcmSensorColormunki *sensor_colormunki)
{
	gint retval;
	static guchar reply[8];
	libusb_device_handle *handle;
	GcmSensorColormunkiPrivate *priv = sensor_colormunki->priv;

	handle = gcm_usb_get_device_handle (priv->usb);
	libusb_fill_interrupt_transfer (priv->transfer_interrupt,
					handle, 0x83, reply, 8,
					gcm_sensor_colormunki_transfer_cb,
					sensor_colormunki, -1);

	egg_debug ("submitting transfer");
	retval = libusb_submit_transfer (priv->transfer_interrupt);
	if (retval < 0)
		egg_warning ("failed to submit transfer: %s", libusb_strerror (retval));
}

/**
 * gcm_sensor_colormunki_playdo:
 **/
static gboolean
gcm_sensor_colormunki_playdo (GcmSensor *sensor, GError **error)
{
	gboolean ret = TRUE;
	GcmSensorColormunki *sensor_colormunki = GCM_SENSOR_COLORMUNKI (sensor);
//	GcmSensorColormunkiPrivate *priv = sensor_colormunki->priv;

	egg_debug ("submit transfer");
	gcm_sensor_colormunki_submit_transfer (sensor_colormunki);
	//ret = gcm_sensor_colormunki_refresh_state (sensor_colormunki, error);

	return ret;
}

/**
 * gcm_sensor_colormunki_startup:
 **/
static gboolean
gcm_sensor_colormunki_startup (GcmSensor *sensor, GError **error)
{
	gboolean ret = FALSE;
	GcmSensorColormunki *sensor_colormunki = GCM_SENSOR_COLORMUNKI (sensor);
	GcmSensorColormunkiPrivate *priv = sensor_colormunki->priv;

	/* connect */
	ret = gcm_usb_connect (priv->usb,
			       COLORMUNKI_VENDOR_ID, COLORMUNKI_PRODUCT_ID,
			       0x01, 0x00, error);
	if (!ret)
		goto out;

	/* attach to the default mainloop */
	gcm_usb_attach_to_context (priv->usb, NULL);

	/* find device */
	ret = gcm_sensor_colormunki_playdo (sensor, error);
	if (!ret)
		goto out;
out:
	return ret;
}

/**
 * gcm_sensor_colormunki_class_init:
 **/
static void
gcm_sensor_colormunki_class_init (GcmSensorColormunkiClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GcmSensorClass *parent_class = GCM_SENSOR_CLASS (klass);
	object_class->finalize = gcm_sensor_colormunki_finalize;

	/* setup klass links */
//	parent_class->get_ambient = gcm_sensor_colormunki_get_ambient;
//	parent_class->set_leds = gcm_sensor_colormunki_set_leds;
//	parent_class->sample = gcm_sensor_colormunki_sample;
	parent_class->startup = gcm_sensor_colormunki_startup;

	g_type_class_add_private (klass, sizeof (GcmSensorColormunkiPrivate));
}

/**
 * gcm_sensor_colormunki_init:
 **/
static void
gcm_sensor_colormunki_init (GcmSensorColormunki *sensor)
{
	GcmSensorColormunkiPrivate *priv;
	priv = sensor->priv = GCM_SENSOR_COLORMUNKI_GET_PRIVATE (sensor);
	priv->transfer_interrupt = libusb_alloc_transfer (0);
	priv->transfer_state = libusb_alloc_transfer (0);
	priv->usb = gcm_usb_new ();
}

/**
 * gcm_sensor_colormunki_finalize:
 **/
static void
gcm_sensor_colormunki_finalize (GObject *object)
{
	GcmSensorColormunki *sensor = GCM_SENSOR_COLORMUNKI (object);
	GcmSensorColormunkiPrivate *priv = sensor->priv;

	/* FIXME: cancel transfers if in progress */

	/* detach from the main loop */
	g_object_unref (priv->usb);

	/* free transfers */
	libusb_free_transfer (priv->transfer_interrupt);
	libusb_free_transfer (priv->transfer_state);

	G_OBJECT_CLASS (gcm_sensor_colormunki_parent_class)->finalize (object);
}

/**
 * gcm_sensor_colormunki_new:
 *
 * Return value: a new #GcmSensor object.
 **/
GcmSensor *
gcm_sensor_colormunki_new (void)
{
	GcmSensorColormunki *sensor;
	sensor = g_object_new (GCM_TYPE_SENSOR_COLORMUNKI,
			       "native", TRUE,
			       NULL);
	return GCM_SENSOR (sensor);
}


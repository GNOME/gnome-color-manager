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
	libusb_context			*ctx;
	libusb_device_handle		*handle;
};

G_DEFINE_TYPE (GcmSensorColormunki, gcm_sensor_colormunki, GCM_TYPE_SENSOR)

#define COLORMUNKI_VENDOR_ID			0x0971
#define COLORMUNKI_PRODUCT_ID			0x2007

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

/**
 * gcm_sensor_colormunki_find_device:
 **/
static gboolean
gcm_sensor_colormunki_find_device (GcmSensorColormunki *sensor_colormunki, GError **error)
{
	gint retval;
	gboolean ret = FALSE;
	GcmSensorColormunkiPrivate *priv = sensor_colormunki->priv;

	/* open device */
	sensor_colormunki->priv->handle = libusb_open_device_with_vid_pid (priv->ctx, COLORMUNKI_VENDOR_ID, COLORMUNKI_PRODUCT_ID);
	if (priv->handle == NULL) {
		g_set_error (error, GCM_SENSOR_ERROR,
			     GCM_SENSOR_ERROR_INTERNAL,
			     "failed to open device: %s", libusb_strerror (retval));
		goto out;
	}

	/* set configuration and interface */
	retval = libusb_set_configuration (priv->handle, 1);
	if (retval < 0) {
		g_set_error (error, GCM_SENSOR_ERROR,
			     GCM_SENSOR_ERROR_INTERNAL,
			     "failed to set configuration: %s", libusb_strerror (retval));
		goto out;
	}
	retval = libusb_claim_interface (priv->handle, 0);
	if (retval < 0) {
		g_set_error (error, GCM_SENSOR_ERROR,
			     GCM_SENSOR_ERROR_INTERNAL,
			     "failed to claim interface: %s", libusb_strerror (retval));
		goto out;
	}

	/* success */
	ret = TRUE;
out:
	return ret;
}

#define COLORMUNKI_COMMAND_DIAL_ROTATE		0x00
#define COLORMUNKI_COMMAND_BUTTON_PRESSED	0x01
#define COLORMUNKI_COMMAND_BUTTON_RELEASED	0x02

/**
 * gcm_sensor_colormunki_playdo:
 **/
static gboolean
gcm_sensor_colormunki_playdo (GcmSensor *sensor, GError **error)
{
	gint retval;
	gsize reply_read;
	guchar reply[8];
	guint i;
	guint32 event;
	GcmSensorColormunki *sensor_colormunki = GCM_SENSOR_COLORMUNKI (sensor);
	GcmSensorColormunkiPrivate *priv = sensor_colormunki->priv;

	/* open device */
	for (i=0; i<9999; i++) {

		reply[0] = 0x00;
		reply[1] = 0x00;
		reply[2] = 0x00;
		reply[3] = 0x00;
		reply[4] = 0x00;
		reply[5] = 0x00;
		reply[6] = 0x00;
		reply[7] = 0x00;

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
 		retval = libusb_interrupt_transfer (priv->handle, 0x83,
						    reply, 8, (gint*)&reply_read,
						    100000);
		if (retval < 0) {
			egg_error ("failed to get data");
		}

		event = (reply[7] << 24) + (reply[6] << 16) + (reply[5] << 8) + (reply[4] << 0);

		gcm_sensor_colormunki_print_data ("reply", reply, reply_read);

		/* we only care when the button is pressed */
		if (reply[0] == COLORMUNKI_COMMAND_BUTTON_RELEASED) {
			egg_debug ("ignoring button released");
			continue;
		}

		if (reply[0] == COLORMUNKI_COMMAND_DIAL_ROTATE)
			egg_warning ("dial rotate at %ims", event);
		else if (reply[0] == COLORMUNKI_COMMAND_BUTTON_PRESSED)
			egg_warning ("button pressed at %ims", event);
	}
	return TRUE;
}

/**
 * gcm_sensor_colormunki_startup:
 **/
static gboolean
gcm_sensor_colormunki_startup (GcmSensor *sensor, GError **error)
{
	gboolean ret = FALSE;
	gint retval;
	GcmSensorColormunki *sensor_colormunki = GCM_SENSOR_COLORMUNKI (sensor);
	GcmSensorColormunkiPrivate *priv = sensor_colormunki->priv;

	/* connect */
	retval = libusb_init (&priv->ctx);
	if (retval < 0) {
		egg_warning ("failed to init libusb: %s", libusb_strerror (retval));
		goto out;
	}

	/* find device */
	ret = gcm_sensor_colormunki_find_device (sensor_colormunki, error);
	if (!ret)
		goto out;

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
}

/**
 * gcm_sensor_colormunki_finalize:
 **/
static void
gcm_sensor_colormunki_finalize (GObject *object)
{
	GcmSensorColormunki *sensor = GCM_SENSOR_COLORMUNKI (object);
	GcmSensorColormunkiPrivate *priv = sensor->priv;

	/* close device */
	libusb_close (priv->handle);
	if (priv->ctx != NULL)
		libusb_exit (priv->ctx);

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


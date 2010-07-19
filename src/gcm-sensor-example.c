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

/*
 * I've reversed engineered this myself using USB Snoop on Windows XP,
 * in a KVM virtual machine. I've also used the verbose logging in argyll
 * without actually looking at the code. Please see ../docs/huey for data.
 */

#include <glib.h>
#include <libcolor-glib.h>
#include <libusb-1.0/libusb.h>

#include "egg-debug.h"

/**
 * print_data:
 **/
static void
print_data (const gchar *title, const guchar *data, gsize length)
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
	g_print ("\n%s\t", title);
	for (i=0; i< length; i++)
		g_print ("%02x ", data[i]);

	g_print ("%c[%dm\n", 0x1B, 0);
}

/**
 * main:
 **/
int
main (int argc, char **argv)
{
	GOptionContext *context;

	context = g_option_context_new ("gnome-color-manager sensor example");
	g_option_context_add_group (context, egg_debug_get_option_group ());
	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);

	g_type_init ();

#if 0
	gboolean ret;
	GError *error = NULL;
	GcmSensor *sensor;
	gdouble value;
	GcmColorXYZ values;

	/* create new sensor */
	sensor = gcm_sensor_huey_new ();

	/* set mode */
	gcm_sensor_set_output_type (sensor, GCM_SENSOR_OUTPUT_TYPE_LCD);

	/* get ambient */
	ret = gcm_sensor_get_ambient (sensor, &value, &error);
	if (!ret) {
		g_warning ("failed to get ambient: %s", error->message);
		g_error_free (error);
		goto out;
	}
	g_debug ("ambient = %.1lf Lux", value);

	/* sample color */
	ret = gcm_sensor_sample (sensor, &values, &error);
	if (!ret) {
		g_warning ("failed to measure: %s", error->message);
		g_error_free (error);
		goto out;
	}
	g_debug ("X=%0.4lf, Y=%0.4lf, Z=%0.4lf", values.X, values.Y, values.Z);
out:
	g_object_unref (sensor);
	return 0;
#else
{
	gint retval;
	gsize reply_read;
	guchar reply[8];
	guint i;
	libusb_device_handle *handle;

	/* connect */
	retval = libusb_init (NULL);
	if (retval < 0) {
		egg_warning ("failed to init libusb: %s", libusb_strerror (retval));
		goto out;
	}

#define COLORMUNKI_VENDOR_ID			0x0971
#define COLORMUNKI_PRODUCT_ID			0x2007

	/* open device */
	handle = libusb_open_device_with_vid_pid (NULL, COLORMUNKI_VENDOR_ID, COLORMUNKI_PRODUCT_ID);
	if (handle == NULL) {
		egg_error ("failed to open device: %s", libusb_strerror (retval));
		goto out;
	}

	/* set configuration and interface */
	retval = libusb_set_configuration (handle, 1);
	if (retval < 0) {
		egg_error ("failed to set configuration: %s", libusb_strerror (retval));
		goto out;
	}
	retval = libusb_claim_interface (handle, 0);
	if (retval < 0) {
		egg_error ("failed to claim interface: %s", libusb_strerror (retval));
		goto out;
	}

	for (i=0; i<9999; i++) {

		reply[0] = 0x00;
		reply[1] = 0x00;
		reply[2] = 0x00;
		reply[3] = 0x00;
		reply[4] = 0x00;
		reply[5] = 0x00;
		reply[6] = 0x00;
		reply[7] = 0x00;

		/* get sync data */
		retval = libusb_interrupt_transfer (handle, 0x83,
						    reply, 8, (gint*)&reply_read,
						    100000);
		if (retval < 0) {
			egg_error ("failed to get data");
		}

#define COLORMUNKI_COMMAND_DIAL_ROTATE		0x00
#define COLORMUNKI_COMMAND_BUTTON_PRESSED	0x01
#define COLORMUNKI_COMMAND_BUTTON_RELEASED	0x02

		print_data ("reply", reply, reply_read);
		if (reply[0] == COLORMUNKI_COMMAND_DIAL_ROTATE)
			egg_warning ("dial rotate");
		if (reply[0] == COLORMUNKI_COMMAND_BUTTON_PRESSED)
			egg_warning ("button pressed");
		if (reply[0] == COLORMUNKI_COMMAND_BUTTON_RELEASED)
			egg_warning ("button released");

/*
 *   subcmd ----\          /---- some kind of time counter, seconds?
 *  cmd ----|\ ||          ||
 * Returns: 02 00 00 00 ac 62 07 00
 *                \|-||----------||-----always zero
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

	}

	libusb_exit (NULL);
	/* get device */
	/* set configuration */
	/* get ambient */
	/* close device */
}
out:
#endif
	return 0;
}


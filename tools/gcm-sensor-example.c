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

#include "gcm-sensor-colormunki.h"
#include "gcm-debug.h"

/**
 * gcm_sensor_example_loop_quit_cb:
 **/
static gboolean
gcm_sensor_example_loop_quit_cb (GMainLoop *loop)
{
	g_main_loop_quit (loop);
	return FALSE;
}

/**
 * main:
 **/
int
main (int argc, char **argv)
{
	GOptionContext *context;
	gboolean ret;
	GError *error = NULL;
	GcmSensor *sensor;
	gdouble value;
	GcmColorXYZ values;
	GMainLoop *loop;

	context = g_option_context_new ("gnome-color-manager sensor example");
	g_option_context_add_group (context, gcm_debug_get_option_group ());
	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);

	g_type_init ();
	g_thread_init (NULL);

	/* spin the loop for a few seconds */
	loop = g_main_loop_new (NULL, FALSE);
	g_timeout_add_seconds (10, (GSourceFunc) gcm_sensor_example_loop_quit_cb, loop);

	/* create new sensor */
	sensor = gcm_sensor_colormunki_new ();

	/* set mode */
	gcm_sensor_set_output_type (sensor, GCM_SENSOR_OUTPUT_TYPE_LCD);

	/* get ambient */
	ret = gcm_sensor_get_ambient (sensor, NULL, &value, &error);
	if (!ret) {
		g_warning ("failed to get ambient: %s", error->message);
		g_error_free (error);
		goto out;
	}
	g_debug ("ambient = %.1lf Lux", value);

	/* sample color */
	ret = gcm_sensor_sample (sensor, NULL, &values, &error);
	if (!ret) {
		g_warning ("failed to measure: %s", error->message);
		g_error_free (error);
		goto out;
	}
	g_debug ("X=%0.4lf, Y=%0.4lf, Z=%0.4lf", values.X, values.Y, values.Z);

	/* spin the loop */
	g_main_loop_run (loop);

out:
	g_main_loop_unref (loop);
	g_object_unref (sensor);
	return 0;
}


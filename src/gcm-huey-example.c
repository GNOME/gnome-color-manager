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

#include "gcm-sensor-huey.h"

/**
 * main:
 **/
int
main (void)
{
	gboolean ret;
	GError *error = NULL;
	GcmSensor *sensor;
	gdouble value;
	GcmColorXYZ values;

	g_type_init ();

	/* start sensor */
	sensor = gcm_sensor_huey_new ();
	ret = gcm_sensor_startup (sensor, &error);
	if (!ret) {
		g_warning ("failed to start sensor: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* set LEDs */
	ret = gcm_sensor_set_leds (sensor, 0x0f, &error);
	if (!ret) {
		g_warning ("failed to send leds: %s", error->message);
		g_error_free (error);
		goto out;
	}

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

	/* set LEDs */
	ret = gcm_sensor_set_leds (sensor, 0x00, &error);
	if (!ret) {
		g_warning ("failed to send leds: %s", error->message);
		g_error_free (error);
		goto out;
	}

out:
	g_object_unref (sensor);
	return 0;
}


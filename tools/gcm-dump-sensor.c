/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include <glib/gi18n.h>
#include <locale.h>

#include "gcm-sensor-client.h"

/**
 * main:
 **/
int
main (int argc, char **argv)
{
	guint retval = 0;
	gboolean ret;
	GError *error = NULL;
	GString *data = NULL;
	GOptionContext *context;
	GcmSensorClient *sensor_client;
	GcmSensor *sensor;
	gchar *filename = NULL;

	setlocale (LC_ALL, "");

	g_type_init ();

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	context = g_option_context_new ("ICC profile dump program");
	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);

	/* find sensors */
	sensor_client = gcm_sensor_client_new ();

	/* get the default sensor */
	sensor = gcm_sensor_client_get_sensor (sensor_client);
	if (sensor == NULL) {
		g_print ("FAILED: Failed to find a supported sensor\n");
		goto out;
	}

	/* check is native */
	if (!gcm_sensor_is_native (sensor)) {
		g_print ("FAILED: Failed to find a native sensor\n");
		goto out;
	}

	/* dump details */
	filename = g_strdup_printf ("./%s-sensor-dump.txt", gcm_sensor_kind_to_string (gcm_sensor_get_kind (sensor)));
	g_print ("Dumping sensor details to %s... ", filename);
	data = g_string_new ("");
	ret = gcm_sensor_dump (sensor, data, &error);
	if (!ret) {
		g_print ("FAILED: Failed to dump sensor: %s\n", error->message);
		g_error_free (error);
		goto out;
	}

	/* write to file */
	ret = g_file_set_contents (filename, data->str, data->len, &error);
	if (!ret) {
		g_print ("FAILED: Failed to write file: %s\n", error->message);
		g_error_free (error);
		goto out;
	}

	/* success */
	g_print ("SUCCESS!!\n");
out:
	g_free (filename);
	if (data != NULL)
		g_string_free (data, TRUE);
	g_object_unref (sensor_client);
	return retval;
}


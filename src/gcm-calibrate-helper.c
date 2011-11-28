/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011 Richard Hughes <richard@hughsie.com>
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
#include <gtk/gtk.h>
#include <locale.h>

#include "gcm-debug.h"
#include "gcm-calibrate.h"

/**
 * gcm_calibrate_setup_sensor:
 **/
static gboolean
gcm_calibrate_setup_sensor (CdClient *client,
			    GcmCalibrate *calibrate,
			    GError **error)
{
	CdSensor *sensor_tmp;
	gboolean ret;
	GPtrArray *sensors = NULL;

	/* get sensor */
	sensors = cd_client_get_sensors_sync (client, NULL, error);
	if (sensors == NULL) {
		ret = FALSE;
		goto out;
	}
	if (sensors->len == 0) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0,
				     "No native sensors plugged in!");
		goto out;
	}
	sensor_tmp = g_ptr_array_index (sensors, 0);
	ret = cd_sensor_connect_sync (sensor_tmp, NULL, error);
	if (!ret)
		goto out;

	/* set sensor */
	gcm_calibrate_set_sensor (calibrate, sensor_tmp);
out:
	if (sensors != NULL)
		g_ptr_array_unref (sensors);
	return ret;
}

/**
 * main:
 **/
int
main (int argc, char **argv)
{
	CdClient *client = NULL;
	CdDevice *device = NULL;
	gboolean ret;
	gchar *device_id = NULL;
	GcmCalibrate *calibrate = NULL;
	GError *error = NULL;
	GOptionContext *context;
	guint retval = 1;
	guint xid = 0;

	const GOptionEntry options[] = {
		{ "device", '\0', 0, G_OPTION_ARG_STRING, &device_id,
			/* TRANSLATORS: command line option */
			_("Use this device for profiling"), NULL },
		{ "xid", '\0', 0, G_OPTION_ARG_INT, &xid,
			/* TRANSLATORS: command line option */
			_("Make the window modal to this XID"), NULL },
		{ NULL}
	};

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	gtk_init (&argc, &argv);

	/* TRANSLATORS: just dumps the EDID to disk */
	context = g_option_context_new (_("gcm-dispread"));
	g_option_context_add_main_entries (context, options, NULL);
	g_option_context_add_group (context, gcm_debug_get_option_group ());
	g_option_context_add_group (context, gtk_get_option_group (TRUE));
	ret = g_option_context_parse (context, &argc, &argv, &error);
	if (!ret)
		goto out;

	/* get client */
	client = cd_client_new ();
	ret = cd_client_connect_sync (client, NULL, &error);
	if (!ret)
		goto out;

	/* check device */
	if (device_id == NULL) {
		g_print ("No device-id specified\n");
		goto out;
	}
	device = cd_client_find_device_sync (client,
					     device_id,
					     NULL,
					     &error);
	if (device == NULL) {
		ret = FALSE;
		goto out;
	}
	ret = cd_device_connect_sync (device,
				      NULL,
				      &error);
	if (!ret)
		goto out;

	/* correct arguments */
	if (argc == 4 &&
	    g_str_has_suffix (argv[1], "ti1") &&
	    g_str_has_suffix (argv[2], "cal") &&
	    g_str_has_suffix (argv[3], "ti3")) {

		/* setup calibration */
		calibrate = gcm_calibrate_new ();
		ret = gcm_calibrate_setup_sensor (client,
						  calibrate,
						  &error);
		if (!ret)
			goto out;

		/* mark device to be profiled in colord */
		ret = cd_device_profiling_inhibit_sync (device,
							NULL,
							&error);
		if (!ret)
			goto out;

		/* convert the ti1 file to a ti3 file */
		ret = gcm_calibrate_display_characterize (calibrate,
							  argv[1],
							  argv[2],
							  argv[3],
							  device,
							  NULL, /* window */
							  &error);
		if (!ret)
			goto out;

		/* success */
		g_print ("%s: %s\n", _("Wrote file"), argv[2]);

	} else {
		ret = FALSE;
		g_set_error_literal (&error, 1, 0,
				     "Specify one of:\n"
				     "file.ti1 file.ti3\n"
				     "file.cal");
		goto out;
	}
out:
	if (!ret) {
		g_print ("%s: %s\n",
			 _("Failed to calibrate"),
			 error->message);
		g_error_free (error);
	}

	/* success */
	retval = 0;

	g_option_context_free (context);
	if (device != NULL)
		g_object_unref (device);
	if (client != NULL)
		g_object_unref (client);
	if (calibrate != NULL)
		g_object_unref (calibrate);
	g_free (device_id);
	return retval;
}

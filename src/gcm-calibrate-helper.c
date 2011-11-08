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
 * gcm_calibrate_helper_ti1_to_ti3:
 **/
static gboolean
gcm_calibrate_helper_ti1_to_ti3 (const gchar *device_id,
				 const gchar *ti1_fn,
				 const gchar *ti3_fn,
				 GError **error)
{
	CdClient *client = NULL;
	CdSensor *sensor_tmp;
	gboolean ret;
	GcmCalibrate *calibrate = NULL;
	GPtrArray *sensors = NULL;

	/* get sensor */
	client = cd_client_new ();
	ret = cd_client_connect_sync (client, NULL, error);
	if (!ret)
		goto out;

	sensors = cd_client_get_sensors_sync (client, NULL, error);
	if (sensors == NULL) {
		ret = FALSE;
		goto out;
	}
	if (sensors->len == 0) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0,
				     "No sensors plugged in!");
		goto out;
	}
	sensor_tmp = g_ptr_array_index (sensors, 0);
	ret = cd_sensor_connect_sync (sensor_tmp, NULL, error);
	if (!ret)
		goto out;

	/* set sensor */
	calibrate = gcm_calibrate_new ();
	gcm_calibrate_set_sensor (calibrate, sensor_tmp);

	/* convert the ti1 file to a ti3 file */
	ret = gcm_calibrate_display_characterize (calibrate,
						  ti1_fn,
						  ti3_fn,
						  NULL, /* device */
						  NULL, /* window */
						  error);
	if (!ret)
		goto out;

	/* success */
	g_print ("%s: %s\n", _("Wrote file"), ti3_fn);
out:
	if (sensors != NULL)
		g_ptr_array_unref (sensors);
	if (calibrate != NULL)
		g_object_unref (calibrate);
	return ret;
}

/**
 * main:
 **/
int
main (int argc, char **argv)
{
	gboolean ret;
	gchar *device_id = NULL;
	GError *error = NULL;
	GOptionContext *context;
	guint retval = 1;
	guint xid = 0;

	const GOptionEntry options[] = {
		{ "device", '\0', 0, G_OPTION_ARG_INT, &device_id,
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
	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);

	/* correct arguments */
	if (argc == 3 &&
	    g_str_has_suffix (argv[1], "ti1") &&
	    g_str_has_suffix (argv[2], "ti3")) {
		ret = gcm_calibrate_helper_ti1_to_ti3 (device_id,
						       argv[1],
						       argv[2],
						       &error);
	} else {
		ret = FALSE;
		g_print ("%s\n", _("Specify one of:"));
		g_print ("file.ti1 file.ti3\n");
		goto out;
	}
	if (!ret) {
		g_print ("%s: %s\n",
			 _("Failed to calibrate"),
			 error->message);
		g_error_free (error);
		goto out;
	}

	retval = 0;
out:
	g_free (device_id);
	return retval;
}

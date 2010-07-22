/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009-2010 Richard Hughes <richard@hughsie.com>
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

#include <config.h>
#include <glib/gstdio.h>

#include <libcolor-glib.h>

/**
 * show_device_md5_cb:
 **/
static void
show_device_md5_cb (GcmDdcDevice *device, gpointer user_data)
{
	GError *error = NULL;
	const gchar *desc;

	desc = gcm_ddc_device_get_edid_md5 (device, &error);
	if (desc == NULL) {
		g_warning ("failed to get EDID: %s", error->message);
		g_error_free (error);
		goto out;
	}
	g_print ("EDID: \t%s\n", desc);
out:
	return;
}

/**
 * show_device:
 **/
static void
show_device (GcmDdcDevice *device)
{
	guint i, j;
	GPtrArray *array;
	GcmDdcControl *control;
	GArray *values;
	GError *error = NULL;
	const gchar *desc;

	desc = gcm_ddc_device_get_edid_md5 (device, &error);
	if (desc == NULL) {
		g_warning ("failed to get EDID: %s", error->message);
		g_error_free (error);
		goto out;
	}
	g_print ("EDID: \t%s\n", desc);

	desc = gcm_ddc_device_get_pnpid (device, &error);
	if (desc == NULL) {
		g_warning ("failed to get PNPID: %s", error->message);
		g_error_free (error);
		goto out;
	}
	g_print ("PNPID:\t%s\n", desc);

	desc = gcm_ddc_device_get_model (device, &error);
	if (desc == NULL) {
		g_warning ("failed to get model: %s", error->message);
		g_error_free (error);
		goto out;
	}
	g_print ("Model:\t%s\n", desc);

	array = gcm_ddc_device_get_controls (device, &error);
	if (array == NULL) {
		g_warning ("failed to get caps: %s", error->message);
		g_error_free (error);
		goto out;
	}
	for (i = 0; i < array->len; i++) {
		control = g_ptr_array_index (array, i);

		desc = gcm_ddc_control_get_description (control);
		g_print ("0x%02x\t[%s]", gcm_ddc_control_get_id (control), desc != NULL ? desc : "unknown");

		/* print allowed values */
		values = gcm_ddc_control_get_values (control);
		if (values->len > 0) {
			g_print ("\t ( ");
			for (j=0; j<values->len; j++) {
				g_print ("%i ", g_array_index (values, guint16, j));
			}
			g_print (")");
		}
		g_print ("\n");
		g_array_unref (values);
	}
	g_ptr_array_unref (array);
out:
	return;
}

/**
 * show_device_cb:
 **/
static void
show_device_cb (GcmDdcDevice *device, gpointer user_data)
{
	show_device (device);
}

/**
 * main:
 **/
int
main (int argc, char **argv)
{
	gboolean ret;
	GcmVerbose verbose = GCM_VERBOSE_NONE;
	gboolean enumerate = FALSE;
	gboolean caps = FALSE;
	gchar *display_md5 = NULL;
	gchar *control_name = NULL;
	gboolean control_get = FALSE;
	gint control_set = -1;
	GcmDdcClient *client;
	GcmDdcDevice *device = NULL;
	GcmDdcControl *control = NULL;
	gint brightness = -1;
	GOptionContext *context;
	GError *error = NULL;
	GPtrArray *array;
	guint16 value, max;
	guchar idx;
	guint i;

	const GOptionEntry options[] = {
		{ "verbose", '\0', 0, G_OPTION_ARG_INT, &verbose,
		  "Enable verbose debugging mode", NULL},
		{ "enumerate", '\0', 0, G_OPTION_ARG_NONE, &enumerate,
		  "Enumerate all displays and display capabilities", NULL},
		{ "display", '\0', 0, G_OPTION_ARG_STRING, &display_md5,
		  "Set the display MD5 to operate on", NULL},
		{ "caps", '\0', 0, G_OPTION_ARG_NONE, &caps,
		  "Print the capabilities of the selected display", NULL},
		{ "brightness", '\0', 0, G_OPTION_ARG_INT, &brightness,
		  "Set the display brightness", NULL},
		{ "control", '\0', 0, G_OPTION_ARG_STRING, &control_name,
		  "Use a control value, e.g. 'select-color-preset'", NULL},
		{ "get", '\0', 0, G_OPTION_ARG_NONE, &control_get,
		  "Get a control value", NULL},
		{ "set", '\0', 0, G_OPTION_ARG_INT, &control_set,
		  "Set a control value", NULL},
		{ NULL}
	};

	g_type_init ();

	context = g_option_context_new ("DDC/CI utility program");
	g_option_context_set_summary (context, "This exposes the DDC/CI protocol used by most modern displays.");
	g_option_context_add_main_entries (context, options, NULL);
	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);

	client = gcm_ddc_client_new ();
	gcm_ddc_client_set_verbose (client, verbose ? GCM_VERBOSE_PROTOCOL : GCM_VERBOSE_NONE);

	/* we want to enumerate all devices */
	if (enumerate) {
		array = gcm_ddc_client_get_devices (client, &error);
		if (array == NULL) {
			g_warning ("failed to get device list: %s", error->message);
			goto out;
		}

		/* show device details */
		g_ptr_array_foreach (array, (GFunc) show_device_cb, NULL);
		g_ptr_array_unref (array);
		goto out;
	}

	if (display_md5 == NULL) {
		array = gcm_ddc_client_get_devices (client, &error);
		if (array == NULL) {
			g_warning ("failed to get device list: %s", error->message);
			goto out;
		}

		/* show device details */
		g_print ("No --display specified, please select from:\n");
		g_ptr_array_foreach (array, (GFunc) show_device_md5_cb, NULL);
		g_ptr_array_unref (array);
		goto out;
	}

	/* get the correct device */
	device = gcm_ddc_client_get_device_from_edid (client, display_md5, &error);
	if (device == NULL) {
		g_warning ("failed to get device list: %s", error->message);
		goto out;
	}

	/* get caps? */
	if (caps) {
		show_device (device);
		goto out;
	}

	/* set brightness? */
	if (brightness != -1) {
		control = gcm_ddc_device_get_control_by_id (device, GCM_DDC_CONTROL_ID_BRIGHTNESS, &error);
		if (control == NULL) {
			g_warning ("Failed to get brightness control: %s", error->message);
			goto out;
		}

		/* get old value */
		ret = gcm_ddc_control_request (control, &value, &max, &error);
		if (!ret) {
			g_warning ("failed to read: %s", error->message);
			goto out;
		}

		/* set new value */
		ret = gcm_ddc_control_set (control, brightness, &error);
		if (!ret) {
			g_warning ("failed to write: %s", error->message);
			goto out;
		}

		/* print what we did */
		g_print  ("brightness before was %i%%, now is %i%% max is %i%%\n", value, brightness, max);
		g_object_unref (control);
		goto out;
	}

	/* get named control */
	if (control_name == NULL) {
		g_print ("you need to specify a control name with --control\n");
		show_device (device);
	}
	idx = gcm_get_vcp_index_from_description (control_name);
	if (idx == GCM_VCP_ID_INVALID) {
		const gchar *description;
		g_warning ("Failed to match description, choose from:");
		for (i=0; i<0xff; i++) {
			description = gcm_get_vcp_description_from_index (i);
			if (description != NULL)
				g_print ("* %s\n", description);
		}
		goto out;
	}

	/* get control */
	control = gcm_ddc_device_get_control_by_id (device, idx, &error);
	if (control == NULL) {
		g_warning ("Failed to get control: %s", error->message);
		goto out;
	}

	/* get named control */
	if (control_get) {
		/* get old value */
		ret = gcm_ddc_control_request (control, &value, &max, &error);
		if (!ret) {
			g_warning ("failed to read: %s", error->message);
			goto out;
		}

		/* print what we got */
		g_print  ("%s is %i, max is %i\n", control_name, value, max);
	}

	/* set named control */
	if (control_set != -1) {

		/* set new value */
		ret = gcm_ddc_control_set (control, control_set, &error);
		if (!ret) {
			g_warning ("failed to write: %s", error->message);
			goto out;
		}
	}
out:
	g_clear_error (&error);
	ret = gcm_ddc_client_close (client, &error);
	if (!ret) {
		g_warning ("failed to close: %s", error->message);
		g_error_free (error);
		goto out;
	}
	if (device != NULL)
		g_object_unref (device);
	if (control != NULL)
		g_object_unref (control);
	g_object_unref (client);
	g_free (display_md5);
	g_free (control_name);
	return 0;
}

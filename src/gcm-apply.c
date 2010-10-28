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

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <math.h>
#include <locale.h>

#include "gcm-utils.h"
#include "gcm-client.h"
#include "gcm-device-xrandr.h"
#include "gcm-debug.h"

/**
 * gcm_apply_create_icc_profile_for_edid:
 **/
static gboolean
gcm_apply_create_icc_profile_for_edid (GcmDevice *device, const gchar *filename, GError **error)
{
	gboolean ret = FALSE;
	GcmProfile *profile;

	/* generate */
	profile = gcm_device_generate_profile (device, error);
	if (profile == NULL)
		goto out;

	/* ensure the per-user directory exists */
	ret = gcm_utils_mkdir_for_filename (filename, error);
	if (!ret)
		goto out;

	/* save this */
	ret = gcm_profile_save (profile, filename, error);
	if (!ret)
		goto out;

	/*
	 * When we get here there are 4 possible situations:
	 *
	 * 1. profiles assigned, use-edid-profile=TRUE		-> add to profiles if not already added, but not as default
	 * 2. profiles assigned, use-edid-profile=FALSE		-> do nothing
	 * 3. no profiles, use-edid-profile=TRUE		-> add to profiles as default
	 * 4. no profiles, use-edid-profile=FALSE		-> do nothing
	 */

	/* do we set this by default? */
	if (!gcm_device_get_use_edid_profile (device)) {
		g_debug ("not using auto-edid profile as device profile");
		goto out;
	}

	/* add to the profiles list */
	ret = gcm_device_profile_add (device, profile, NULL);
	if (ret) {
		/* need to save new list */
		ret = gcm_device_save (device, error);
		if (!ret)
			goto out;
	} else {
		/* if this failed, it's because it's already associated
		 * with the device which is okay with us */
		g_debug ("already added auto-edid profile, not adding %s",
			 gcm_profile_get_checksum (profile));
		ret = TRUE;
	}
out:
	if (profile != NULL)
		g_object_unref (profile);
	return ret;
}

/**
 * main:
 **/
int
main (int argc, char **argv)
{
	gboolean ret;
	gboolean login = FALSE;
	guint retval = 0;
	GError *error = NULL;
	GOptionContext *context;
	GPtrArray *array = NULL;
	guint i;
	GcmClient *client = NULL;
	GcmDevice *device;
	gchar *filename;
	gchar *path;

	const GOptionEntry options[] = {
		{ "login", 'l', 0, G_OPTION_ARG_NONE, &login,
		  /* TRANSLATORS: we use this mode at login as we're sure there are no previous settings to clear */
		  _("Do not attempt to clear previously applied settings"), NULL },
		{ NULL}
	};

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	gtk_init (&argc, &argv);

	context = g_option_context_new ("gnome-color-manager apply program");
	g_option_context_add_main_entries (context, options, NULL);
	g_option_context_add_group (context, gcm_debug_get_option_group ());
	g_option_context_add_group (context, gtk_get_option_group (TRUE));
	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);

	/* get devices */
	client = gcm_client_new ();
	ret = gcm_client_coldplug (client, GCM_CLIENT_COLDPLUG_XRANDR, &error);
	if (!ret) {
		g_warning ("failed to get devices: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* set for each output */
	array = gcm_client_get_devices (client);
	for (i=0; i<array->len; i++) {
		device = g_ptr_array_index (array, i);

		/* optimize for login to save a few hundred ms */
		gcm_device_xrandr_set_remove_atom (GCM_DEVICE_XRANDR (device), !login);

		/* do we have to generate a edid profile? */
		filename = g_strdup_printf ("edid-%s.icc", gcm_device_xrandr_get_edid_md5 (GCM_DEVICE_XRANDR (device)));
		path = g_build_filename (g_get_user_data_dir (), "icc", filename, NULL);
		if (g_file_test (path, G_FILE_TEST_EXISTS)) {
			g_debug ("auto-profile edid %s exists", path);
		} else {
			g_debug ("auto-profile edid does not exist, creating as %s", path);
			ret = gcm_apply_create_icc_profile_for_edid (device, path, &error);
			if (!ret) {
				g_warning ("failed to create profile from EDID data: %s",
					     error->message);
				g_clear_error (&error);
			}
		}
		g_free (filename);
		g_free (path);

		/* set gamma for device */
		g_debug ("applying default profile for device: %s", gcm_device_get_id (device));
		ret = gcm_device_apply (device, &error);
		if (!ret) {
			retval = 1;
			g_warning ("failed to set gamma: %s", error->message);
			g_error_free (error);
			break;
		}
	}
out:
	if (array != NULL)
		g_ptr_array_unref (array);
	if (client != NULL)
		g_object_unref (client);
	return retval;
}


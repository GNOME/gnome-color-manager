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
#include <locale.h>
#include <colord.h>

#include "gcm-utils.h"
#include "gcm-debug.h"

/**
 * gcm_inspect_print_data_info:
 **/
static gboolean
gcm_inspect_print_data_info (const gchar *title, const guint8 *data, gsize length)
{
	CdIcc *icc = NULL;
	GError *error = NULL;
	gboolean ret;

	/* parse the data */
	icc = cd_icc_new ();
	ret = cd_icc_load_data (icc, data, length,
				CD_ICC_LOAD_FLAGS_NONE,
				&error);
	if (!ret) {
		g_warning ("failed to parse data: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* print title */
	g_print ("%s\n", title);

	/* TRANSLATORS: this is the ICC profile description stored in an atom in the XServer */
	g_print (" - %s %s\n", _("Description:"), cd_icc_get_description (icc, NULL, NULL));

	/* TRANSLATORS: this is the ICC profile copyright */
	g_print (" - %s %s\n", _("Copyright:"), cd_icc_get_copyright (icc, NULL, NULL));
out:
	if (icc != NULL)
		g_object_unref (icc);
	return ret;
}

static gboolean
gcm_inspect_get_screen_protocol_version (GdkWindow *gdk_window,
					 guint *major,
					 guint *minor,
					 GError **error)
{
	gboolean ret;
	gint length;
	gint rc;
	guchar *data_tmp = NULL;

	/* get the value */
	gdk_error_trap_push ();
	ret = gdk_property_get (gdk_window,
				gdk_atom_intern_static_string ("_ICC_PROFILE_IN_X_VERSION"),
				gdk_atom_intern_static_string ("CARDINAL"),
				0,
				G_MAXLONG,
				FALSE,
				NULL,
				NULL,
				&length,
				&data_tmp);
	if (!ret) {
		g_set_error_literal (error, 1, 0, "failed to get property");
		goto out;
	}
	rc = gdk_error_trap_pop ();
	if (rc != 0) {
		ret = FALSE;
		g_set_error (error, 1, 0, "failed to get atom: %i", rc);
		goto out;
	}

	/* was nothing found */
	if (length == 0) {
		ret = FALSE;
		g_set_error (error, 1, 0, "icc profile atom has not been set");
		goto out;
	}

	/* set total */
	*major = (guint) data_tmp[0] / 100;
	*minor = (guint) data_tmp[0] % 100;

	/* success */
	ret = TRUE;
out:
	if (data_tmp != NULL)
		g_free (data_tmp);
	return ret;
}

static gboolean
gcm_inspect_get_screen_profile_data (GdkWindow *gdk_window,
				     guint8 **data,
				     gsize *length,
				     GError **error)
{
	gboolean ret;
	gint rc;
	gint length_tmp;

	/* get the value */
	gdk_error_trap_push ();
	ret = gdk_property_get (gdk_window,
				gdk_atom_intern_static_string ("_ICC_PROFILE"),
				gdk_atom_intern_static_string ("CARDINAL"),
				0,
				G_MAXLONG,
				FALSE,
				NULL,
				NULL,
				&length_tmp,
				(guchar **) data);
	if (!ret) {
		g_set_error_literal (error, 1, 0, "failed to get property");
		goto out;
	}
	rc = gdk_error_trap_pop ();
	if (rc != 0) {
		ret = FALSE;
		g_set_error (error, 1, 0, "failed to get icc profile atom: %i", rc);
		goto out;
	}

	/* was nothing found */
	if (length_tmp == 0) {
		ret = FALSE;
		g_set_error (error, 1, 0, "icc profile atom has not been set");
		goto out;
	}

	/* proxy size */
	*length = length_tmp;

	/* success */
	ret = TRUE;
out:
	return ret;
}

/**
 * gcm_inspect_show_x11_atoms:
 **/
static gboolean
gcm_inspect_show_x11_atoms (void)
{
	gboolean ret;
	guint8 *data = NULL;
	gsize length = 0;
	GError *error = NULL;
	guint major = -1;
	guint minor = -1;
	GdkWindow *gdk_window;

	/* setup object to access X */
	gdk_window = gdk_screen_get_root_window (gdk_screen_get_default ());

	/* get profile from XServer */
	ret = gcm_inspect_get_screen_profile_data (gdk_window, &data, &length, &error);
	if (!ret) {
		g_warning ("failed to get XServer profile data: %s", error->message);
		g_error_free (error);
		/* non-fatal */
		error = NULL;
	} else {
		/* TRANSLATORS: the root window of all the screens */
		gcm_inspect_print_data_info (_("Root window profile:"), data, length);
	}

	/* get profile from XServer */
	ret = gcm_inspect_get_screen_protocol_version (gdk_window, &major, &minor, &error);
	if (!ret) {
		g_warning ("failed to get XServer protocol version: %s", error->message);
		g_error_free (error);
		/* non-fatal */
		error = NULL;
	} else {
		/* TRANSLATORS: the root window of all the screens */
		g_print ("%s %i.%i\n", _("Root window protocol version:"), major, minor);
	}
	g_free (data);
	return ret;
}

/**
 * gcm_inspect_show_profiles_for_file:
 **/
static gboolean
gcm_inspect_show_profiles_for_file (const gchar *filename)
{
	gboolean ret = FALSE;
	const gchar *description;
	guint i = 0;
	GDBusConnection *connection;
	GError *error = NULL;
	GVariant *args = NULL;
	GVariant *response = NULL;
	GVariantIter *iter = NULL;

	/* get a session bus connection */
	connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
	if (connection == NULL) {
		/* TRANSLATORS: no DBus session bus */
		g_print ("%s %s\n", _("Failed to connect to session bus:"), error->message);
		g_error_free (error);
		goto out;
	}

	/* execute sync method */
	args = g_variant_new ("(ss)", filename, ""),
	response = g_dbus_connection_call_sync (connection,
						GCM_DBUS_SERVICE,
						GCM_DBUS_PATH,
						GCM_DBUS_INTERFACE,
						"GetProfilesForFile",
						args,
						G_VARIANT_TYPE ("(a(ss))"),
						G_DBUS_CALL_FLAGS_NONE,
						-1, NULL, &error);
	if (response == NULL) {
		/* TRANSLATORS: the DBus method failed */
		g_print ("%s %s\n", _("The request failed:"), error->message);
		g_error_free (error);
		goto out;
	}

	/* unpack the array */
	g_variant_get (response, "(a(ss))", &iter);
	if (g_variant_iter_n_children (iter) == 0) {
		/* TRANSLATORS: no profile has been asigned to this device */
		g_print ("%s\n", _("There are no ICC profiles assigned to this file"));
		goto out;
	}

	/* TRANSLATORS: this is a list of profiles suitable for the device */
	g_print ("%s %s\n", _("Suitable profiles for:"), filename);

	/* for each entry in the array */
	while (g_variant_iter_loop (iter, "(ss)", &filename, &description))
		g_print ("%i.\t%s\n\t%s\n", ++i, description, filename);

	/* success */
	ret = TRUE;
out:
	if (iter != NULL)
		g_variant_iter_free (iter);
	if (args != NULL)
		g_variant_unref (args);
	if (response != NULL)
		g_variant_unref (response);
	return ret;
}

/**
 * gcm_inspect_show_profile_for_window:
 **/
static gboolean
gcm_inspect_show_profile_for_window (guint xid)
{
	gboolean ret = FALSE;
	GDBusConnection *connection;
	GError *error = NULL;
	const gchar *profile;
	GVariant *args = NULL;
	GVariant *response = NULL;
	GVariant *response_child = NULL;
	GVariantIter *iter = NULL;

	/* get a session bus connection */
	connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
	if (connection == NULL) {
		/* TRANSLATORS: no DBus session bus */
		g_print ("%s %s\n", _("Failed to connect to session bus:"), error->message);
		g_error_free (error);
		goto out;
	}

	/* execute sync method */
	args = g_variant_new ("(u)", xid),
	response = g_dbus_connection_call_sync (connection,
						GCM_DBUS_SERVICE,
						GCM_DBUS_PATH,
						GCM_DBUS_INTERFACE,
						"GetProfileForWindow",
						args,
						G_VARIANT_TYPE ("(s)"),
						G_DBUS_CALL_FLAGS_NONE,
						-1, NULL, &error);
	if (response == NULL) {
		/* TRANSLATORS: the DBus method failed */
		g_print ("%s %s\n", _("The request failed:"), error->message);
		g_error_free (error);
		goto out;
	}

	/* print each device */
	response_child = g_variant_get_child_value (response, 0);
	profile = g_variant_get_string (response_child, NULL);

	/* no data */
	if (profile == NULL) {
		/* TRANSLATORS: no profile has been asigned to this window */
		g_print ("%s\n", _("There are no ICC profiles for this window"));
		goto out;
	}

	/* TRANSLATORS: this is a list of profiles suitable for the device */
	g_print ("%s %i\n", _("Suitable profiles for:"), xid);
	g_print ("1.\t%s\n\t%s\n", "this is a title", profile);

	/* success */
	ret = TRUE;
out:
	if (iter != NULL)
		g_variant_iter_free (iter);
	if (args != NULL)
		g_variant_unref (args);
	if (response != NULL)
		g_variant_unref (response);
	return ret;
}

/**
 * main:
 **/
int
main (int argc, char **argv)
{
	gboolean x11 = FALSE;
	gboolean dump = FALSE;
	guint xid = 0;
	gchar *filename = NULL;
	guint retval = 0;
	GOptionContext *context;

	const GOptionEntry options[] = {
		{ "xserver", 'x', 0, G_OPTION_ARG_NONE, &x11,
			/* TRANSLATORS: command line option */
			_("Show xserver properties"), NULL },
		{ "file", '\0', 0, G_OPTION_ARG_STRING, &filename,
			/* TRANSLATORS: command line option */
			_("Get the profiles for a specific file"), NULL },
		{ "xid", '\0', 0, G_OPTION_ARG_INT, &xid,
			/* TRANSLATORS: command line option */
			_("Get the profile for a specific window"), NULL },
		{ "dump", 'd', 0, G_OPTION_ARG_NONE, &dump,
			/* TRANSLATORS: command line option */
			_("Dump all details about this system"), NULL },
		{ NULL}
	};

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	gtk_init (&argc, &argv);

	/* TRANSLATORS: just dumps the EDID to disk */
	context = g_option_context_new (_("EDID inspect program"));
	g_option_context_add_main_entries (context, options, NULL);
	g_option_context_add_group (context, gcm_debug_get_option_group ());
	g_option_context_add_group (context, gtk_get_option_group (TRUE));
	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);

	if (x11 || dump)
		gcm_inspect_show_x11_atoms ();
	if (filename != NULL)
		gcm_inspect_show_profiles_for_file (filename);
	if (xid != 0)
		gcm_inspect_show_profile_for_window (xid);

	g_free (filename);
	return retval;
}


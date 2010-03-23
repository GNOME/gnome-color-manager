/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009 Richard Hughes <richard@hughsie.com>
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
#include <libgnomeui/gnome-rr.h>
#include <dbus/dbus-glib.h>
#include <locale.h>

#include "egg-debug.h"

#include "gcm-utils.h"
#include "gcm-profile.h"
#include "gcm-xserver.h"
#include "gcm-screen.h"

/**
 * gcm_inspect_print_data_info:
 **/
static gboolean
gcm_inspect_print_data_info (const gchar *title, const guint8 *data, gsize length)
{
	gchar *description = NULL;
	gchar *copyright = NULL;
	GcmProfile *profile = NULL;
	GError *error = NULL;
	gboolean ret;

	/* parse the data */
	profile = gcm_profile_default_new ();
	ret = gcm_profile_parse_data (profile, data, length, &error);
	if (!ret) {
		egg_warning ("failed to parse data: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* print some interesting facts about the profile */
	g_object_get (profile,
		      "description", &description,
		      "copyright", &copyright,
		      NULL);

	/* print title */
	g_print ("%s\n", title);

	/* TRANSLATORS: this is the ICC profile description stored in an atom in the XServer */
	g_print (" - %s %s\n", _("Description:"), description);

	/* TRANSLATORS: this is the ICC profile copyright */
	g_print (" - %s %s\n", _("Copyright:"), copyright);
out:
	g_free (copyright);
	g_free (description);
	if (profile != NULL)
		g_object_unref (profile);
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
	guint8 *data_tmp;
	gsize length;
	GcmXserver *xserver = NULL;
	GnomeRROutput **outputs;
	guint i;
	GcmScreen *screen = NULL;
	const gchar *output_name;
	gchar *title;
	GError *error = NULL;
	guint major;
	guint minor;

	/* setup object to access X */
	xserver = gcm_xserver_new ();

	/* get profile from XServer */
	ret = gcm_xserver_get_root_window_profile_data (xserver, &data, &length, &error);
	if (!ret) {
		egg_warning ("failed to get XServer profile data: %s", error->message);
		g_error_free (error);
		/* non-fatal */
		error = NULL;
	} else {
		/* TRANSLATORS: the root window of all the screens */
		gcm_inspect_print_data_info (_("Root window profile:"), data, length);
	}

	/* get profile from XServer */
	ret = gcm_xserver_get_protocol_version (xserver, &major, &minor, &error);
	if (!ret) {
		egg_warning ("failed to get XServer protocol version: %s", error->message);
		g_error_free (error);
		/* non-fatal */
		error = NULL;
	} else {
		/* TRANSLATORS: the root window of all the screens */
		g_print ("%s %i.%i\n", _("Root window protocol version:"), major, minor);
	}

	/* coldplug devices */
	screen = gcm_screen_new ();
	outputs = gcm_screen_get_outputs (screen, &error);
	if (outputs == NULL) {
		ret = FALSE;
		egg_warning ("failed to get outputs: %s", error->message);
		g_error_free (error);
		goto out;
	}
	for (i=0; outputs[i] != NULL; i++) {

		/* get output name */
		output_name = gnome_rr_output_get_name (outputs[i]);
		title = g_strdup_printf (_("Output profile '%s':"), output_name);

		/* get profile from XServer */
		ret = gcm_xserver_get_output_profile_data (xserver, output_name, &data_tmp, &length, &error);
		if (!ret) {
			egg_warning ("failed to get output profile data: %s", error->message);
			/* TRANSLATORS: this is when the profile has not been set */
			g_print ("%s %s\n", title, _("not set"));
			g_error_free (error);
			/* non-fatal */
			error = NULL;
		} else {
			/* TRANSLATORS: the output, i.e. the flat panel */
			gcm_inspect_print_data_info (title, data_tmp, length);
			g_free (data_tmp);
		}
		g_free (title);
	}
out:
	g_free (data);
	if (screen != NULL)
		g_object_unref (screen);
	if (xserver != NULL)
		g_object_unref (xserver);
	return ret;
}

/**
 * gcm_inspect_show_profiles_for_device:
 **/
static gboolean
gcm_inspect_show_profiles_for_device (const gchar *device_id)
{
	gboolean ret;
	DBusGConnection *connection;
	DBusGProxy *proxy;
	GError *error = NULL;
	gchar *title;
	gchar *profile;
	guint i;
	GType custom_g_type_string_string;
	GPtrArray *profile_data_array = NULL;
	GValueArray *gva;
	GValue *gv;

	/* get a session bus connection */
	connection = dbus_g_bus_get (DBUS_BUS_SESSION, NULL);

	/* connect to the interface */
	proxy = dbus_g_proxy_new_for_name (connection,
					   "org.gnome.ColorManager",
					   "/org/gnome/ColorManager",
					   "org.gnome.ColorManager");

	/* create a specialized type, because dbus-glib sucks monkey balls */
	custom_g_type_string_string = dbus_g_type_get_collection ("GPtrArray",
					dbus_g_type_get_struct("GValueArray",
						G_TYPE_STRING,
						G_TYPE_STRING,
						G_TYPE_INVALID));

	/* execute sync method */
	ret = dbus_g_proxy_call (proxy, "GetProfilesForDevice", &error,
				 G_TYPE_STRING, device_id,
				 G_TYPE_STRING, "",
				 G_TYPE_INVALID,
				 custom_g_type_string_string, &profile_data_array,
				 G_TYPE_INVALID);
	if (!ret) {
		/* TRANSLATORS: the DBus method failed */
		g_print ("%s: %s\n", _("The request failed"), error->message);
		g_error_free (error);
		goto out;
	}

	/* no data */
	if (profile_data_array->len == 0) {
		/* TRANSLATORS: no rofile has been asigned to this device */
		g_print ("%s\n", _("There are no ICC profiles for this device"));
		goto out;
	}

	/* TRANSLATORS: this is a list of profiles suitable for the device */
	g_print ("%s %s\n", _("Suitable profiles for:"), device_id);

	/* list each entry */
	for (i=0; i<profile_data_array->len; i++) {
		gva = (GValueArray *) g_ptr_array_index (profile_data_array, i);
		/* 0 */
		gv = g_value_array_get_nth (gva, 0);
		title = g_value_dup_string (gv);
		g_value_unset (gv);
		/* 1 */
		gv = g_value_array_get_nth (gva, 1);
		profile = g_value_dup_string (gv);
		g_value_unset (gv);

		/* done */
		g_print ("%i.\t%s\n\t%s\n", i+1, title, profile);
		g_value_array_free (gva);
		g_free (title);
		g_free (profile);
	}
out:
	if (profile_data_array != NULL)
		g_ptr_array_free (profile_data_array, TRUE);
	g_object_unref (proxy);
	return ret;
}

/**
 * gcm_inspect_show_profiles_for_devices:
 **/
static gboolean
gcm_inspect_show_profiles_for_devices (void)
{
	gboolean ret;
	DBusGConnection *connection;
	DBusGProxy *proxy;
	GError *error = NULL;
	guint i;
	gchar **devices = NULL;

	/* get a session bus connection */
	connection = dbus_g_bus_get (DBUS_BUS_SESSION, NULL);

	/* connect to the interface */
	proxy = dbus_g_proxy_new_for_name (connection,
					   "org.gnome.ColorManager",
					   "/org/gnome/ColorManager",
					   "org.gnome.ColorManager");

	/* execute sync method */
	ret = dbus_g_proxy_call (proxy, "GetDevices", &error,
				 G_TYPE_INVALID,
				 G_TYPE_STRV, &devices,
				 G_TYPE_INVALID);
	if (!ret) {
		/* TRANSLATORS: the DBus method failed */
		g_print ("%s: %s\n", _("The request failed"), error->message);
		g_error_free (error);
		goto out;
	}

	/* print each device */
	for (i=0; devices[i] != NULL; i++) {
		ret = gcm_inspect_show_profiles_for_device (devices[i]);
		if (!ret)
			goto out;
	}
out:
	g_object_unref (proxy);
	g_strfreev (devices);
	return ret;
}

/**
 * gcm_inspect_show_profile_for_window:
 **/
static gboolean
gcm_inspect_show_profile_for_window (guint xid)
{
	gboolean ret;
	DBusGConnection *connection;
	DBusGProxy *proxy;
	GError *error = NULL;
	gchar *profile = NULL;

	/* get a session bus connection */
	connection = dbus_g_bus_get (DBUS_BUS_SESSION, NULL);

	/* connect to the interface */
	proxy = dbus_g_proxy_new_for_name (connection,
					   "org.gnome.ColorManager",
					   "/org/gnome/ColorManager",
					   "org.gnome.ColorManager");

	/* execute sync method */
	ret = dbus_g_proxy_call (proxy, "GetProfileForWindow", &error,
				 G_TYPE_UINT, xid,
				 G_TYPE_INVALID,
				 G_TYPE_STRING, &profile,
				 G_TYPE_INVALID);
	if (!ret) {
		/* TRANSLATORS: the DBus method failed */
		g_print ("%s: %s\n", _("The request failed"), error->message);
		g_error_free (error);
		goto out;
	}

	/* no data */
	if (profile == NULL) {
		/* TRANSLATORS: no profile has been asigned to this window */
		g_print ("%s\n", _("There are no ICC profiles for this window"));
		goto out;
	}

	/* TRANSLATORS: this is a list of profiles suitable for the device */
	g_print ("%s %i\n", _("Suitable profiles for:"), xid);
	g_print ("1.\t%s\n\t%s\n", "this is a title", profile);
out:
	g_object_unref (proxy);
	g_free (profile);
	return ret;
}

/**
 * gcm_inspect_show_profiles_for_type:
 **/
static gboolean
gcm_inspect_show_profiles_for_type (const gchar *type)
{
	gboolean ret;
	DBusGConnection *connection;
	DBusGProxy *proxy;
	GError *error = NULL;
	gchar *title;
	gchar *profile;
	guint i;
	GType custom_g_type_string_string;
	GPtrArray *profile_data_array = NULL;
	GValueArray *gva;
	GValue *gv;

	/* get a session bus connection */
	connection = dbus_g_bus_get (DBUS_BUS_SESSION, NULL);

	/* connect to the interface */
	proxy = dbus_g_proxy_new_for_name (connection,
					   "org.gnome.ColorManager",
					   "/org/gnome/ColorManager",
					   "org.gnome.ColorManager");

	/* create a specialized type, because dbus-glib sucks monkey balls */
	custom_g_type_string_string = dbus_g_type_get_collection ("GPtrArray",
					dbus_g_type_get_struct("GValueArray",
						G_TYPE_STRING,
						G_TYPE_STRING,
						G_TYPE_INVALID));

	/* execute sync method */
	ret = dbus_g_proxy_call (proxy, "GetProfilesForType", &error,
				 G_TYPE_STRING, type,
				 G_TYPE_STRING, "",
				 G_TYPE_INVALID,
				 custom_g_type_string_string, &profile_data_array,
				 G_TYPE_INVALID);
	if (!ret) {
		/* TRANSLATORS: the DBus method failed */
		g_print ("%s: %s\n", _("The request failed"), error->message);
		g_error_free (error);
		goto out;
	}

	/* no data */
	if (profile_data_array->len == 0) {
		/* TRANSLATORS: no rofile has been asigned to this device type */
		g_print ("%s\n", _("There are no ICC profiles for this device type"));
		goto out;
	}

	/* TRANSLATORS: this is a list of profiles suitable for the device */
	g_print ("%s %s\n", _("Suitable profiles for:"), type);

	/* list each entry */
	for (i=0; i<profile_data_array->len; i++) {
		gva = (GValueArray *) g_ptr_array_index (profile_data_array, i);
		/* 0 */
		gv = g_value_array_get_nth (gva, 0);
		title = g_value_dup_string (gv);
		g_value_unset (gv);
		/* 1 */
		gv = g_value_array_get_nth (gva, 1);
		profile = g_value_dup_string (gv);
		g_value_unset (gv);

		/* done */
		g_print ("%i.\t%s\n\t%s\n", i+1, title, profile);
		g_value_array_free (gva);
		g_free (title);
		g_free (profile);
	}
out:
	if (profile_data_array != NULL)
		g_ptr_array_free (profile_data_array, TRUE);
	g_object_unref (proxy);
	return ret;
}

/**
 * gcm_inspect_get_properties_collect_cb:
 **/
static void
gcm_inspect_get_properties_collect_cb (const char *key, const GValue *value, gpointer user_data)
{
	if (g_strcmp0 (key, "RenderingIntentDisplay") == 0) {
		/* TRANSLATORS: this is the rendering intent of the output */
		g_print ("%s %s\n", _("Rendering intent (display):"), g_value_get_string (value));
	} else if (g_strcmp0 (key, "RenderingIntentSoftproof") == 0) {
		/* TRANSLATORS: this is the rendering intent of the printer */
		g_print ("%s %s\n", _("Rendering intent (softproof):"), g_value_get_string (value));
	} else if (g_strcmp0 (key, "ColorspaceRgb") == 0) {
		/* TRANSLATORS: this is filename of the default colorspace */
		g_print ("%s %s\n", _("RGB Colorspace:"), g_value_get_string (value));
	} else if (g_strcmp0 (key, "ColorspaceCmyk") == 0) {
		/* TRANSLATORS: this is filename of the default colorspace */
		g_print ("%s %s\n", _("CMYK Colorspace:"), g_value_get_string (value));
	} else {
		egg_warning ("unhandled property '%s'", key);
	}
}

/**
 * gcm_inspect_get_properties:
 **/
static gboolean
gcm_inspect_get_properties (void)
{
	gboolean ret;
	DBusGConnection *connection;
	DBusGProxy *proxy;
	GError *error = NULL;
	gchar **profiles = NULL;
	GHashTable *hash = NULL;

	/* get a session bus connection */
	connection = dbus_g_bus_get (DBUS_BUS_SESSION, NULL);

	/* connect to the interface */
	proxy = dbus_g_proxy_new_for_name (connection,
					   "org.gnome.ColorManager",
					   "/org/gnome/ColorManager",
					   "org.freedesktop.DBus.Properties");

	/* execute sync method */
	ret = dbus_g_proxy_call (proxy, "GetAll", &error,
				 G_TYPE_STRING, "org.gnome.ColorManager",
				 G_TYPE_INVALID,
				 dbus_g_type_get_map ("GHashTable", G_TYPE_STRING, G_TYPE_VALUE), &hash,
				 G_TYPE_INVALID);
	if (!ret) {
		/* TRANSLATORS: the DBus method failed */
		g_print ("%s: %s\n", _("The request failed"), error->message);
		g_error_free (error);
		goto out;
	}

	/* process results */
	if (hash != NULL)
		g_hash_table_foreach (hash, (GHFunc) gcm_inspect_get_properties_collect_cb, NULL);
out:
	if (hash != NULL)
		g_hash_table_unref (hash);
	g_object_unref (proxy);
	g_strfreev (profiles);
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
	gchar *device_id = NULL;
	gchar *type = NULL;
	GcmDeviceTypeEnum type_enum;
	guint retval = 0;
	GOptionContext *context;

	const GOptionEntry options[] = {
		{ "x11", 'x', 0, G_OPTION_ARG_NONE, &x11,
			/* TRANSLATORS: command line option */
			_("Show X11 properties"), NULL },
		{ "device", '\0', 0, G_OPTION_ARG_STRING, &device_id,
			/* TRANSLATORS: command line option */
			_("Get the profiles for a specific device"), NULL },
		{ "xid", '\0', 0, G_OPTION_ARG_INT, &xid,
			/* TRANSLATORS: command line option */
			_("Get the profile for a specific window"), NULL },
		{ "type", '\0', 0, G_OPTION_ARG_STRING, &type,
			/* TRANSLATORS: command line option */
			_("Get the profiles for a specific device type"), NULL },
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
	g_option_context_add_group (context, egg_debug_get_option_group ());
	g_option_context_add_group (context, gtk_get_option_group (TRUE));
	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);

	if (x11 || dump)
		gcm_inspect_show_x11_atoms ();
	if (device_id != NULL)
		gcm_inspect_show_profiles_for_device (device_id);
	if (xid != 0)
		gcm_inspect_show_profile_for_window (xid);
	if (type != NULL) {
		type_enum = gcm_device_type_enum_from_string (type);
		if (type_enum == GCM_DEVICE_TYPE_ENUM_UNKNOWN) {
			/* TRANSLATORS: this is when the user does --type=mickeymouse */
			g_print ("%s\n", _("Device type not recognized"));
		} else {
			/* show device profiles */
			gcm_inspect_show_profiles_for_type (type);
		}
	}
	if (dump) {
		gcm_inspect_get_properties ();
		gcm_inspect_show_profiles_for_devices ();
	}

	g_free (device_id);
	g_free (type);
	return retval;
}


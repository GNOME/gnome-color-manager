/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007-2011 Richard Hughes <richard@hughsie.com>
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
#include <gdk/gdkx.h>
#include <locale.h>
#include <colord.h>
#include <lcms2.h>

#include "gcm-debug.h"
#include "gcm-exif.h"
#include "gcm-utils.h"

typedef struct {
	CdClient	*client;
	GDBusConnection	*connection;
	GDBusNodeInfo	*introspection;
	GMainLoop	*loop;
} GcmSessionPrivate;

/**
 * gcm_session_variant_from_profile_array:
 **/
static GVariant *
gcm_session_variant_from_profile_array (GPtrArray *array)
{
	guint i;
	CdProfile *profile;
	GVariantBuilder *builder;
	GVariant *value;

	/* create builder */
	builder = g_variant_builder_new (G_VARIANT_TYPE("a(ss)"));

	/* add each tuple to the array */
	for (i=0; i<array->len; i++) {
		profile = g_ptr_array_index (array, i);
		g_variant_builder_add (builder, "(ss)",
				       cd_profile_get_filename (profile),
				       cd_profile_get_title (profile));
	}

	value = g_variant_builder_end (builder);
	g_variant_builder_unref (builder);
	return value;
}

/**
 * gcm_session_get_profiles_for_file:
 **/
static GPtrArray *
gcm_session_get_profiles_for_file (GcmSessionPrivate *priv,
				   const gchar *filename,
				   GError **error)
{
	guint i;
	gboolean ret;
	GcmExif *exif;
	CdDevice *device;
	GPtrArray *array = NULL;
	GPtrArray *array_devices = NULL;
	GFile *file;

	/* get file type */
	exif = gcm_exif_new ();
	file = g_file_new_for_path (filename);
	ret = gcm_exif_parse (exif, file, error);
	if (!ret)
		goto out;

	/* get list */
	g_debug ("query=%s", filename);
	array_devices = cd_client_get_devices_sync (priv->client,
						    NULL,
						    error);
	if (array_devices == NULL)
		goto out;
	for (i=0; i<array_devices->len; i++) {
		device = g_ptr_array_index (array_devices, i);

		/* match up critical parts */
		if (g_strcmp0 (cd_device_get_vendor (device), gcm_exif_get_manufacturer (exif)) == 0 &&
		    g_strcmp0 (cd_device_get_model (device), gcm_exif_get_model (exif)) == 0 &&
		    g_strcmp0 (cd_device_get_serial (device), gcm_exif_get_serial (exif)) == 0) {
			array = cd_device_get_profiles (device);
			break;
		}
	}

	/* nothing found, so set error */
	if (array == NULL)
		g_set_error_literal (error, 1, 0, "No profiles were found.");

	/* unref list of devices */
	g_ptr_array_unref (array_devices);
out:
	g_object_unref (file);
	g_object_unref (exif);
	return array;
}

/**
 * gcm_session_get_profiles_for_device:
 **/
static GPtrArray *
gcm_session_get_profiles_for_device (GcmSessionPrivate *priv,
				     const gchar *device_id_with_prefix,
				     GError **error)
{
	CdDevice *device;
	gchar *device_id;
	GPtrArray *profiles = NULL;

	/* reformat to the device-and-profile-naming-spec */
	device_id = g_strdup (device_id_with_prefix);
	if (g_str_has_prefix (device_id_with_prefix, "sane:"))
		device_id[4] = '-';

	/* get list */
	g_debug ("query=%s", device_id);
	device = cd_client_find_device_sync (priv->client,
					     device_id,
					     NULL,
					     error);
	if (device == NULL)
		goto out;
	profiles = cd_device_get_profiles (device);
	if (profiles->len == 0) {
		g_set_error_literal (error, 1, 0, "No profiles were found.");
		goto out;
	}
out:
	g_free (device_id);
	if (device != NULL)
		g_object_unref (device);
	return profiles;
}

/**
 * gcm_session_handle_method_call:
 **/
static void
gcm_session_handle_method_call (GDBusConnection *connection_, const gchar *sender,
				const gchar *object_path, const gchar *interface_name,
				const gchar *method_name, GVariant *parameters,
				GDBusMethodInvocation *invocation, gpointer user_data)
{
	GVariant *tuple = NULL;
	GVariant *value = NULL;
	gchar *device_id = NULL;
	gchar *filename = NULL;
	gchar *hints = NULL;
	gchar *type = NULL;
	GPtrArray *array = NULL;
	gchar **devices = NULL;
	GError *error = NULL;
	GcmSessionPrivate *priv = (GcmSessionPrivate *) user_data;

	/* return 'a(ss)' */
	if (g_strcmp0 (method_name, "GetProfilesForDevice") == 0) {
		g_variant_get (parameters, "(ss)", &device_id, &hints);

		/* get array of profile filenames */
		array = gcm_session_get_profiles_for_device (priv,
							     device_id,
							     &error);
		if (array == NULL) {
			g_dbus_method_invocation_return_dbus_error (invocation,
								    "org.gnome.ColorManager.Failed",
								    error->message);
			g_error_free (error);
			goto out;
		}

		/* format the value */
		value = gcm_session_variant_from_profile_array (array);
		tuple = g_variant_new_tuple (&value, 1);
		g_dbus_method_invocation_return_value (invocation, tuple);
		goto out;
	}

	/* return 'a(ss)' */
	if (g_strcmp0 (method_name, "GetProfilesForFile") == 0) {
		g_variant_get (parameters, "(ss)", &filename, &hints);

		/* get array of profile filenames */
		array = gcm_session_get_profiles_for_file (priv,
							   filename,
							   &error);
		if (array == NULL) {
			g_dbus_method_invocation_return_dbus_error (invocation,
								    "org.gnome.ColorManager.Failed",
								    error->message);
			g_error_free (error);
			goto out;
		}

		/* format the value */
		value = gcm_session_variant_from_profile_array (array);
		tuple = g_variant_new_tuple (&value, 1);
		g_dbus_method_invocation_return_value (invocation, tuple);
		goto out;
	}
out:
	if (array != NULL)
		g_ptr_array_unref (array);
	if (tuple != NULL)
		g_variant_unref (tuple);
	if (value != NULL)
		g_variant_unref (value);
	g_free (device_id);
	g_free (type);
	g_free (filename);
	g_free (hints);
	g_strfreev (devices);
	return;
}

/**
 * gcm_session_on_bus_acquired:
 **/
static void
gcm_session_on_bus_acquired (GDBusConnection *connection,
			     const gchar *name,
			     gpointer user_data)
{
	guint registration_id;
	GcmSessionPrivate *priv = (GcmSessionPrivate *) user_data;
	static const GDBusInterfaceVTable interface_vtable = {
		gcm_session_handle_method_call,
		NULL,
		NULL
	};

	registration_id = g_dbus_connection_register_object (connection,
							     GCM_DBUS_PATH,
							     priv->introspection->interfaces[0],
							     &interface_vtable,
							     priv,  /* user_data */
							     NULL,  /* user_data_free_func */
							     NULL); /* GError** */
	g_assert (registration_id > 0);
}

/**
 * gcm_session_on_name_acquired:
 **/
static void
gcm_session_on_name_acquired (GDBusConnection *connection,
			      const gchar *name,
			      gpointer user_data)
{
	GcmSessionPrivate *priv = (GcmSessionPrivate *) user_data;
	g_debug ("acquired name: %s", name);
	priv->connection = g_object_ref (connection);
}

/**
 * gcm_session_on_name_lost:
 **/
static void
gcm_session_on_name_lost (GDBusConnection *connection,
			  const gchar *name,
			  gpointer user_data)
{
	GcmSessionPrivate *priv = (GcmSessionPrivate *) user_data;
	g_debug ("lost name: %s", name);
	g_main_loop_quit (priv->loop);
}

/**
 * gcm_session_client_connect_cb:
 **/
static void
gcm_session_client_connect_cb (GObject *source_object,
			       GAsyncResult *res,
			       gpointer user_data)
{
	gboolean ret;
	GError *error = NULL;
	GcmSessionPrivate *priv = (GcmSessionPrivate *) user_data;

	/* connected */
	g_debug ("connected to colord");
	ret = cd_client_connect_finish (priv->client, res, &error);
	if (!ret) {
		g_warning ("failed to connect to colord: %s", error->message);
		g_error_free (error);
		goto out;
	}
out:
	return;
}

/**
 * gcm_session_load_introspection:
 **/
static gboolean
gcm_session_load_introspection (GcmSessionPrivate *priv,
				GError **error)
{
	gboolean ret;
	gchar *introspection_data = NULL;
	GFile *file = NULL;

	/* load introspection from file */
	file = g_file_new_for_path (DATADIR "/dbus-1/interfaces/org.gnome.ColorManager.xml");
	ret = g_file_load_contents (file, NULL, &introspection_data,
				    NULL, NULL, error);
	if (!ret)
		goto out;

	/* build introspection from XML */
	priv->introspection = g_dbus_node_info_new_for_xml (introspection_data,
							    error);
	if (priv->introspection == NULL) {
		ret = FALSE;
		goto out;
	}
out:
	if (file != NULL)
		g_object_unref (file);
	g_free (introspection_data);
	return ret;
}

/**
 * main:
 **/
int
main (int argc, char *argv[])
{
	gboolean login = FALSE;
	gboolean ret;
	GError *error = NULL;
	GOptionContext *context;
	guint owner_id = 0;
	guint poll_id = 0;
	guint retval = 1;
	GcmSessionPrivate *priv;

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

	if (! g_thread_supported ())
		g_thread_init (NULL);
	g_type_init ();

	/* TRANSLATORS: program name, a session wide daemon to watch for updates and changing system state */
	g_set_application_name (_("Color Management"));
	context = g_option_context_new (NULL);
	g_option_context_set_summary (context, _("Color Management D-Bus Service"));
	g_option_context_add_main_entries (context, options, NULL);
	g_option_context_add_group (context, gcm_debug_get_option_group ());
	g_option_context_add_group (context, gtk_get_option_group (TRUE));
	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);

	gtk_init (&argc, &argv);

	priv = g_new0 (GcmSessionPrivate, 1);

	/* monitor daemon */
	priv->client = cd_client_new ();
	cd_client_connect (priv->client,
			   NULL,
			   gcm_session_client_connect_cb,
			   priv);

	/* create new objects */
	g_debug ("spinning loop");
	priv->loop = g_main_loop_new (NULL, FALSE);

	/* load introspection from file */
	ret = gcm_session_load_introspection (priv, &error);
	if (!ret) {
		g_warning ("failed to load introspection: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* own the object */
	owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
				   GCM_DBUS_SERVICE,
				   G_BUS_NAME_OWNER_FLAGS_NONE,
				   gcm_session_on_bus_acquired,
				   gcm_session_on_name_acquired,
				   gcm_session_on_name_lost,
				   priv, NULL);

	/* wait */
	g_main_loop_run (priv->loop);

	/* success */
	retval = 0;
out:
	if (poll_id != 0)
		g_source_remove (poll_id);
	if (owner_id > 0)
		g_bus_unown_name (owner_id);
	if (priv != NULL) {
		if (priv->connection != NULL)
			g_object_unref (priv->connection);
		g_dbus_node_info_unref (priv->introspection);
		g_object_unref (priv->client);
		g_main_loop_unref (priv->loop);
		g_free (priv);
	}
	return retval;
}


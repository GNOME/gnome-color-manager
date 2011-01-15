/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007-2010 Richard Hughes <richard@hughsie.com>
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
#include <libnotify/notify.h>

#include "gcm-client.h"
#include "gcm-device-xrandr.h"
#include "gcm-exif.h"
#include "gcm-device.h"
#include "gcm-utils.h"
#include "gcm-client.h"
#include "gcm-profile-store.h"
#include "gcm-debug.h"

static GMainLoop *loop = NULL;
static GSettings *settings = NULL;
static GDBusNodeInfo *introspection = NULL;
static GcmClient *client = NULL;
static GcmProfileStore *profile_store = NULL;
static GTimer *timer = NULL;
static GDBusConnection *connection = NULL;

#define GCM_SESSION_IDLE_EXIT		60 /* seconds */
#define GCM_SESSION_NOTIFY_TIMEOUT	30000 /* ms */

/**
 * gcm_session_check_idle_cb:
 **/
static gboolean
gcm_session_check_idle_cb (gpointer user_data)
{
	guint idle;

	/* get the idle time */
	idle = (guint) g_timer_elapsed (timer, NULL);
	g_debug ("we've been idle for %is", idle);
	if (idle > GCM_SESSION_IDLE_EXIT) {
		g_debug ("exiting loop as idle");
		g_main_loop_quit (loop);
		return FALSE;
	}
	/* continue to poll */
	return TRUE;
}

/**
 * gcm_session_notify_cb:
 **/
static void
gcm_session_notify_cb (NotifyNotification *notification, gchar *action, gpointer user_data)
{
	gboolean ret;
	GError *error = NULL;

	if (g_strcmp0 (action, "display") == 0) {
		g_settings_set_int (settings, GCM_SETTINGS_RECALIBRATE_DISPLAY_THRESHOLD, 0);
	} else if (g_strcmp0 (action, "printer") == 0) {
		g_settings_set_int (settings, GCM_SETTINGS_RECALIBRATE_PRINTER_THRESHOLD, 0);
	} else if (g_strcmp0 (action, "recalibrate") == 0) {
		ret = g_spawn_command_line_async ("gcm-prefs", &error);
		if (!ret) {
			g_warning ("failed to spawn: %s", error->message);
			g_error_free (error);
		}
	}
}

/**
 * gcm_session_notify_recalibrate:
 **/
static gboolean
gcm_session_notify_recalibrate (const gchar *title, const gchar *message, GcmDeviceKind kind)
{
	gboolean ret;
	GError *error = NULL;
	NotifyNotification *notification;

	/* show a bubble */
	notification = notify_notification_new (title, message, GCM_STOCK_ICON);
	notify_notification_set_timeout (notification, GCM_SESSION_NOTIFY_TIMEOUT);
	notify_notification_set_urgency (notification, NOTIFY_URGENCY_LOW);

	/* TRANSLATORS: button: this is to open GCM */
	notify_notification_add_action (notification, "recalibrate", _("Recalibrate now"), gcm_session_notify_cb, NULL, NULL);

	/* TRANSLATORS: button: this is to ignore the recalibrate notifications */
	notify_notification_add_action (notification, gcm_device_kind_to_string (kind), _("Ignore"), gcm_session_notify_cb, NULL, NULL);

	ret = notify_notification_show (notification, &error);
	if (!ret) {
		g_warning ("failed to show notification: %s", error->message);
		g_error_free (error);
	}
	return ret;
}

/**
 * gcm_session_notify_device:
 **/
static void
gcm_session_notify_device (GcmDevice *device)
{
	gchar *message;
	const gchar *title;
	GcmDeviceKind kind;
	glong since;
	GTimeVal timeval;
	gint threshold;

	/* get current time */
	g_get_current_time (&timeval);

	/* TRANSLATORS: this is when the device has not been recalibrated in a while */
	title = _("Recalibration required");

	/* check we care */
	kind = gcm_device_get_kind (device);
	if (kind == GCM_DEVICE_KIND_DISPLAY) {

		/* get from GSettings */
		threshold = g_settings_get_int (settings, GCM_SETTINGS_RECALIBRATE_DISPLAY_THRESHOLD);

		/* TRANSLATORS: this is when the display has not been recalibrated in a while */
		message = g_strdup_printf (_("The display '%s' should be recalibrated soon."), gcm_device_get_title (device));
	} else {

		/* get from GSettings */
		threshold = g_settings_get_int (settings, GCM_SETTINGS_RECALIBRATE_DISPLAY_THRESHOLD);

		/* TRANSLATORS: this is when the printer has not been recalibrated in a while */
		message = g_strdup_printf (_("The printer '%s' should be recalibrated soon."), gcm_device_get_title (device));
	}

	/* check if we need to notify */
	since = timeval.tv_sec - gcm_device_get_modified_time (device);
	if (threshold > since)
		gcm_session_notify_recalibrate (title, message, kind);
	g_free (message);
}

/**
 * gcm_session_added_cb:
 **/
static void
gcm_session_added_cb (GcmClient *client_, GcmDevice *device, gpointer user_data)
{
	GcmDeviceKind kind;
	const gchar *profile;
	gchar *basename = NULL;
	gboolean allow_notifications;

	/* check we care */
	kind = gcm_device_get_kind (device);
	if (kind != GCM_DEVICE_KIND_DISPLAY &&
	    kind != GCM_DEVICE_KIND_PRINTER)
		return;

	/* ensure we have a profile */
	profile = gcm_device_get_default_profile_filename (device);
	if (profile == NULL) {
		g_debug ("no profile set for %s", gcm_device_get_id (device));
		goto out;
	}

	/* ensure it's a profile generated by us */
	basename = g_path_get_basename (profile);
	if (!g_str_has_prefix (basename, "GCM")) {
		g_debug ("not a GCM profile for %s: %s", gcm_device_get_id (device), profile);
		goto out;
	}

	/* do we allow notifications */
	allow_notifications = g_settings_get_boolean (settings, GCM_SETTINGS_SHOW_NOTIFICATIONS);
	if (!allow_notifications)
		goto out;

	/* handle device */
	gcm_session_notify_device (device);
out:
	g_free (basename);
}

/**
 * gcm_session_get_profile_for_window:
 **/
static const gchar *
gcm_session_get_profile_for_window (guint xid, GError **error)
{
	GcmDevice *device;
	GdkWindow *window;
	const gchar *filename = NULL;

	g_debug ("getting profile for %i", xid);

	/* get window for xid */
	window = gdk_x11_window_foreign_new_for_display (gdk_display_get_default (), xid);
	if (window == NULL) {
		g_set_error (error, 1, 0, "failed to find window with xid %i", xid);
		goto out;
	}

	/* get device for this window */
	device = gcm_client_get_device_by_window (client, window);
	if (device == NULL) {
		g_set_error (error, 1, 0, "no device found for xid %i", xid);
		goto out;
	}

	/* get the data */
	filename = gcm_device_get_default_profile_filename (device);
	if (filename == NULL) {
		g_set_error (error, 1, 0, "no profiles found for xid %i", xid);
		goto out;
	}
out:
	if (window != NULL)
		g_object_unref (window);
	return filename;
}

/**
 * gcm_session_get_profiles_for_profile_kind:
 **/
static GPtrArray *
gcm_session_get_profiles_for_profile_kind (GcmProfileKind kind, GError **error)
{
	guint i;
	GcmProfile *profile;
	GPtrArray *array;
	GPtrArray *profile_array;

	/* create a temp array */
	array = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);

	/* get list */
	profile_array = gcm_profile_store_get_array (profile_store);
	for (i=0; i<profile_array->len; i++) {
		profile = g_ptr_array_index (profile_array, i);

		/* compare what we have against what we were given */
		if (kind == gcm_profile_get_kind (profile))
			g_ptr_array_add (array, g_object_ref (profile));
	}

	/* unref profile list */
	g_ptr_array_unref (profile_array);
	return array;
}

/**
 * gcm_session_variant_from_profile_array:
 **/
static GVariant *
gcm_session_variant_from_profile_array (GPtrArray *array)
{
	guint i;
	GcmProfile *profile;
	GVariantBuilder *builder;
	GVariant *value;

	/* create builder */
	builder = g_variant_builder_new (G_VARIANT_TYPE("a(ss)"));

	/* add each tuple to the array */
	for (i=0; i<array->len; i++) {
		profile = g_ptr_array_index (array, i);
		g_variant_builder_add (builder, "(ss)",
				       gcm_profile_get_filename (profile),
				       gcm_profile_get_description (profile));
	}

	value = g_variant_builder_end (builder);
	g_variant_builder_unref (builder);
	return value;
}

/**
 * gcm_session_get_profiles_for_file:
 **/
static GPtrArray *
gcm_session_get_profiles_for_file (const gchar *filename, GError **error)
{
	guint i;
	gboolean ret;
	GcmExif *exif;
	GcmDevice *device;
	GPtrArray *array = NULL;
	GPtrArray *array_devices;
	GFile *file;

	/* get file type */
	exif = gcm_exif_new ();
	file = g_file_new_for_path (filename);
	ret = gcm_exif_parse (exif, file, error);
	if (!ret)
		goto out;

	/* get list */
	g_debug ("query=%s", filename);
	array_devices = gcm_client_get_devices (client);
	for (i=0; i<array_devices->len; i++) {
		device = g_ptr_array_index (array_devices, i);

		/* match up critical parts */
		if (g_strcmp0 (gcm_device_get_manufacturer (device), gcm_exif_get_manufacturer (exif)) == 0 &&
		    g_strcmp0 (gcm_device_get_model (device), gcm_exif_get_model (exif)) == 0 &&
		    g_strcmp0 (gcm_device_get_serial (device), gcm_exif_get_serial (exif)) == 0) {
			array = gcm_device_get_profiles (device);
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
gcm_session_get_profiles_for_device (const gchar *device_id_with_prefix, GError **error)
{
	const gchar *device_id;
	const gchar *device_id_tmp;
	guint i;
	gboolean use_native_device = FALSE;
	GcmDevice *device;
	GPtrArray *array = NULL;
	GPtrArray *array_devices;

	/* strip the prefix, if there is any */
	device_id = g_strstr_len (device_id_with_prefix, -1, ":");
	if (device_id == NULL) {
		device_id = device_id_with_prefix;
	} else {
		device_id++;
		use_native_device = TRUE;
	}

	/* use the sysfs path to be backwards compatible */
	if (g_str_has_prefix (device_id_with_prefix, "/"))
		use_native_device = TRUE;

	/* get list */
	g_debug ("query=%s [%s] %i", device_id_with_prefix, device_id, use_native_device);
	array_devices = gcm_client_get_devices (client);
	for (i=0; i<array_devices->len; i++) {
		device = g_ptr_array_index (array_devices, i);

		/* get the id for this device */
		if (use_native_device && GCM_IS_DEVICE_XRANDR (device)) {
			device_id_tmp = gcm_device_xrandr_get_native_device (GCM_DEVICE_XRANDR (device));
		} else {
			device_id_tmp = gcm_device_get_id (device);
		}

		/* wrong kind of device */
		if (device_id_tmp == NULL)
			continue;

		/* compare what we have against what we were given */
		g_debug ("comparing %s with %s", device_id_tmp, device_id);
		if (g_strcmp0 (device_id_tmp, device_id) == 0) {
			array = gcm_device_get_profiles (device);
			break;
		}
	}

	/* nothing found, so set error */
	if (array == NULL)
		g_set_error_literal (error, 1, 0, "No profiles were found.");

	/* unref list of devices */
	g_ptr_array_unref (array_devices);
	return array;
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
	guint xid;
	gchar *device_id = NULL;
	gchar *filename = NULL;
	gchar *hints = NULL;
	gchar *type = NULL;
	GPtrArray *array = NULL;
	gchar **devices = NULL;
	GcmDevice *device;
	GError *error = NULL;
	const gchar *profile_filename;
	GcmProfileKind profile_kind;
	GcmDeviceKind device_kind;
	guint i;

	/* return 'as' */
	if (g_strcmp0 (method_name, "GetDevices") == 0) {

		/* copy the device id */
		array = gcm_client_get_devices (client);
		devices = g_new0 (gchar *, array->len + 1);
		for (i=0; i<array->len; i++) {
			device = g_ptr_array_index (array, i);
			devices[i] = g_strdup (gcm_device_get_id (device));
		}

		/* format the value */
		value = g_variant_new_strv ((const gchar * const *) devices, -1);
		tuple = g_variant_new_tuple (&value, 1);
		g_dbus_method_invocation_return_value (invocation, tuple);
		goto out;
	}

	/* return 's' */
	if (g_strcmp0 (method_name, "GetProfileForWindow") == 0) {
		g_variant_get (parameters, "(u)", &xid);

		/* get the profile for a window */
		profile_filename = gcm_session_get_profile_for_window (xid, &error);
		if (profile_filename == NULL) {
			g_dbus_method_invocation_return_dbus_error (invocation,
								    "org.gnome.ColorManager.Failed",
								    error->message);
			g_error_free (error);
			goto out;
		}

		/* format the value */
		value = g_variant_new_string (profile_filename);
		tuple = g_variant_new_tuple (&value, 1);
		g_dbus_method_invocation_return_value (invocation, tuple);
		goto out;
	}

	/* return 'a(ss)' */
	if (g_strcmp0 (method_name, "GetProfilesForDevice") == 0) {
		g_variant_get (parameters, "(ss)", &device_id, &hints);

		/* get array of profile filenames */
		array = gcm_session_get_profiles_for_device (device_id, &error);
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
	if (g_strcmp0 (method_name, "GetProfilesForType") == 0) {
		g_variant_get (parameters, "(ss)", &type, &hints);

		/* try to parse string */
		profile_kind = gcm_profile_kind_from_string (type);
		if (profile_kind == GCM_PROFILE_KIND_UNKNOWN) {
			/* get the correct profile kind for the device kind */
			device_kind = gcm_device_kind_from_string (type);
			profile_kind = gcm_utils_device_kind_to_profile_kind (device_kind);
		}

		/* still nothing */
		if (profile_kind == GCM_PROFILE_KIND_UNKNOWN) {
			g_dbus_method_invocation_return_dbus_error (invocation,
								    "org.gnome.ColorManager.Failed",
								    "did not get a profile or device type");
			goto out;
		}

		/* get array of profiles */
		array = gcm_session_get_profiles_for_profile_kind (profile_kind, &error);
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
		array = gcm_session_get_profiles_for_file (filename, &error);
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
	/* reset time */
	g_timer_reset (timer);

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
 * gcm_session_handle_get_property:
 **/
static GVariant *
gcm_session_handle_get_property (GDBusConnection *connection_, const gchar *sender,
				 const gchar *object_path, const gchar *interface_name,
				 const gchar *property_name, GError **error,
				 gpointer user_data)
{
	GVariant *retval = NULL;

	if (g_strcmp0 (property_name, "RenderingIntentDisplay") == 0) {
		retval = g_settings_get_value (settings, GCM_SETTINGS_RENDERING_INTENT_DISPLAY);
	} else if (g_strcmp0 (property_name, "RenderingIntentSoftproof") == 0) {
		retval = g_settings_get_value (settings, GCM_SETTINGS_RENDERING_INTENT_SOFTPROOF);
	} else if (g_strcmp0 (property_name, "ColorspaceRgb") == 0) {
		retval = g_settings_get_value (settings, GCM_SETTINGS_COLORSPACE_RGB);
	} else if (g_strcmp0 (property_name, "ColorspaceCmyk") == 0) {
		retval = g_settings_get_value (settings, GCM_SETTINGS_COLORSPACE_CMYK);
	} else if (g_strcmp0 (property_name, "ColorspaceGray") == 0) {
		retval = g_settings_get_value (settings, GCM_SETTINGS_COLORSPACE_GRAY);
	}

	/* reset time */
	g_timer_reset (timer);

	return retval;
}

/**
 * gcm_session_on_bus_acquired:
 **/
static void
gcm_session_on_bus_acquired (GDBusConnection *connection_, const gchar *name, gpointer user_data)
{
	guint registration_id;
	static const GDBusInterfaceVTable interface_vtable = {
		gcm_session_handle_method_call,
		gcm_session_handle_get_property,
		NULL
	};

	registration_id = g_dbus_connection_register_object (connection_,
							     GCM_DBUS_PATH,
							     introspection->interfaces[0],
							     &interface_vtable,
							     NULL,  /* user_data */
							     NULL,  /* user_data_free_func */
							     NULL); /* GError** */
	g_assert (registration_id > 0);
}

/**
 * gcm_session_on_name_acquired:
 **/
static void
gcm_session_on_name_acquired (GDBusConnection *connection_, const gchar *name, gpointer user_data)
{
	g_debug ("acquired name: %s", name);
	connection = g_object_ref (connection_);
}

/**
 * gcm_session_on_name_lost:
 **/
static void
gcm_session_on_name_lost (GDBusConnection *connection_, const gchar *name, gpointer user_data)
{
	g_debug ("lost name: %s", name);
	g_main_loop_quit (loop);
}

/**
 * gcm_session_emit_changed:
 **/
static void
gcm_session_emit_changed (void)
{
	gboolean ret;
	GError *error = NULL;

	/* check we are connected */
	if (connection == NULL)
		return;

	/* just emit signal */
	ret = g_dbus_connection_emit_signal (connection,
					     NULL,
					     GCM_DBUS_PATH,
					     GCM_DBUS_INTERFACE,
					     "Changed",
					     NULL,
					     &error);
	if (!ret) {
		g_warning ("failed to emit signal: %s", error->message);
		g_error_free (error);
	}
}

/**
 * gcm_session_key_changed_cb:
 **/
static void
gcm_session_key_changed_cb (GSettings *settings_, const gchar *key, gpointer user_data)
{
	gcm_session_emit_changed ();
}

/**
 * gcm_session_client_changed_cb:
 **/
static void
gcm_session_client_changed_cb (GcmClient *client_, GcmDevice *device, gpointer user_data)
{
	gcm_session_emit_changed ();
}

/**
 * main:
 **/
int
main (int argc, char *argv[])
{
	gboolean no_timed_exit = FALSE;
	GOptionContext *context;
	GError *error = NULL;
	gboolean ret;
	guint retval = 1;
	guint owner_id = 0;
	guint poll_id = 0;
	GFile *file = NULL;
	gchar *introspection_data = NULL;

	const GOptionEntry options[] = {
		{ "no-timed-exit", '\0', 0, G_OPTION_ARG_NONE, &no_timed_exit,
		  _("Do not exit after the request has been processed"), NULL },
		{ NULL}
	};

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	if (! g_thread_supported ())
		g_thread_init (NULL);
	g_type_init ();
	notify_init ("gnome-color-manager");

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

	/* get the settings */
	settings = g_settings_new (GCM_SETTINGS_SCHEMA);
	g_signal_connect (settings, "changed", G_CALLBACK (gcm_session_key_changed_cb), NULL);

	/* monitor devices as they are added */
	client = gcm_client_new ();
	gcm_client_set_use_threads (client, TRUE);
	g_signal_connect (client, "added", G_CALLBACK (gcm_session_added_cb), NULL);
	g_signal_connect (client, "added", G_CALLBACK (gcm_session_client_changed_cb), NULL);
	g_signal_connect (client, "removed", G_CALLBACK (gcm_session_client_changed_cb), NULL);
	g_signal_connect (client, "changed", G_CALLBACK (gcm_session_client_changed_cb), NULL);

	/* have access to all profiles */
	profile_store = gcm_profile_store_new ();
	gcm_profile_store_search (profile_store);
	timer = g_timer_new ();

	/* get all connected devices */
	ret = gcm_client_coldplug (client, GCM_CLIENT_COLDPLUG_ALL, &error);
	if (!ret) {
		g_warning ("failed to coldplug: %s", error->message);
		g_error_free (error);
	}

	/* create new objects */
	loop = g_main_loop_new (NULL, FALSE);

	/* load introspection from file */
	file = g_file_new_for_path (DATADIR "/dbus-1/interfaces/org.gnome.ColorManager.xml");
	ret = g_file_load_contents (file, NULL, &introspection_data, NULL, NULL, &error);
	if (!ret) {
		g_warning ("failed to load introspection: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* build introspection from XML */
	introspection = g_dbus_node_info_new_for_xml (introspection_data, &error);
	if (introspection == NULL) {
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
				   NULL, NULL);

	/* only timeout if we have specified it on the command line */
	if (!no_timed_exit) {
		poll_id = g_timeout_add_seconds (5, (GSourceFunc) gcm_session_check_idle_cb, NULL);
#if GLIB_CHECK_VERSION(2,25,8)
		g_source_set_name_by_id (poll_id, "[GcmSession] inactivity checker");
#endif
	}

	/* wait */
	g_main_loop_run (loop);

	/* success */
	retval = 0;
out:
	g_free (introspection_data);
	if (poll_id != 0)
		g_source_remove (poll_id);
	if (file != NULL)
		g_object_unref (file);
	if (owner_id > 0)
		g_bus_unown_name (owner_id);
	if (profile_store != NULL)
		g_object_unref (profile_store);
	if (timer != NULL)
		g_timer_destroy (timer);
	if (connection != NULL)
		g_object_unref (connection);
	g_dbus_node_info_unref (introspection);
	g_object_unref (settings);
	g_object_unref (client);
	g_main_loop_unref (loop);
	return retval;
}


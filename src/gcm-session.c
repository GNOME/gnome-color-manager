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
#include <colord.h>
#include <libnotify/notify.h>

#include "gcm-debug.h"
#include "gcm-dmi.h"
#include "gcm-exif.h"
#include "gcm-profile-store.h"
#include "gcm-utils.h"
#include "gcm-x11-output.h"
#include "gcm-x11-screen.h"

static GMainLoop *loop = NULL;
static GSettings *settings = NULL;
static GDBusNodeInfo *introspection = NULL;
static CdClient *client = NULL;
static GcmProfileStore *profile_store = NULL;
static GcmDmi *dmi = NULL;
static GcmX11Screen *x11_screen = NULL;
static GDBusConnection *connection = NULL;

#define GCM_SESSION_NOTIFY_TIMEOUT		30000 /* ms */
#define GCM_ICC_PROFILE_IN_X_VERSION_MAJOR	0
#define GCM_ICC_PROFILE_IN_X_VERSION_MINOR	3

/**
 * cd_device_get_title:
 **/
static gchar *
cd_device_get_title (CdDevice *device)
{
	const gchar *vendor;
	const gchar *model;

	model = cd_device_get_model (device);
	vendor = cd_device_get_vendor (device);
	if (model != NULL && vendor != NULL)
		return g_strdup_printf ("%s - %s", vendor, model);
	if (vendor != NULL)
		return g_strdup (vendor);
	if (model != NULL)
		return g_strdup (model);
	return g_strdup (cd_device_get_id (device));
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
		g_settings_set_int (settings,
				    GCM_SETTINGS_RECALIBRATE_DISPLAY_THRESHOLD,
				    0);
	} else if (g_strcmp0 (action, "printer") == 0) {
		g_settings_set_int (settings,
				    GCM_SETTINGS_RECALIBRATE_PRINTER_THRESHOLD,
				    0);
	} else if (g_strcmp0 (action, "recalibrate") == 0) {
		ret = g_spawn_command_line_async ("gnome-control-center color",
						  &error);
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
gcm_session_notify_recalibrate (const gchar *title,
				const gchar *message,
				CdDeviceKind kind)
{
	gboolean ret;
	GError *error = NULL;
	NotifyNotification *notification;

	/* show a bubble */
	notification = notify_notification_new (title, message, GCM_STOCK_ICON);
	notify_notification_set_timeout (notification, GCM_SESSION_NOTIFY_TIMEOUT);
	notify_notification_set_urgency (notification, NOTIFY_URGENCY_LOW);

	/* TRANSLATORS: button: this is to open GCM */
	notify_notification_add_action (notification,
					"recalibrate",
					_("Recalibrate now"),
					gcm_session_notify_cb,
					NULL, NULL);

	/* TRANSLATORS: button: this is to ignore the recalibrate notifications */
	notify_notification_add_action (notification,
					cd_device_kind_to_string (kind),
					_("Ignore"),
					gcm_session_notify_cb,
					NULL, NULL);

	ret = notify_notification_show (notification, &error);
	if (!ret) {
		g_warning ("failed to show notification: %s",
			   error->message);
		g_error_free (error);
	}
	return ret;
}

/**
 * gcm_session_notify_device:
 **/
static void
gcm_session_notify_device (CdDevice *device)
{
	CdDeviceKind kind;
	const gchar *title;
	gchar *device_title = NULL;
	gchar *message;
	gint threshold;
	glong since;
	GTimeVal timeval;

	/* get current time */
	g_get_current_time (&timeval);

	/* TRANSLATORS: this is when the device has not been recalibrated in a while */
	title = _("Recalibration required");
	device_title = cd_device_get_title (device);

	/* check we care */
	kind = cd_device_get_kind (device);
	if (kind == CD_DEVICE_KIND_DISPLAY) {

		/* get from GSettings */
		threshold = g_settings_get_int (settings,
						GCM_SETTINGS_RECALIBRATE_DISPLAY_THRESHOLD);

		/* TRANSLATORS: this is when the display has not been recalibrated in a while */
		message = g_strdup_printf (_("The display '%s' should be recalibrated soon."),
					   device_title);
	} else {

		/* get from GSettings */
		threshold = g_settings_get_int (settings,
						GCM_SETTINGS_RECALIBRATE_DISPLAY_THRESHOLD);

		/* TRANSLATORS: this is when the printer has not been recalibrated in a while */
		message = g_strdup_printf (_("The printer '%s' should be recalibrated soon."),
					   device_title);
	}

	/* check if we need to notify */
	since = timeval.tv_sec - cd_device_get_modified (device);
	if (threshold > since)
		gcm_session_notify_recalibrate (title, message, kind);
	g_free (device_title);
	g_free (message);
}

/**
 * gcm_session_device_added_notify_cb:
 **/
static void
gcm_session_device_added_notify_cb (CdClient *client_,
				    CdDevice *device,
				    gpointer user_data)
{
	CdDeviceKind kind;
	CdProfile *profile;
	const gchar *filename;
	gchar *basename = NULL;
	gboolean allow_notifications;

	/* check we care */
	kind = cd_device_get_kind (device);
	if (kind != CD_DEVICE_KIND_DISPLAY &&
	    kind != CD_DEVICE_KIND_PRINTER)
		return;

	/* ensure we have a profile */
	profile = cd_device_get_default_profile (device);
	if (profile == NULL) {
		g_debug ("no profile set for %s", cd_device_get_id (device));
		goto out;
	}

	/* ensure it's a profile generated by us */
	filename = cd_profile_get_filename (profile);
	basename = g_path_get_basename (filename);
	if (!g_str_has_prefix (basename, "GCM")) {
		g_debug ("not a GCM profile for %s: %s",
			 cd_device_get_id (device), filename);
		goto out;
	}

	/* do we allow notifications */
	allow_notifications = g_settings_get_boolean (settings,
						      GCM_SETTINGS_SHOW_NOTIFICATIONS);
	if (!allow_notifications)
		goto out;

	/* handle device */
	gcm_session_notify_device (device);
out:
	g_free (basename);
}

/**
 * gcm_session_profile_added_notify_cb:
 **/
static void
gcm_session_profile_added_notify_cb (CdClient *client_,
				     CdProfile *profile,
				     gpointer user_data)
{
	GHashTable *metadata;
	const gchar *edid_md5;
	GcmX11Output *output = NULL;
	GError *error = NULL;
	CdDevice *device = NULL;
	gchar *device_id = NULL;
	gboolean ret;

	/* does the profile have EDID metadata? */
	metadata = cd_profile_get_metadata (profile);
	edid_md5 = g_hash_table_lookup (metadata, "EDID_md5");
	if (edid_md5 == NULL)
		goto out;

	/* get the GcmX11Output for the edid */
	output = gcm_x11_screen_get_output_by_edid (x11_screen,
						    edid_md5,
						    &error);
	if (output == NULL) {
		g_debug ("edid hash %s ignored: %s",
			 edid_md5,
			 error->message);
		g_error_free (error);
		goto out;
	}

	/* get the CdDevice for this ID */
	device_id = g_strdup_printf ("xrandr_%s",
				     gcm_x11_output_get_name (output));
	device = cd_client_find_device_sync (client,
					     device_id,
					     NULL,
					     &error);
	if (device == NULL) {
		g_warning ("not found device %s which should have been added: %s",
			   device_id,
			   error->message);
		g_error_free (error);
		goto out;
	}

	/* add the profile to the device */
	ret = cd_device_add_profile_sync (device,
					  CD_DEVICE_RELATION_SOFT,
					  profile,
					  NULL,
					  &error);
	if (!ret) {
		/* this will fail if the profile is already added */
		g_debug ("failed to assign auto-edid profile to device %s: %s",
			 gcm_x11_output_get_name (output),
			 error->message);
		g_error_free (error);
		goto out;
	}

	/* phew! */
	g_debug ("successfully assigned %s to %s",
		 cd_profile_get_object_path (profile),
		 cd_device_get_object_path (device));
out:
	g_free (device_id);
	if (metadata != NULL)
		g_hash_table_unref (metadata);
	if (output != NULL)
		g_object_unref (output);
	if (device != NULL)
		g_object_unref (device);
}

/**
 * gcm_apply_create_icc_profile_for_edid:
 **/
static gboolean
gcm_apply_create_icc_profile_for_edid (GcmEdid *edid,
				       const gchar *filename,
				       GError **error)
{
	const gchar *data;
	gboolean ret;
	gchar *title = NULL;
	GcmProfile *profile = NULL;

	/* ensure the per-user directory exists */
	ret = gcm_utils_mkdir_for_filename (filename, error);
	if (!ret)
		goto out;

	/* create new profile */
	profile = gcm_profile_new ();
	gcm_profile_set_colorspace (profile, CD_COLORSPACE_RGB);
	gcm_profile_set_copyright (profile, "No copyright");
	gcm_profile_set_kind (profile, CD_PROFILE_KIND_DISPLAY_DEVICE);

	/* get manufacturer */
	data = gcm_edid_get_vendor_name (edid);
	if (data == NULL)
		data = "Unknown vendor";
	gcm_profile_set_manufacturer (profile, data);

	/* get model */
	data = gcm_edid_get_monitor_name (edid);
	if (data == NULL)
		data = "Unknown monitor";
	gcm_profile_set_model (profile, data);

	/* TRANSLATORS: this is prepended to the device title to let the use know it was generated by us automatically */
	title = g_strdup_printf ("%s, %s",
				 _("Automatic"),
				 gcm_edid_get_monitor_name (edid));
	gcm_profile_set_description (profile, title);

	/* generate a profile from the chroma data */
	ret = gcm_profile_create_from_chroma (profile,
					      gcm_edid_get_gamma (edid),
					      gcm_edid_get_red (edid),
					      gcm_edid_get_green (edid),
					      gcm_edid_get_blue (edid),
					      gcm_edid_get_white (edid),
					      error);
	if (!ret)
		goto out;

	/* set 'ICC meta Tag for Monitor Profiles' data */
	gcm_profile_set_data (profile,
			      "EDID_md5",
			      gcm_edid_get_checksum (edid));
	gcm_profile_set_data (profile,
			      "EDID_model",
			      gcm_edid_get_monitor_name (edid));
	gcm_profile_set_data (profile,
			      "EDID_serial",
			      gcm_edid_get_serial_number (edid));
	gcm_profile_set_data (profile,
			      "EDID_mnft",
			      gcm_edid_get_pnp_id (edid));
	gcm_profile_set_data (profile,
			      "EDID_manufacturer",
			      gcm_edid_get_vendor_name (edid));

	/* save this */
	ret = gcm_profile_save (profile, filename, error);
	if (!ret)
		goto out;
out:
	g_object_unref (profile);
	g_free (title);
	return ret;
}

/**
 * gcm_session_device_set_gamma:
 **/
static gboolean
gcm_session_device_set_gamma (GcmX11Output *output,
			      CdProfile *profile,
			      GError **error)
{
	const gchar *filename;
	gboolean ret;
	GcmClut *clut = NULL;
	GcmProfile *gcm_profile = NULL;
	GFile *file = NULL;

	/* parse locally so we can access the VCGT data */
	gcm_profile = gcm_profile_new ();
	filename = cd_profile_get_filename (profile);
	file = g_file_new_for_path (filename);
	ret = gcm_profile_parse (gcm_profile, file, error);
	if (!ret)
		goto out;

	/* create a lookup table */
	clut = gcm_profile_generate_vcgt (gcm_profile,
					  gcm_x11_output_get_gamma_size (output));

	/* apply the vcgt to this output */
	ret = gcm_x11_output_set_gamma_from_clut (output, clut, error);
	if (!ret)
		goto out;
out:
	if (clut != NULL)
		g_object_unref (clut);
	if (gcm_profile != NULL)
		g_object_unref (gcm_profile);
	if (file != NULL)
		g_object_unref (file);
	return ret;
}

/**
 * gcm_session_device_reset_gamma:
 **/
static gboolean
gcm_session_device_reset_gamma (GcmX11Output *output,
			        GError **error)
{
	gboolean ret;
	GcmClut *clut;

	/* create a linear ramp */
	clut = gcm_clut_new ();
	g_object_set (clut,
		      "size", gcm_x11_output_get_gamma_size (output),
		      NULL);

	/* apply the vcgt to this output */
	ret = gcm_x11_output_set_gamma_from_clut (output, clut, error);
	if (!ret)
		goto out;
out:
	g_object_unref (clut);
	return ret;
}

/**
 * gcm_session_device_assign:
 **/
static void
gcm_session_device_assign (CdDevice *device)
{
	CdDeviceKind kind;
	CdProfile *profile = NULL;
	const gchar *filename;
	gboolean ret;
	gchar *autogen_filename = NULL;
	gchar *autogen_path = NULL;
	GcmEdid *edid = NULL;
	GcmX11Output *output = NULL;
	GError *error = NULL;
	const gchar *qualifier_default[] = { "*", NULL};
	const gchar *xrandr_id;

	/* check we care */
	kind = cd_device_get_kind (device);
	if (kind != CD_DEVICE_KIND_DISPLAY)
		return;

	g_debug ("need to assign display device %s",
		 cd_device_get_id (device));

	/* get the GcmX11Output for the device id */
	xrandr_id = cd_device_get_id (device);
	if (!g_str_has_prefix (xrandr_id, "xrandr_")) {
		g_warning ("xrandr device does not start with prefix: %s",
			   xrandr_id);
		return;
	}
	xrandr_id += 7;
	output = gcm_x11_screen_get_output_by_name (x11_screen,
						    xrandr_id,
						    &error);
	if (output == NULL) {
		g_warning ("no %s device found: %s",
			   cd_device_get_id (device),
			   error->message);
		g_error_free (error);
		goto out;
	}

	/* get the output EDID */
	edid = gcm_x11_output_get_edid (output, &error);
	if (edid == NULL) {
		g_warning ("unable to get EDID for %s: %s",
			   cd_device_get_id (device),
			   error->message);
		g_error_free (error);
		goto out;
	}

	/* create profile from device edid if it does not exist */
	autogen_filename = g_strdup_printf ("edid-%s.icc",
					    gcm_edid_get_checksum (edid));
	autogen_path = g_build_filename (g_get_user_data_dir (),
					 "icc", autogen_filename, NULL);

	if (g_file_test (autogen_path, G_FILE_TEST_EXISTS)) {
		g_debug ("auto-profile edid %s exists", autogen_path);
	} else {
		g_debug ("auto-profile edid does not exist, creating as %s",
			 autogen_path);
		ret = gcm_apply_create_icc_profile_for_edid (edid,
							     autogen_path,
							     &error);
		if (!ret) {
			g_warning ("failed to create profile from EDID data: %s",
				     error->message);
			g_clear_error (&error);
		}
	}

	/* get the default profile for the device */
	profile = cd_device_get_profile_for_qualifiers_sync (device,
							     qualifier_default,
							     NULL,
							     &error);
	if (profile == NULL) {
		g_debug ("%s has no default profile to set: %s",
			 cd_device_get_id (device),
			 error->message);
		g_clear_error (&error);

		/* clear the _ICC_PROFILE atom if not logging in */
		ret = gcm_x11_output_remove_profile (output,
						     &error);
		if (!ret) {
			g_warning ("failed to clear screen _ICC_PROFILE: %s",
				   error->message);
			g_clear_error (&error);
		}

		/* the default output? */
		if (gcm_x11_output_get_primary (output)) {
			ret = gcm_x11_screen_remove_profile (x11_screen,
							     &error);
			if (!ret) {
				g_warning ("failed to clear output _ICC_PROFILE: %s",
					   error->message);
				g_clear_error (&error);
			}
			ret = gcm_x11_screen_remove_protocol_version (x11_screen,
								      &error);
			if (!ret) {
				g_warning ("failed to clear output _ICC_PROFILE version: %s",
					   error->message);
				g_clear_error (&error);
			}
		}

		/* reset, as we want linear profiles for profiling */
		ret = gcm_session_device_reset_gamma (output,
						      &error);
		if (!ret) {
			g_warning ("failed to reset %s gamma tables: %s",
				   cd_device_get_id (device),
				   error->message);
			g_error_free (error);
			goto out;
		}
		goto out;
	}

	/* get the filename */
	filename = cd_profile_get_filename (profile);
	g_assert (filename != NULL);

	/* set the _ICC_PROFILE atom */
	if (gcm_x11_output_get_primary (output)) {
		ret = gcm_x11_screen_set_profile (x11_screen,
						 filename,
						 &error);
		if (!ret) {
			g_warning ("failed to set screen _ICC_PROFILE: %s",
				   error->message);
			g_clear_error (&error);
		}
		ret = gcm_x11_screen_set_protocol_version (x11_screen,
							   GCM_ICC_PROFILE_IN_X_VERSION_MAJOR,
							   GCM_ICC_PROFILE_IN_X_VERSION_MINOR,
							   &error);
		if (!ret) {
			g_warning ("failed to set screen _ICC_PROFILE: %s",
				   error->message);
			g_clear_error (&error);
		}
	}
	ret = gcm_x11_output_set_profile (output,
					  filename,
					  &error);
	if (!ret) {
		g_warning ("failed to set output _ICC_PROFILE: %s",
			   error->message);
		g_clear_error (&error);
	}

	/* create a vcgt for this icc file */
	ret = cd_profile_get_has_vcgt (profile);
	if (ret) {
		ret = gcm_session_device_set_gamma (output,
						    profile,
						    &error);
		if (!ret) {
			g_warning ("failed to set %s gamma tables: %s",
				   cd_device_get_id (device),
				   error->message);
			g_error_free (error);
			goto out;
		}
	} else {
		ret = gcm_session_device_reset_gamma (output,
						      &error);
		if (!ret) {
			g_warning ("failed to reset %s gamma tables: %s",
				   cd_device_get_id (device),
				   error->message);
			g_error_free (error);
			goto out;
		}
	}
out:
	g_free (autogen_filename);
	g_free (autogen_path);
	if (edid != NULL)
		g_object_unref (edid);
	if (output != NULL)
		g_object_unref (output);
	if (profile != NULL)
		g_object_unref (profile);
}

/**
 * gcm_session_device_added_assign_cb:
 **/
static void
gcm_session_device_added_assign_cb (CdClient *client_,
				    CdDevice *device,
				    gpointer user_data)
{
	gcm_session_device_assign (device);
}

/**
 * gcm_session_device_changed_assign_cb:
 **/
static void
gcm_session_device_changed_assign_cb (CdClient *client_,
				      CdDevice *device,
				      gpointer user_data)
{
	g_debug ("%s changed", cd_device_get_id (device));
	gcm_session_device_assign (device);
}

/**
 * gcm_session_get_profile_for_window:
 **/
static const gchar *
gcm_session_get_profile_for_window (guint xid, GError **error)
{
	CdDevice *device = NULL;
	CdProfile *profile = NULL;
	const gchar *filename = NULL;
	gchar *device_id = NULL;
	GcmX11Output *output = NULL;
	GdkWindow *window;
	GError *error_local = NULL;

	g_debug ("getting profile for %i", xid);

	/* get window for xid */
	window = gdk_x11_window_foreign_new_for_display (gdk_display_get_default (), xid);
	if (window == NULL) {
		g_set_error (error, 1, 0,
			     "failed to create window for xid %i",
			     xid);
		goto out;
	}

	/* get output for this window */
	output = cd_x11_screen_get_output_by_window (x11_screen, window);
	if (output == NULL) {
		g_set_error (error, 1, 0,
			     "no output found for xid %i",
			     xid);
		goto out;
	}

	/* get device for this output */
	device_id = g_strdup_printf ("xrandr_%s",
				     gcm_x11_output_get_name (output));
	device = cd_client_find_device_sync (client,
					     device_id,
					     NULL,
					     &error_local);

	if (device == NULL) {
		g_set_error (error, 1, 0,
			     "no device found for xid %i: %s",
			     xid, error_local->message);
		g_error_free (error_local);
		goto out;
	}

	/* get the default profile for the device */
	profile = cd_device_get_default_profile (device);
	if (filename == NULL) {
		g_set_error (error, 1, 0,
			     "no profiles found for device %s",
			     device_id);
		goto out;
	}

	/* get the filename */
	filename = cd_profile_get_filename (profile);
	if (filename == NULL) {
		g_set_error (error, 1, 0,
			     "no filname found for profile %s",
			     cd_profile_get_id (profile));
		goto out;
	}
out:
	g_free (device_id);
	if (window != NULL)
		g_object_unref (window);
	if (profile != NULL)
		g_object_unref (profile);
	if (device != NULL)
		g_object_unref (device);
	if (output != NULL)
		g_object_unref (output);
	return filename;
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
//	array_devices = cd_client_get_devices (client);
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
gcm_session_get_profiles_for_device (const gchar *device_id_with_prefix, GError **error)
{
	const gchar *device_id;
	const gchar *device_id_tmp;
	guint i;
	gboolean use_native_device = FALSE;
	CdDevice *device;
	GPtrArray *array = NULL;
	GPtrArray *array_devices = NULL;

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
//	array_devices = cd_client_get_devices (client);
	for (i=0; i<array_devices->len; i++) {
		device = g_ptr_array_index (array_devices, i);

		/* get the id for this device */
		device_id_tmp = cd_device_get_id (device);

		/* wrong kind of device */
		if (device_id_tmp == NULL)
			continue;

		/* compare what we have against what we were given */
		g_debug ("comparing %s with %s", device_id_tmp, device_id);
		if (g_strcmp0 (device_id_tmp, device_id) == 0) {
			array = cd_device_get_profiles (device);
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
	GError *error = NULL;
	const gchar *profile_filename;

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
 * gcm_session_profile_store_added_cb:
 **/
static void
gcm_session_profile_store_added_cb (GcmProfileStore *profile_store_,
				    const gchar *filename,
				    gpointer user_data)
{
	CdProfile *profile;
	GError *error = NULL;
	GHashTable *profile_props;

	g_debug ("profile %s added", filename);
	profile_props = g_hash_table_new_full (g_str_hash, g_str_equal,
					       g_free, g_free);
	g_hash_table_insert (profile_props,
			     g_strdup ("Filename"),
			     g_strdup (filename));
	profile = cd_client_create_profile_sync (client,
						 filename,
						 CD_OBJECT_SCOPE_TEMP,
						 profile_props,
						 NULL,
						 &error);
	if (profile == NULL) {
		g_warning ("failed to create profile: %s",
			   error->message);
		g_error_free (error);
		goto out;
	}
out:
	g_hash_table_unref (profile_props);
	if (profile != NULL)
		g_object_unref (profile);
}

/**
 * gcm_session_profile_store_removed_cb:
 **/
static void
gcm_session_profile_store_removed_cb (GcmProfileStore *profile_store_,
				      const gchar *filename,
				      gpointer user_data)
{
	gboolean ret;
	GError *error = NULL;

	g_debug ("profile %s removed", filename);
	ret = cd_client_delete_profile_sync (client,
					     filename,
					     NULL,
					     &error);
	if (!ret) {
		g_warning ("failed to create profile: %s",
			   error->message);
		g_error_free (error);
		goto out;
	}
out:
	return;
}

/**
 * gcm_session_add_x11_output:
 **/
static void
gcm_session_add_x11_output (GcmX11Output *output)
{
	CdDevice *device = NULL;
	const gchar *model;
	const gchar *serial;
	const gchar *vendor;
	gboolean ret;
	gchar *device_id = NULL;
	GcmEdid *edid;
	GError *error = NULL;
	GHashTable *device_props = NULL;

	/* get edid */
	edid = gcm_x11_output_get_edid (output, &error);
	if (edid == NULL) {
		g_warning ("failed to get edid: %s",
			   error->message);
		g_error_free (error);
		goto out;
	}

	/* is this an internal device? */
	ret = gcm_utils_output_is_lcd_internal (gcm_x11_output_get_name (output));
	if (ret) {
		model = gcm_dmi_get_name (dmi);
		vendor = gcm_dmi_get_vendor (dmi);
	} else {
		model = gcm_edid_get_monitor_name (edid);
		if (model == NULL)
			model = gcm_x11_output_get_name (output);
		vendor = gcm_edid_get_vendor_name (edid);
	}

	/* get a serial number if one exists */
	serial = gcm_edid_get_serial_number (edid);
	if (serial == NULL)
		serial = "unknown";

	device_id = g_strdup_printf ("xrandr_%s",
				     gcm_x11_output_get_name (output));
	g_debug ("output %s added", device_id);
	device_props = g_hash_table_new_full (g_str_hash, g_str_equal,
					      g_free, g_free);
	g_hash_table_insert (device_props,
			     g_strdup ("Kind"),
			     g_strdup (cd_device_kind_to_string (CD_DEVICE_KIND_DISPLAY)));
	g_hash_table_insert (device_props,
			     g_strdup ("Mode"),
			     g_strdup (cd_device_mode_to_string (CD_DEVICE_MODE_PHYSICAL)));
	g_hash_table_insert (device_props,
			     g_strdup ("Colorspace"),
			     g_strdup (cd_colorspace_to_string (CD_COLORSPACE_RGB)));
	g_hash_table_insert (device_props,
			     g_strdup ("Vendor"),
			     g_strdup (vendor));
	g_hash_table_insert (device_props,
			     g_strdup ("Model"),
			     g_strdup (model));
	g_hash_table_insert (device_props,
			     g_strdup ("Serial"),
			     g_strdup (serial));
	device = cd_client_create_device_sync (client,
					       device_id,
					       CD_OBJECT_SCOPE_TEMP,
					       device_props,
					       NULL,
					       &error);
	if (device == NULL) {
		g_warning ("failed to create device: %s",
			   error->message);
		g_error_free (error);
		goto out;
	}
out:
	g_free (device_id);
	if (device_props != NULL)
		g_hash_table_unref (device_props);
	if (device != NULL)
		g_object_unref (device);
	if (edid != NULL)
		g_object_unref (edid);
}


/**
 * gcm_x11_screen_output_added_cb:
 **/
static void
gcm_x11_screen_output_added_cb (GcmX11Screen *screen_,
				GcmX11Output *output,
				gpointer user_data)
{
	gcm_session_add_x11_output (output);
}

/**
 * gcm_x11_screen_output_removed_cb:
 **/
static void
gcm_x11_screen_output_removed_cb (GcmX11Screen *screen_,
				  GcmX11Output *output,
				  gpointer user_data)
{
	gboolean ret;
	GError *error = NULL;

	g_debug ("output %s removed",
		 gcm_x11_output_get_name (output));
	ret = cd_client_delete_device_sync (client,
					    gcm_x11_output_get_name (output),
					    NULL,
					    &error);
	if (!ret) {
		g_warning ("failed to delete device: %s",
			   error->message);
		g_error_free (error);
		goto out;
	}
out:
	return;
}

/**
 * gcm_session_colord_appeared_cb:
 **/
static void
gcm_session_colord_appeared_cb (GDBusConnection *_connection,
				const gchar *name,
				const gchar *name_owner,
				gpointer user_data)
{
	gboolean ret;
	GError *error = NULL;
	GPtrArray *array = NULL;
	GPtrArray *outputs = NULL;
	guint i;
	CdDevice *device;
	GcmX11Output *output;

	g_debug ("%s has appeared as %s", name, name_owner);

	/* add screens */
	ret = gcm_x11_screen_refresh (x11_screen, &error);
	if (!ret) {
		g_warning ("failed to refresh: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* get X11 outputs */
	outputs = gcm_x11_screen_get_outputs (x11_screen, &error);
	if (outputs == NULL) {
		g_warning ("failed to get outputs: %s", error->message);
		g_error_free (error);
		goto out;
	}
	for (i=0; i<outputs->len; i++) {
		output = g_ptr_array_index (outputs, i);
		gcm_session_add_x11_output (output);
	}

	/* only connect when colord is awake */
	g_signal_connect (x11_screen, "added",
			  G_CALLBACK (gcm_x11_screen_output_added_cb), NULL);
	g_signal_connect (x11_screen, "removed",
			  G_CALLBACK (gcm_x11_screen_output_removed_cb), NULL);

	/* add profiles */
	gcm_profile_store_search (profile_store);

	/* set for each device that already exist */
	array = cd_client_get_devices_sync (client, NULL, &error);
	if (array == NULL) {
		g_warning ("failed to get devices: %s",
			   error->message);
		g_error_free (error);
		goto out;
	}
	for (i=0; i<array->len; i++) {
		device = g_ptr_array_index (array, i);
		gcm_session_device_assign (device);
	}
out:
	if (array != NULL)
		g_ptr_array_unref (array);
}

/**
 * gcm_session_colord_vanished_cb:
 **/
static void
gcm_session_colord_vanished_cb (GDBusConnection *_connection,
				const gchar *name,
				gpointer user_data)
{
	g_debug ("%s has vanished", name);
}

/**
 * main:
 **/
int
main (int argc, char *argv[])
{
	gboolean login = FALSE;
	gboolean ret;
	gchar *introspection_data = NULL;
	GError *error = NULL;
	GFile *file = NULL;
	GOptionContext *context;
	guint owner_id = 0;
	guint poll_id = 0;
	guint retval = 1;
	guint watcher_id = 0;

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
	g_signal_connect (settings, "changed",
			  G_CALLBACK (gcm_session_key_changed_cb), NULL);

	/* use DMI data for internal panels */
	dmi = gcm_dmi_new ();

	/* monitor daemon */
	client = cd_client_new ();
	g_signal_connect (client, "device-added",
			  G_CALLBACK (gcm_session_device_added_notify_cb),
			  NULL);
	g_signal_connect (client, "profile-added",
			  G_CALLBACK (gcm_session_profile_added_notify_cb),
			  NULL);
	g_signal_connect (client, "device-added",
			  G_CALLBACK (gcm_session_device_added_assign_cb),
			  NULL);
	g_signal_connect (client, "device-changed",
			  G_CALLBACK (gcm_session_device_changed_assign_cb),
			  NULL);
	ret = cd_client_connect_sync (client, NULL, &error);
	if (!ret) {
		g_warning ("failed to connect to colord: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* have access to all profiles */
	profile_store = gcm_profile_store_new ();
	g_signal_connect (profile_store, "added",
			  G_CALLBACK (gcm_session_profile_store_added_cb),
			  NULL);
	g_signal_connect (profile_store, "removed",
			  G_CALLBACK (gcm_session_profile_store_removed_cb),
			  NULL);

	/* monitor displays */
	x11_screen = gcm_x11_screen_new ();
	ret = gcm_x11_screen_assign (x11_screen, NULL, &error);
	if (!ret) {
		g_warning ("failed to assign: %s", error->message);
		g_error_free (error);
		goto out;
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

	/* watch to see when colord appears */
	watcher_id = g_bus_watch_name (G_BUS_TYPE_SYSTEM,
				       "org.freedesktop.ColorManager",
				       G_BUS_NAME_WATCHER_FLAGS_NONE,
				       gcm_session_colord_appeared_cb,
				       gcm_session_colord_vanished_cb,
				       NULL,
				       NULL);

	/* wait */
	g_main_loop_run (loop);

	/* success */
	retval = 0;
out:
	g_free (introspection_data);
	if (watcher_id > 0)
		g_bus_unwatch_name (watcher_id);
	if (poll_id != 0)
		g_source_remove (poll_id);
	if (file != NULL)
		g_object_unref (file);
	if (owner_id > 0)
		g_bus_unown_name (owner_id);
	if (profile_store != NULL)
		g_object_unref (profile_store);
	if (dmi != NULL)
		g_object_unref (dmi);
	if (connection != NULL)
		g_object_unref (connection);
	g_dbus_node_info_unref (introspection);
	g_object_unref (settings);
	g_object_unref (client);
	g_main_loop_unref (loop);
	return retval;
}


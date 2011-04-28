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
#include <libnotify/notify.h>
#include <canberra-gtk.h>
#include <lcms2.h>

#include "gcm-debug.h"
#include "gcm-dmi.h"
#include "gcm-exif.h"
#include "gcm-profile-store.h"
#include "gcm-utils.h"
#include "gcm-x11-output.h"
#include "gcm-x11-screen.h"

typedef struct {
	CdClient	*client;
	GcmDmi		*dmi;
	GcmProfileStore	*profile_store;
	GcmX11Screen	*x11_screen;
	GDBusConnection	*connection;
	GDBusNodeInfo	*introspection;
	GMainLoop	*loop;
	GSettings	*settings;
	guint		 watcher_id;
} GcmSessionPrivate;

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
gcm_session_notify_cb (NotifyNotification *notification,
		       gchar *action,
		       gpointer user_data)
{
	gboolean ret;
	GError *error = NULL;
	GcmSessionPrivate *priv = (GcmSessionPrivate *) user_data;

	if (g_strcmp0 (action, "display") == 0) {
		g_settings_set_int (priv->settings,
				    GCM_SETTINGS_RECALIBRATE_DISPLAY_THRESHOLD,
				    0);
	} else if (g_strcmp0 (action, "printer") == 0) {
		g_settings_set_int (priv->settings,
				    GCM_SETTINGS_RECALIBRATE_PRINTER_THRESHOLD,
				    0);
	} else if (g_strcmp0 (action, "recalibrate") == 0) {
		ret = g_spawn_command_line_async (BINDIR "/gcm-prefs",
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
gcm_session_notify_recalibrate (GcmSessionPrivate *priv,
				const gchar *title,
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
					priv, NULL);

	/* TRANSLATORS: button: this is to ignore the recalibrate notifications */
	notify_notification_add_action (notification,
					cd_device_kind_to_string (kind),
					_("Ignore"),
					gcm_session_notify_cb,
					priv, NULL);

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
gcm_session_notify_device (GcmSessionPrivate *priv, CdDevice *device)
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
		threshold = g_settings_get_int (priv->settings,
						GCM_SETTINGS_RECALIBRATE_DISPLAY_THRESHOLD);

		/* TRANSLATORS: this is when the display has not been recalibrated in a while */
		message = g_strdup_printf (_("The display '%s' should be recalibrated soon."),
					   device_title);
	} else {

		/* get from GSettings */
		threshold = g_settings_get_int (priv->settings,
						GCM_SETTINGS_RECALIBRATE_DISPLAY_THRESHOLD);

		/* TRANSLATORS: this is when the printer has not been recalibrated in a while */
		message = g_strdup_printf (_("The printer '%s' should be recalibrated soon."),
					   device_title);
	}

	/* check if we need to notify */
	since = timeval.tv_sec - cd_device_get_modified (device);
	if (threshold > since)
		gcm_session_notify_recalibrate (priv, title, message, kind);
	g_free (device_title);
	g_free (message);
}

/**
 * gcm_session_device_added_notify_cb:
 **/
static void
gcm_session_device_added_notify_cb (CdClient *client,
				    CdDevice *device,
				    GcmSessionPrivate *priv)
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
	allow_notifications = g_settings_get_boolean (priv->settings,
						      GCM_SETTINGS_SHOW_NOTIFICATIONS);
	if (!allow_notifications)
		goto out;

	/* handle device */
	gcm_session_notify_device (priv, device);
out:
	g_free (basename);
}

/**
 * gcm_session_get_output_id:
 **/
static gchar *
gcm_session_get_output_id (GcmX11Output *output)
{
	const gchar *name;
	const gchar *serial;
	const gchar *vendor;
	GcmEdid *edid = NULL;
	GString *device_id;
	GError *error = NULL;

	/* all output devices are prefixed with this */
	device_id = g_string_new ("xrandr");

	/* get the output EDID if possible */
	edid = gcm_x11_output_get_edid (output, &error);
	if (edid == NULL) {
		g_debug ("no edid for %s [%s], falling back to connection name",
			 gcm_x11_output_get_name (output),
			 error->message);
		g_error_free (error);
		g_string_append_printf (device_id,
					"_%s",
					gcm_x11_output_get_name (output));
		goto out;
	}

	/* get EDID data */
	vendor = gcm_edid_get_vendor_name (edid);
	if (vendor != NULL)
		g_string_append_printf (device_id, "-%s", vendor);
	name = gcm_edid_get_monitor_name (edid);
	if (name != NULL)
		g_string_append_printf (device_id, "-%s", name);
	serial = gcm_edid_get_serial_number (edid);
	if (serial != NULL)
		g_string_append_printf (device_id, "-%s", serial);
out:
	if (edid != NULL)
		g_object_unref (edid);
	return g_string_free (device_id, FALSE);
}

/**
 * gcm_session_profile_added_notify_cb:
 **/
static void
gcm_session_profile_added_notify_cb (CdClient *client,
				     CdProfile *profile,
				     GcmSessionPrivate *priv)
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
	output = gcm_x11_screen_get_output_by_edid (priv->x11_screen,
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
	device_id = gcm_session_get_output_id (output);
	device = cd_client_find_device_sync (priv->client,
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
gcm_apply_create_icc_profile_for_edid (GcmSessionPrivate *priv,
				       GcmEdid *edid,
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
		data = gcm_dmi_get_vendor (priv->dmi);
	if (data == NULL)
		data = "Unknown vendor";
	gcm_profile_set_manufacturer (profile, data);

	/* get model */
	data = gcm_edid_get_monitor_name (edid);
	if (data == NULL)
		data = gcm_dmi_get_name (priv->dmi);
	if (data == NULL)
		data = "Unknown monitor";
	gcm_profile_set_model (profile, data);

	/* TRANSLATORS: this is prepended to the device title to let the use know it was generated by us automatically */
	title = g_strdup_printf ("%s, %s",
				 _("Automatic"),
				 data);
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
	data = gcm_edid_get_monitor_name (edid);
	if (data != NULL) {
		gcm_profile_set_data (profile,
				      "EDID_model",
				      data);
	}
	data = gcm_edid_get_serial_number (edid);
	if (data != NULL) {
		gcm_profile_set_data (profile,
				      "EDID_serial",
				      data);
	}
	data = gcm_edid_get_pnp_id (edid);
	if (data != NULL) {
		gcm_profile_set_data (profile,
				      "EDID_mnft",
				      data);
	}
	data = gcm_edid_get_vendor_name (edid);
	if (data != NULL) {
		gcm_profile_set_data (profile,
				      "EDID_manufacturer",
				      data);
	}

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
 * gcm_session_get_x11_output_by_id:
 **/
static GcmX11Output *
gcm_session_get_x11_output_by_id (GcmSessionPrivate *priv,
				  const gchar *device_id,
				  GError **error)
{
	gchar *output_id;
	GcmX11Output *output = NULL;
	GcmX11Output *output_tmp;
	GPtrArray *outputs = NULL;
	guint i;

	/* search all X11 outputs for the device id */
	outputs = gcm_x11_screen_get_outputs (priv->x11_screen, error);
	if (outputs == NULL)
		goto out;
	for (i=0; i<outputs->len && output == NULL; i++) {
		output_tmp = g_ptr_array_index (outputs, i);
		output_id = gcm_session_get_output_id (output_tmp);
		if (g_strcmp0 (output_id, device_id) == 0) {
			output = g_object_ref (output_tmp);
		}
		g_free (output_id);
	}
	if (output == NULL) {
		g_set_error (error, 1, 0,
			     "Failed to find output %s",
			     device_id);
	}
out:
	if (outputs != NULL)
		g_ptr_array_unref (outputs);
	return output;
}

/**
 * gcm_session_device_assign:
 **/
static void
gcm_session_device_assign (GcmSessionPrivate *priv, CdDevice *device)
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
	output = gcm_session_get_x11_output_by_id (priv,
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
		ret = gcm_apply_create_icc_profile_for_edid (priv,
							     edid,
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
			ret = gcm_x11_screen_remove_profile (priv->x11_screen,
							     &error);
			if (!ret) {
				g_warning ("failed to clear output _ICC_PROFILE: %s",
					   error->message);
				g_clear_error (&error);
			}
			ret = gcm_x11_screen_remove_protocol_version (priv->x11_screen,
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
		ret = gcm_x11_screen_set_profile (priv->x11_screen,
						  filename,
						  &error);
		if (!ret) {
			g_warning ("failed to set screen _ICC_PROFILE: %s",
				   error->message);
			g_clear_error (&error);
		}
		ret = gcm_x11_screen_set_protocol_version (priv->x11_screen,
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
gcm_session_device_added_assign_cb (CdClient *client,
				    CdDevice *device,
				    GcmSessionPrivate *priv)
{
	gcm_session_device_assign (priv, device);
}

/**
 * gcm_session_device_changed_assign_cb:
 **/
static void
gcm_session_device_changed_assign_cb (CdClient *client,
				      CdDevice *device,
				      GcmSessionPrivate *priv)
{
	g_debug ("%s changed", cd_device_get_id (device));
	gcm_session_device_assign (priv, device);
}

/**
 * gcm_session_get_profile_for_window:
 **/
static const gchar *
gcm_session_get_profile_for_window (GcmSessionPrivate *priv,
				    guint xid,
				    GError **error)
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
	output = cd_x11_screen_get_output_by_window (priv->x11_screen,
						     window);
	if (output == NULL) {
		g_set_error (error, 1, 0,
			     "no output found for xid %i",
			     xid);
		goto out;
	}

	/* get device for this output */
	device_id = gcm_session_get_output_id (output);
	device = cd_client_find_device_sync (priv->client,
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
	guint xid;
	gchar *device_id = NULL;
	gchar *filename = NULL;
	gchar *hints = NULL;
	gchar *type = NULL;
	GPtrArray *array = NULL;
	gchar **devices = NULL;
	GError *error = NULL;
	const gchar *profile_filename;
	GcmSessionPrivate *priv = (GcmSessionPrivate *) user_data;

	/* return 's' */
	if (g_strcmp0 (method_name, "GetProfileForWindow") == 0) {
		g_variant_get (parameters, "(u)", &xid);

		/* get the profile for a window */
		profile_filename = gcm_session_get_profile_for_window (priv,
								       xid,
								       &error);
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
 * gcm_session_handle_get_property:
 **/
static GVariant *
gcm_session_handle_get_property (GDBusConnection *connection_, const gchar *sender,
				 const gchar *object_path, const gchar *interface_name,
				 const gchar *property_name, GError **error,
				 gpointer user_data)
{
	GVariant *retval = NULL;
	GcmSessionPrivate *priv = (GcmSessionPrivate *) user_data;

	if (g_strcmp0 (property_name, "RenderingIntentDisplay") == 0) {
		retval = g_settings_get_value (priv->settings, GCM_SETTINGS_RENDERING_INTENT_DISPLAY);
	} else if (g_strcmp0 (property_name, "RenderingIntentSoftproof") == 0) {
		retval = g_settings_get_value (priv->settings, GCM_SETTINGS_RENDERING_INTENT_SOFTPROOF);
	} else if (g_strcmp0 (property_name, "ColorspaceRgb") == 0) {
		retval = g_settings_get_value (priv->settings, GCM_SETTINGS_COLORSPACE_RGB);
	} else if (g_strcmp0 (property_name, "ColorspaceCmyk") == 0) {
		retval = g_settings_get_value (priv->settings, GCM_SETTINGS_COLORSPACE_CMYK);
	} else if (g_strcmp0 (property_name, "ColorspaceGray") == 0) {
		retval = g_settings_get_value (priv->settings, GCM_SETTINGS_COLORSPACE_GRAY);
	}
	return retval;
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
		gcm_session_handle_get_property,
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
 * gcm_session_emit_changed:
 **/
static void
gcm_session_emit_changed (GcmSessionPrivate *priv)
{
	gboolean ret;
	GError *error = NULL;

	/* check we are connected */
	if (priv->connection == NULL)
		return;

	/* just emit signal */
	ret = g_dbus_connection_emit_signal (priv->connection,
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
gcm_session_key_changed_cb (GSettings *settings,
			    const gchar *key,
			    GcmSessionPrivate *priv)
{
	gcm_session_emit_changed (priv);
}

/**
 * gcm_session_get_precooked_md5:
 **/
static gchar *
gcm_session_get_precooked_md5 (cmsHPROFILE lcms_profile)
{
	cmsUInt8Number profile_id[16];
	gboolean md5_precooked = FALSE;
	guint i;
	gchar *md5 = NULL;

	/* check to see if we have a pre-cooked MD5 */
	cmsGetHeaderProfileID (lcms_profile, profile_id);
	for (i=0; i<16; i++) {
		if (profile_id[i] != 0) {
			md5_precooked = TRUE;
			break;
		}
	}
	if (!md5_precooked)
		goto out;

	/* convert to a hex string */
	md5 = g_new0 (gchar, 32 + 1);
	for (i=0; i<16; i++)
		g_snprintf (md5 + i*2, 3, "%02x", profile_id[i]);
out:
	return md5;
}

/**
 * gcm_session_get_md5_for_filename:
 **/
static gchar *
gcm_session_get_md5_for_filename (const gchar *filename,
				  GError **error)
{
	gboolean ret;
	gchar *checksum = NULL;
	gchar *data = NULL;
	gsize length;
	cmsHPROFILE lcms_profile = NULL;

	/* get the internal profile id, if it exists */
	lcms_profile = cmsOpenProfileFromFile (filename, "r");
	if (lcms_profile == NULL) {
		g_set_error_literal (error, 1, 0,
				     "failed to load: not an ICC profile");
		goto out;
	}
	checksum = gcm_session_get_precooked_md5 (lcms_profile);
	if (checksum != NULL)
		goto out;

	/* generate checksum */
	ret = g_file_get_contents (filename, &data, &length, error);
	if (!ret)
		goto out;
	checksum = g_compute_checksum_for_data (G_CHECKSUM_MD5,
						(const guchar *) data,
						length);
out:
	g_free (data);
	if (lcms_profile != NULL)
		cmsCloseProfile (lcms_profile);
	return checksum;
}


/**
 * gcm_session_profile_store_added_cb:
 **/
static void
gcm_session_profile_store_added_cb (GcmProfileStore *profile_store_,
				    const gchar *filename,
				    GcmSessionPrivate *priv)
{
	CdProfile *profile = NULL;
	gchar *checksum = NULL;
	gchar *profile_id = NULL;
	GError *error = NULL;
	GHashTable *profile_props = NULL;

	g_debug ("profile %s added", filename);

	/* generate ID */
	checksum = gcm_session_get_md5_for_filename (filename, &error);
	if (checksum == NULL) {
		g_warning ("failed to get profile checksum: %s",
			   error->message);
		g_error_free (error);
		goto out;
	}
	profile_id = g_strdup_printf ("icc-%s", checksum);
	profile_props = g_hash_table_new_full (g_str_hash, g_str_equal,
					       g_free, g_free);
	g_hash_table_insert (profile_props,
			     g_strdup ("Filename"),
			     g_strdup (filename));
	g_hash_table_insert (profile_props,
			     g_strdup ("FILE_checksum"),
			     g_strdup (checksum));
	profile = cd_client_create_profile_sync (priv->client,
						 profile_id,
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
	g_free (checksum);
	g_free (profile_id);
	if (profile_props != NULL)
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
				      GcmSessionPrivate *priv)
{
	gboolean ret;
	GError *error = NULL;
	CdProfile *profile;

	/* find the ID for the filename */
	g_debug ("filename %s removed", filename);
	profile = cd_client_find_profile_by_filename_sync (priv->client,
							   filename,
							   NULL,
							   &error);
	if (profile == NULL) {
		g_warning ("%s", error->message);
		g_error_free (error);
		goto out;
	}

	/* remove it from colord */
	g_debug ("profile %s removed", cd_profile_get_id (profile));
	ret = cd_client_delete_profile_sync (priv->client,
					     cd_profile_get_id (profile),
					     NULL,
					     &error);
	if (!ret) {
		g_warning ("%s", error->message);
		g_error_free (error);
		goto out;
	}
out:
	if(profile != NULL)
		g_object_unref (profile);
}

/**
 * gcm_session_add_x11_output:
 **/
static void
gcm_session_add_x11_output (GcmSessionPrivate *priv, GcmX11Output *output)
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
		model = gcm_dmi_get_name (priv->dmi);
		vendor = gcm_dmi_get_vendor (priv->dmi);
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

	device_id = gcm_session_get_output_id (output);
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
	g_hash_table_insert (device_props,
			     g_strdup ("XRANDR_name"),
			     g_strdup (gcm_x11_output_get_name (output)));
	device = cd_client_create_device_sync (priv->client,
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
				GcmSessionPrivate *priv)
{
	gcm_session_add_x11_output (priv, output);
}

/**
 * gcm_x11_screen_output_removed_cb:
 **/
static void
gcm_x11_screen_output_removed_cb (GcmX11Screen *screen_,
				  GcmX11Output *output,
				  GcmSessionPrivate *priv)
{
	gboolean ret;
	GError *error = NULL;

	g_debug ("output %s removed",
		 gcm_x11_output_get_name (output));
	ret = cd_client_delete_device_sync (priv->client,
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
gcm_session_colord_appeared_cb (GDBusConnection *connection,
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
	GcmSessionPrivate *priv = (GcmSessionPrivate *) user_data;

	g_debug ("%s has appeared as %s", name, name_owner);

	/* add screens */
	ret = gcm_x11_screen_refresh (priv->x11_screen, &error);
	if (!ret) {
		g_warning ("failed to refresh: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* get X11 outputs */
	outputs = gcm_x11_screen_get_outputs (priv->x11_screen, &error);
	if (outputs == NULL) {
		g_warning ("failed to get outputs: %s", error->message);
		g_error_free (error);
		goto out;
	}
	for (i=0; i<outputs->len; i++) {
		output = g_ptr_array_index (outputs, i);
		gcm_session_add_x11_output (priv, output);
	}

	/* only connect when colord is awake */
	g_signal_connect (priv->x11_screen, "added",
			  G_CALLBACK (gcm_x11_screen_output_added_cb),
			  priv);
	g_signal_connect (priv->x11_screen, "removed",
			  G_CALLBACK (gcm_x11_screen_output_removed_cb),
			  priv);

	/* add profiles */
	gcm_profile_store_search (priv->profile_store);

	/* set for each device that already exist */
	array = cd_client_get_devices_sync (priv->client, NULL, &error);
	if (array == NULL) {
		g_warning ("failed to get devices: %s",
			   error->message);
		g_error_free (error);
		goto out;
	}
	for (i=0; i<array->len; i++) {
		device = g_ptr_array_index (array, i);
		gcm_session_device_assign (priv, device);
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
 * gcm_session_sensor_added_cb:
 **/
static void
gcm_session_sensor_added_cb (CdClient *client,
			     CdSensor *sensor,
			     GcmSessionPrivate *priv)
{
	gboolean ret;
	GError *error = NULL;

	ca_context_play (ca_gtk_context_get (), 0,
			 CA_PROP_EVENT_ID, "device-added",
			 /* TRANSLATORS: this is the application name for libcanberra */
			 CA_PROP_APPLICATION_NAME, _("GNOME Color Manager"),
			/* TRANSLATORS: this is a sound description */
			 CA_PROP_EVENT_DESCRIPTION, _("Sensor added"), NULL);

	/* open up the color prefs window */
	ret = g_spawn_command_line_async (BINDIR "/gcm-prefs",
					  &error);
	if (!ret) {
		g_warning ("failed to spawn: %s", error->message);
		g_error_free (error);
	}
}

/**
 * gcm_session_sensor_removed_cb:
 **/
static void
gcm_session_sensor_removed_cb (CdClient *client,
			       CdSensor *sensor,
			       GcmSessionPrivate *priv)
{
	ca_context_play (ca_gtk_context_get (), 0,
			 CA_PROP_EVENT_ID, "device-removed",
			 /* TRANSLATORS: this is the application name for libcanberra */
			 CA_PROP_APPLICATION_NAME, _("GNOME Color Manager"),
			/* TRANSLATORS: this is a sound description */
			 CA_PROP_EVENT_DESCRIPTION, _("Sensor removed"), NULL);
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

	/* watch to see when colord appears */
	priv->watcher_id = g_bus_watch_name (G_BUS_TYPE_SYSTEM,
					     "org.freedesktop.ColorManager",
					     G_BUS_NAME_WATCHER_FLAGS_NONE,
					     gcm_session_colord_appeared_cb,
					     gcm_session_colord_vanished_cb,
					     priv, NULL);
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

	priv = g_new0 (GcmSessionPrivate, 1);

	/* get the settings */
	priv->settings = g_settings_new (GCM_SETTINGS_SCHEMA);
	g_signal_connect (priv->settings, "changed",
			  G_CALLBACK (gcm_session_key_changed_cb), priv);

	/* use DMI data for internal panels */
	priv->dmi = gcm_dmi_new ();

	/* monitor daemon */
	priv->client = cd_client_new ();
	g_signal_connect (priv->client, "device-added",
			  G_CALLBACK (gcm_session_device_added_notify_cb),
			  priv);
	g_signal_connect (priv->client, "profile-added",
			  G_CALLBACK (gcm_session_profile_added_notify_cb),
			  priv);
	g_signal_connect (priv->client, "device-added",
			  G_CALLBACK (gcm_session_device_added_assign_cb),
			  priv);
	g_signal_connect (priv->client, "device-changed",
			  G_CALLBACK (gcm_session_device_changed_assign_cb),
			  priv);
	g_signal_connect (priv->client, "sensor-added",
			  G_CALLBACK (gcm_session_sensor_added_cb),
			  priv);
	g_signal_connect (priv->client, "sensor-removed",
			  G_CALLBACK (gcm_session_sensor_removed_cb),
			  priv);
	cd_client_connect (priv->client,
			   NULL,
			   gcm_session_client_connect_cb,
			   priv);

	/* have access to all profiles */
	priv->profile_store = gcm_profile_store_new ();
	g_signal_connect (priv->profile_store, "added",
			  G_CALLBACK (gcm_session_profile_store_added_cb),
			  priv);
	g_signal_connect (priv->profile_store, "removed",
			  G_CALLBACK (gcm_session_profile_store_removed_cb),
			  priv);

	/* monitor displays */
	priv->x11_screen = gcm_x11_screen_new ();
	ret = gcm_x11_screen_assign (priv->x11_screen, NULL, &error);
	if (!ret) {
		g_warning ("failed to assign: %s", error->message);
		g_error_free (error);
		goto out;
	}

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
		if (priv->watcher_id > 0)
			g_bus_unwatch_name (priv->watcher_id);
		if (priv->profile_store != NULL)
			g_object_unref (priv->profile_store);
		if (priv->dmi != NULL)
			g_object_unref (priv->dmi);
		if (priv->connection != NULL)
			g_object_unref (priv->connection);
		if (priv->x11_screen != NULL)
			g_object_unref (priv->x11_screen);
		g_dbus_node_info_unref (priv->introspection);
		g_object_unref (priv->settings);
		g_object_unref (priv->client);
		g_main_loop_unref (priv->loop);
		g_free (priv);
	}
	return retval;
}


/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010-2011 Richard Hughes <richard@hughsie.com>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <canberra-gtk.h>
#include <colord.h>

#include "gcm-cell-renderer-profile-text.h"
#include "gcm-cell-renderer-profile-icon.h"
#include "gcm-calibrate-argyll.h"
#include "gcm-cie-widget.h"
#include "gcm-sensor-client.h"
#include "gcm-exif.h"
#include "gcm-profile.h"
#include "gcm-trc-widget.h"
#include "gcm-utils.h"
#include "gcm-color.h"
#include "gcm-list-store-profiles.h"

#include "cc-color-panel.h"

struct _CcColorPanelPrivate {
	CdClient		*client;
	CdDevice		*current_device;
	gboolean		 setting_up_device;
	GCancellable		*cancellable;
	GcmSensorClient		*sensor_client;
	GSettings		*settings;
	GtkBuilder		*builder;
	GtkListStore		*list_store_devices;
	GtkListStore		*list_store_profiles;
	GtkWidget		*info_bar_profiles;
	GtkWidget		*main_window;
	guint			 apply_all_devices_id;
	guint			 save_and_apply_id;
};

G_DEFINE_DYNAMIC_TYPE (CcColorPanel, cc_color_panel, CC_TYPE_PANEL)

static void cc_color_panel_finalize (GObject *object);

#define CC_COLOR_PREFS_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_COLOR_PANEL, CcColorPanelPrivate))

#define PK_DBUS_SERVICE				"org.freedesktop.PackageKit"
#define PK_DBUS_PATH				"/org/freedesktop/PackageKit"
#define PK_DBUS_INTERFACE_QUERY			"org.freedesktop.PackageKit.Query"

enum {
	CD_DEVICES_COLUMN_ID,
	CD_DEVICES_COLUMN_SORT,
	CD_DEVICES_COLUMN_ICON,
	CD_DEVICES_COLUMN_TITLE,
	CD_DEVICES_COLUMN_DEVICE,
	CD_DEVICES_COLUMN_LAST
};

enum {
	GCM_PREFS_COMBO_COLUMN_TEXT,
	GCM_PREFS_COMBO_COLUMN_PROFILE,
	GCM_PREFS_COMBO_COLUMN_TYPE,
	GCM_PREFS_COMBO_COLUMN_SORTABLE,
	GCM_PREFS_COMBO_COLUMN_LAST
};

typedef enum {
	GCM_PREFS_ENTRY_TYPE_PROFILE,
	GCM_PREFS_ENTRY_TYPE_IMPORT,
	GCM_PREFS_ENTRY_TYPE_LAST
} GcmPrefsEntryType;

static void cc_color_panel_devices_treeview_clicked_cb (GtkTreeSelection *selection,
							CcColorPanel *panel);
static void cc_color_panel_profile_store_changed_cb (CdClient *client,
						     CcColorPanel *panel);

/**
 * cc_color_panel_error_dialog:
 **/
static void
cc_color_panel_error_dialog (CcColorPanel *panel,
			     const gchar *title,
			     const gchar *message)
{
	GtkWidget *dialog;

	dialog = gtk_message_dialog_new (GTK_WINDOW (panel->priv->main_window),
					 GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
					 GTK_BUTTONS_CLOSE, "%s", title);
	gtk_window_set_icon_name (GTK_WINDOW (dialog), GCM_STOCK_ICON);
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", message);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

/**
 * cc_color_panel_combobox_add_profile:
 **/
static void
cc_color_panel_combobox_add_profile (GtkWidget *widget,
				     CdProfile *profile,
				     GcmPrefsEntryType entry_type,
				     GtkTreeIter *iter)
{
	GtkTreeModel *model;
	GtkTreeIter iter_tmp;
	const gchar *description;
	gchar *sortable;

	/* iter is optional */
	if (iter == NULL)
		iter = &iter_tmp;

	/* use description */
	if (entry_type == GCM_PREFS_ENTRY_TYPE_IMPORT) {
		/* TRANSLATORS: this is where the user can click and import a profile */
		description = _("Other profile…");
		sortable = g_strdup ("9");
	} else {
		description = cd_profile_get_title (profile);
		sortable = g_strdup_printf ("5%s", description);
	}

	/* also add profile */
	model = gtk_combo_box_get_model (GTK_COMBO_BOX(widget));
	gtk_list_store_append (GTK_LIST_STORE(model), iter);
	gtk_list_store_set (GTK_LIST_STORE(model), iter,
			    GCM_PREFS_COMBO_COLUMN_TEXT, description,
			    GCM_PREFS_COMBO_COLUMN_PROFILE, profile,
			    GCM_PREFS_COMBO_COLUMN_TYPE, entry_type,
			    GCM_PREFS_COMBO_COLUMN_SORTABLE, sortable,
			    -1);
	g_free (sortable);
}

/**
 * cc_color_panel_default_cb:
 **/
static void
cc_color_panel_default_cb (GtkWidget *widget, CcColorPanel *panel)
{
	GPtrArray *array = NULL;
	CdDevice *device;
	CdProfile *profile;
	gboolean ret;
	guint i;
	GError *error = NULL;

	/* set for each output */
	array = cd_client_get_devices_sync (panel->priv->client,
					    panel->priv->cancellable,
					    &error);
	if (array == NULL) {
		g_warning ("failed to get devices: %s",
			   error->message);
		g_error_free (error);
		goto out;
	}
	for (i=0; i<array->len; i++) {
		device = g_ptr_array_index (array, i);

		/* TODO: check if the profile is already systemwide */
		profile = cd_device_get_default_profile (device);
		if (profile == NULL)
			continue;

		/* install somewhere out of $HOME */
		ret = cd_profile_install_system_wide_sync (profile,
							   panel->priv->cancellable,
							   &error);
		if (!ret) {
			g_warning ("failed to set profile system-wide: %s",
				   error->message);
			g_object_unref (profile);
			g_error_free (error);
			goto out;
		}
		g_object_unref (profile);
	}
out:
	if (array != NULL)
		g_ptr_array_unref (array);
}

/**
 * cc_color_panel_calibrate_display:
 **/
static gboolean
cc_color_panel_calibrate_display (CcColorPanel *panel,
				  GcmCalibrate *calibrate)
{
	gboolean ret = FALSE;
	GError *error = NULL;
	GtkWindow *window;

	/* run each task in order */
	window = GTK_WINDOW(panel->priv->main_window);
	ret = gcm_calibrate_display (calibrate, window, &error);
	if (!ret) {
		g_warning ("failed to calibrate: %s", error->message);
		g_error_free (error);
		goto out;
	}
out:
	return ret;
}

/**
 * cc_color_panel_calibrate_device:
 **/
static gboolean
cc_color_panel_calibrate_device (CcColorPanel *panel, GcmCalibrate *calibrate)
{
	gboolean ret = FALSE;
	GError *error = NULL;
	GtkWindow *window;

	/* do each step */
	window = GTK_WINDOW(panel->priv->main_window);
	ret = gcm_calibrate_device (calibrate, window, &error);
	if (!ret) {
		if (error->code != GCM_CALIBRATE_ERROR_USER_ABORT) {
			/* TRANSLATORS: could not calibrate */
			cc_color_panel_error_dialog (panel,
						     _("Failed to calibrate device"),
						     error->message);
		} else {
			g_warning ("failed to calibrate: %s", error->message);
		}
		g_error_free (error);
		goto out;
	}
out:
	return ret;
}

/**
 * cc_color_panel_calibrate_printer:
 **/
static gboolean
cc_color_panel_calibrate_printer (CcColorPanel *panel, GcmCalibrate *calibrate)
{
	gboolean ret = FALSE;
	GError *error = NULL;
	GtkWindow *window;

	/* do each step */
	window = GTK_WINDOW(panel->priv->main_window);
	ret = gcm_calibrate_printer (calibrate, window, &error);
	if (!ret) {
		if (error->code != GCM_CALIBRATE_ERROR_USER_ABORT) {
			/* TRANSLATORS: could not calibrate */
			cc_color_panel_error_dialog (panel,
						     _("Failed to calibrate printer"),
						     error->message);
		} else {
			g_warning ("failed to calibrate: %s", error->message);
		}
		g_error_free (error);
		goto out;
	}
out:
	return ret;
}

/**
 * cc_color_panel_file_chooser_get_icc_profile:
 **/
static GFile *
cc_color_panel_file_chooser_get_icc_profile (CcColorPanel *panel)
{
	GtkWindow *window;
	GtkWidget *dialog;
	GFile *file = NULL;
	GtkFileFilter *filter;

	/* create new dialog */
	window = GTK_WINDOW(gtk_builder_get_object (panel->priv->builder,
						    "dialog_assign"));
	/* TRANSLATORS: dialog for file->open dialog */
	dialog = gtk_file_chooser_dialog_new (_("Select ICC Profile File"), window,
					       GTK_FILE_CHOOSER_ACTION_OPEN,
					       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					       _("Import"), GTK_RESPONSE_ACCEPT,
					      NULL);
	gtk_window_set_icon_name (GTK_WINDOW (dialog), GCM_STOCK_ICON);
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER(dialog), g_get_home_dir ());
	gtk_file_chooser_set_create_folders (GTK_FILE_CHOOSER(dialog), FALSE);
	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER(dialog), FALSE);

	/* setup the filter */
	filter = gtk_file_filter_new ();
	gtk_file_filter_add_mime_type (filter, "application/vnd.iccprofile");

	/* we can remove this when we depend on a new shared-mime-info */
	gtk_file_filter_add_pattern (filter, "*.icc");
	gtk_file_filter_add_pattern (filter, "*.icm");
	gtk_file_filter_add_pattern (filter, "*.ICC");
	gtk_file_filter_add_pattern (filter, "*.ICM");

	/* TRANSLATORS: filter name on the file->open dialog */
	gtk_file_filter_set_name (filter, _("Supported ICC profiles"));
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER(dialog), filter);

	/* setup the all files filter */
	filter = gtk_file_filter_new ();
	gtk_file_filter_add_pattern (filter, "*");
	/* TRANSLATORS: filter name on the file->open dialog */
	gtk_file_filter_set_name (filter, _("All files"));
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER(dialog), filter);

	/* did user choose file */
	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
		file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER(dialog));

	/* we're done */
	gtk_widget_destroy (dialog);

	/* or NULL for missing */
	return file;
}

/**
 * cc_color_panel_profile_import_file:
 **/
static gboolean
cc_color_panel_profile_import_file (CcColorPanel *panel, GFile *file)
{
	gboolean ret;
	GError *error = NULL;
	GFile *destination = NULL;

	/* check if correct type */
	ret = gcm_utils_is_icc_profile (file);
	if (!ret) {
		g_debug ("not a ICC profile");
		goto out;
	}

	/* copy icc file to ~/.color/icc */
	destination = gcm_utils_get_profile_destination (file);
	ret = gcm_utils_mkdir_and_copy (file, destination, &error);
	if (!ret) {
		/* TRANSLATORS: could not read file */
		cc_color_panel_error_dialog (panel,
					     _("Failed to copy file"),
					     error->message);
		g_error_free (error);
		goto out;
	}
out:
	if (destination != NULL)
		g_object_unref (destination);
	return ret;
}

/**
 * cc_color_panel_drag_data_received_cb:
 **/
static void
cc_color_panel_drag_data_received_cb (GtkWidget *widget,
				      GdkDragContext *context,
				      gint x, gint y,
				      GtkSelectionData *data,
				      guint _time,
				      CcColorPanel *panel)
{
	const guchar *filename;
	gchar **filenames = NULL;
	GFile *file = NULL;
	guint i;
	gboolean ret;
	gboolean success = FALSE;

	/* get filenames */
	filename = gtk_selection_data_get_data (data);
	if (filename == NULL)
		goto out;

	/* import this */
	g_debug ("dropped: %p (%s)", data, filename);

	/* split, as multiple drag targets are accepted */
	filenames = g_strsplit_set ((const gchar *)filename, "\r\n", -1);
	for (i=0; filenames[i]!=NULL; i++) {

		/* blank entry */
		if (filenames[i][0] == '\0')
			continue;

		/* convert the URI */
		file = g_file_new_for_uri (filenames[i]);

		/* try to import it */
		ret = cc_color_panel_profile_import_file (panel, file);
		if (ret)
			success = TRUE;

		g_object_unref (file);
	}

out:
	gtk_drag_finish (context, success, FALSE, _time);
	g_strfreev (filenames);
}

/**
 * cc_color_panel_virtual_set_from_file:
 **/
static gboolean
cc_color_panel_virtual_set_from_file (CcColorPanel *panel, GFile *file)
{
	gboolean ret;
	GcmExif *exif;
	GError *error = NULL;
	const gchar *model;
	const gchar *manufacturer;
	GtkWidget *widget;

	/* parse file */
	exif = gcm_exif_new ();
	ret = gcm_exif_parse (exif, file, &error);
	if (!ret) {
		g_warning ("failed to parse file: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* set model and manufacturer */
	model = gcm_exif_get_model (exif);
	if (model != NULL) {
		widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
							     "entry_virtual_model"));
		gtk_entry_set_text (GTK_ENTRY (widget), model);
	}
	manufacturer = gcm_exif_get_manufacturer (exif);
	if (manufacturer != NULL) {
		widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "entry_virtual_manufacturer"));
		gtk_entry_set_text (GTK_ENTRY (widget), manufacturer);
	}

	/* set type */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "combobox_virtual_type"));
	gtk_combo_box_set_active (GTK_COMBO_BOX(widget), CD_DEVICE_KIND_CAMERA - 2);
out:
	g_object_unref (exif);
	return ret;
}

/**
 * cc_color_panel_virtual_drag_data_received_cb:
 **/
static void
cc_color_panel_virtual_drag_data_received_cb (GtkWidget *widget,
					      GdkDragContext *context,
					      gint x, gint y,
					      GtkSelectionData *data,
					      guint info,
					      guint _time,
					      CcColorPanel *panel)
{
	const guchar *filename;
	gchar **filenames = NULL;
	GFile *file = NULL;
	guint i;
	gboolean ret;

	g_return_if_fail (CC_IS_COLOR_PANEL (panel));

	/* get filenames */
	filename = gtk_selection_data_get_data (data);
	if (filename == NULL) {
		gtk_drag_finish (context, FALSE, FALSE, _time);
		goto out;
	}

	/* import this */
	g_debug ("dropped: %p (%s)", data, filename);

	/* split, as multiple drag targets are accepted */
	filenames = g_strsplit_set ((const gchar *)filename, "\r\n", -1);
	for (i=0; filenames[i]!=NULL; i++) {

		/* blank entry */
		if (filenames[i][0] == '\0')
			continue;

		/* check this is an ICC profile */
		g_debug ("trying to set %s", filenames[i]);
		file = g_file_new_for_uri (filenames[i]);
		ret = cc_color_panel_virtual_set_from_file (panel, file);
		if (!ret) {
			g_debug ("%s did not set from file correctly",
				 filenames[i]);
			gtk_drag_finish (context, FALSE, FALSE, _time);
			goto out;
		}
		g_object_unref (file);
		file = NULL;
	}

	gtk_drag_finish (context, TRUE, FALSE, _time);
out:
	if (file != NULL)
		g_object_unref (file);
	g_strfreev (filenames);
}

/**
 * cc_color_panel_ensure_argyllcms_installed:
 **/
static gboolean
cc_color_panel_ensure_argyllcms_installed (CcColorPanel *panel)
{
	gboolean ret;
	GtkWindow *window;
	GtkWidget *dialog;
	GtkResponseType response;
	GString *string = NULL;

	/* find whether argyllcms is installed using a tool which should exist */
	ret = g_file_test ("/usr/bin/dispcal", G_FILE_TEST_EXISTS);
	if (ret)
		goto out;

#ifndef HAVE_PACKAGEKIT
	g_warning ("cannot install: this package was not compiled with --enable-packagekit");
	goto out;
#endif

	/* ask the user to confirm */
	window = GTK_WINDOW(panel->priv->main_window);
	dialog = gtk_message_dialog_new (window, GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
					 /* TRANSLATORS: title, usually we can tell based on the EDID data or output name */
					 _("Install calibration and profiling software?"));

	string = g_string_new ("");
	/* TRANSLATORS: dialog message saying the argyllcms is not installed */
	g_string_append_printf (string, "%s\n",
				_("Calibration and profiling software is not installed."));
	/* TRANSLATORS: dialog message saying the color targets are not installed */
	g_string_append_printf (string, "%s",
				_("These tools are required to build color profiles for devices."));

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", string->str);
	gtk_window_set_icon_name (GTK_WINDOW (dialog), GCM_STOCK_ICON);
	/* TRANSLATORS: button, skip installing a package */
	gtk_dialog_add_button (GTK_DIALOG (dialog), _("Do not install"), GTK_RESPONSE_CANCEL);
	/* TRANSLATORS: button, install a package */
	gtk_dialog_add_button (GTK_DIALOG (dialog), _("Install"), GTK_RESPONSE_YES);
	response = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	/* only install if the user wanted to */
	if (response != GTK_RESPONSE_YES)
		goto out;

	/* do the install */
	ret = gcm_utils_install_package (GCM_PREFS_PACKAGE_NAME_ARGYLLCMS, window);
out:
	if (string != NULL)
		g_string_free (string, TRUE);
	return ret;
}

/**
 * cc_color_panel_wait_in_mainloop_quit_cb:
 **/
static gboolean
cc_color_panel_wait_in_mainloop_quit_cb (GMainLoop *loop)
{
	g_main_loop_quit (loop);
	return FALSE;
}

/**
 * cc_color_panel_wait_in_mainloop:
 **/
static void
cc_color_panel_wait_in_mainloop (guint timeout_ms)
{
	GMainLoop *loop;
	loop = g_main_loop_new (NULL, FALSE);
	g_timeout_add (timeout_ms,
		       (GSourceFunc) cc_color_panel_wait_in_mainloop_quit_cb,
		       loop);
	g_main_loop_run (loop);
	g_main_loop_unref (loop);
}

/**
 * cc_color_panel_calibrate_cb:
 **/
static void
cc_color_panel_calibrate_cb (GtkWidget *widget, CcColorPanel *panel)
{
	CdDeviceKind kind;
	CdProfile *profile = NULL;
	const gchar *filename;
	gchar *filename_dest = NULL;
	gboolean ret;
	gboolean success;
	gchar *destination = NULL;
	GcmCalibrate *calibrate = NULL;
	GError *error = NULL;
	GFile *dest = NULL;
	GFile *file = NULL;

	/* ensure argyllcms is installed */
	ret = cc_color_panel_ensure_argyllcms_installed (panel);
	if (!ret)
		goto out;

	/* create new calibration object */
	calibrate = GCM_CALIBRATE(gcm_calibrate_argyll_new ());

	/* set defaults from device */
	ret = gcm_calibrate_set_from_device (calibrate,
					     panel->priv->current_device,
					     &error);
	if (!ret) {
		g_warning ("failed to calibrate: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* mark device to be profiled in colord */
	ret = cd_device_profiling_inhibit_sync (panel->priv->current_device,
						panel->priv->cancellable,
						&error);
	if (!ret) {
		g_warning ("failed to profile inhibit: %s",
			   error->message);
		g_error_free (error);
		goto out;
	}

	/* choose the correct kind of calibration */
	kind = cd_device_get_kind (panel->priv->current_device);
	switch (kind) {
	case CD_DEVICE_KIND_DISPLAY:
		success = cc_color_panel_calibrate_display (panel, calibrate);
		break;
	case CD_DEVICE_KIND_SCANNER:
	case CD_DEVICE_KIND_CAMERA:
		success = cc_color_panel_calibrate_device (panel, calibrate);
		break;
	case CD_DEVICE_KIND_PRINTER:
		success = cc_color_panel_calibrate_printer (panel, calibrate);
		break;
	default:
		g_warning ("calibration and/or profiling not supported for this device");
		goto out;
	}

	/* unmark device to be profiled in colord */
	ret = cd_device_profiling_uninhibit_sync (panel->priv->current_device,
						  panel->priv->cancellable,
						  &error);
	if (!ret) {
		g_warning ("failed to profile uninhibit: %s",
			   error->message);
		g_error_free (error);
		goto out;
	}

	/* we failed to calibrate */
	if (!success) {
		g_warning ("failed to calibrate");
		goto out;
	}

	/* failed to get profile */
	filename = gcm_calibrate_get_filename_result (calibrate);
	if (filename == NULL) {
		g_warning ("failed to get filename from calibration");
		goto out;
	}

	/* copy the ICC file to the proper location */
	file = g_file_new_for_path (filename);
	dest = gcm_utils_get_profile_destination (file);
	ret = gcm_utils_mkdir_and_copy (file, dest, &error);
	if (!ret) {
		g_warning ("failed to calibrate: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* spin the mainloop, waiting for gcm-session to notice the new
	 * file and add it as a profile to colord */
	cc_color_panel_wait_in_mainloop (2000);

	/* add the new profile as the default */
	filename_dest = g_file_get_path (dest);
	profile = cd_client_find_profile_by_filename_sync (panel->priv->client,
							   filename_dest,
							   panel->priv->cancellable,
							   &error);
	if (profile == NULL) {
		g_warning ("failed to find calibration profile: %s",
			   error->message);
		g_error_free (error);
		goto out;
	}
	ret = cd_device_add_profile_sync (panel->priv->current_device,
					  CD_DEVICE_RELATION_HARD,
					  profile,
					  panel->priv->cancellable,
					  &error);
	if (!ret) {
		g_warning ("failed to add %s to %s: %s",
			   cd_profile_get_id (profile),
			   cd_device_get_id (panel->priv->current_device),
			   error->message);
		g_error_free (error);
		goto out;
	}

	/* remove temporary file */
	g_unlink (filename);

	/* play sound from the naming spec */
	ca_context_play (ca_gtk_context_get (), 0,
			 CA_PROP_EVENT_ID, "complete",
			 /* TRANSLATORS: this is the application name for libcanberra */
			 CA_PROP_APPLICATION_NAME, _("GNOME Color Manager"),
			 /* TRANSLATORS: this is the sound description */
			 CA_PROP_EVENT_DESCRIPTION, _("Profiling completed"), NULL);
out:
	g_free (destination);
	g_free (filename_dest);
	if (profile != NULL)
		g_object_unref (profile);
	if (calibrate != NULL)
		g_object_unref (calibrate);
	if (file != NULL)
		g_object_unref (file);
	if (dest != NULL)
		g_object_unref (dest);
}

/**
 * cc_color_panel_device_add_cb:
 **/
static void
cc_color_panel_device_add_cb (GtkWidget *widget, CcColorPanel *panel)
{
	/* show ui */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "dialog_virtual"));
	gtk_widget_show (widget);
	gtk_window_set_transient_for (GTK_WINDOW (widget),
				      GTK_WINDOW (panel->priv->main_window));

	/* clear entries */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "combobox_virtual_type"));
	gtk_combo_box_set_active (GTK_COMBO_BOX(widget), 0);
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "entry_virtual_model"));
	gtk_entry_set_text (GTK_ENTRY (widget), "");
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "entry_virtual_manufacturer"));
	gtk_entry_set_text (GTK_ENTRY (widget), "");
}

/**
 * cc_color_panel_is_profile_suitable_for_device:
 **/
static gboolean
cc_color_panel_is_profile_suitable_for_device (CdProfile *profile,
					       CdDevice *device)
{
	CdProfileKind profile_kind_tmp;
	CdProfileKind profile_kind;
	CdColorspace profile_colorspace;
	CdColorspace device_colorspace = 0;
	gboolean ret = FALSE;
	CdDeviceKind device_kind;

	/* not the right colorspace */
	device_colorspace = cd_device_get_colorspace (device);
	profile_colorspace = cd_profile_get_colorspace (profile);
	if (device_colorspace != profile_colorspace)
		goto out;

	/* not the correct kind */
	device_kind = cd_device_get_kind (device);
	profile_kind_tmp = cd_profile_get_kind (profile);
	profile_kind = gcm_utils_device_kind_to_profile_kind (device_kind);
	if (profile_kind_tmp != profile_kind)
		goto out;

	/* success */
	ret = TRUE;
out:
	return ret;
}

/**
 * cc_color_panel_add_profiles_suitable_for_devices:
 **/
static void
cc_color_panel_add_profiles_suitable_for_devices (CcColorPanel *panel,
						  GtkWidget *widget,
						  CdProfile *profile)
{
	CdProfile *profile_tmp;
	gboolean ret;
	GError *error = NULL;
	GPtrArray *profile_array = NULL;
	GtkTreeIter iter;
	GtkTreeModel *model;
	guint i;

	/* clear existing entries */
	model = gtk_combo_box_get_model (GTK_COMBO_BOX (widget));
	gtk_list_store_clear (GTK_LIST_STORE (model));

	/* get profiles */
	profile_array = cd_client_get_profiles_sync (panel->priv->client,
						     panel->priv->cancellable,
						     &error);
	if (profile_array == NULL) {
		g_warning ("failed to get profiles: %s",
			   error->message);
		g_error_free (error);
		goto out;
	}

	/* add profiles of the right kind */
	for (i=0; i<profile_array->len; i++) {
		profile_tmp = g_ptr_array_index (profile_array, i);

		/* don't add the current profile */
		if (profile != NULL &&
		    g_strcmp0 (cd_profile_get_id (profile),
			       cd_profile_get_id (profile_tmp)) == 0)
			continue;

		/* only add correct types */
		ret = cc_color_panel_is_profile_suitable_for_device (profile_tmp,
								     panel->priv->current_device);
		if (!ret)
			continue;

		/* add */
		cc_color_panel_combobox_add_profile (widget,
						     profile_tmp,
						     GCM_PREFS_ENTRY_TYPE_PROFILE,
						     &iter);
	}

	/* add a import entry */
	cc_color_panel_combobox_add_profile (widget, NULL, GCM_PREFS_ENTRY_TYPE_IMPORT, NULL);
	gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
out:
	if (profile_array != NULL)
		g_ptr_array_unref (profile_array);
}

/**
 * cc_color_panel_profile_add_cb:
 **/
static void
cc_color_panel_profile_add_cb (GtkWidget *widget, CcColorPanel *panel)
{
	CdProfile *profile = NULL;

	/* add profiles of the right kind */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "combobox_profile"));
	profile = cd_device_get_default_profile (panel->priv->current_device);
	cc_color_panel_add_profiles_suitable_for_devices (panel, widget, profile);

	/* show the dialog */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "dialog_assign"));
	gtk_widget_show (widget);
	gtk_window_set_transient_for (GTK_WINDOW (widget), GTK_WINDOW (panel->priv->main_window));
	if (profile != NULL)
		g_object_unref (profile);
}

/**
 * cc_color_panel_profile_remove_cb:
 **/
static void
cc_color_panel_profile_remove_cb (GtkWidget *widget, CcColorPanel *panel)
{
	GtkTreeIter iter;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	gboolean ret = FALSE;
	CdProfile *profile = NULL;
	GError *error = NULL;

	/* get the selected row */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "treeview_assign"));
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
		g_debug ("no row selected");
		goto out;
	}

	/* if the profile is default, then we'll have to make the first profile default */
	gtk_tree_model_get (model, &iter,
			    GCM_LIST_STORE_PROFILES_COLUMN_PROFILE, &profile,
			    -1);

	/* just remove it, the list store will get ::changed */
	ret = cd_device_remove_profile_sync (panel->priv->current_device,
					     profile,
					     panel->priv->cancellable,
					     &error);
	if (!ret) {
		g_warning ("failed to remove profile: %s", error->message);
		g_error_free (error);
		goto out;
	}
out:
	if (profile != NULL)
		g_object_unref (profile);
	return;
}

/**
 * cc_color_panel_profile_make_default_internal:
 **/
static void
cc_color_panel_profile_make_default_internal (CcColorPanel *panel,
					      GtkTreeModel *model,
					      GtkTreeIter *iter_selected)
{
	CdProfile *profile;
	GError *error = NULL;
	gboolean ret = FALSE;
	GtkWidget *widget;

	/* get currentlt selected item */
	gtk_tree_model_get (model, iter_selected,
			    GCM_LIST_STORE_PROFILES_COLUMN_PROFILE, &profile,
			    -1);

	/* just set it default */
	ret = cd_device_make_profile_default_sync (panel->priv->current_device,
						   profile,
						   panel->priv->cancellable,
						   &error);
	if (!ret) {
		g_warning ("failed to set default profile: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* set button insensitive */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "button_assign_make_default"));
	gtk_widget_set_sensitive (widget, FALSE);
out:
	g_object_unref (profile);
}

/**
 * cc_color_panel_profile_make_default_cb:
 **/
static void
cc_color_panel_profile_make_default_cb (GtkWidget *widget, CcColorPanel *panel)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	GtkTreeSelection *selection;

	/* get the selected row */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "treeview_assign"));
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
		g_debug ("no row selected");
		return;
	}

	/* make this profile the default */
	cc_color_panel_profile_make_default_internal (panel, model, &iter);
}

/**
 * cc_color_panel_button_virtual_add_cb:
 **/
static void
cc_color_panel_button_virtual_add_cb (GtkWidget *widget, CcColorPanel *panel)
{
	CdDeviceKind device_kind;
	CdDevice *device;
	const gchar *model;
	const gchar *manufacturer;
	gchar *device_id;
	GError *error = NULL;
	GHashTable *device_props;

	/* get device details */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "combobox_virtual_type"));
	device_kind = gtk_combo_box_get_active (GTK_COMBO_BOX(widget)) + 2;
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "entry_virtual_model"));
	model = gtk_entry_get_text (GTK_ENTRY (widget));
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "entry_virtual_manufacturer"));
	manufacturer = gtk_entry_get_text (GTK_ENTRY (widget));

	/* create device */
	device_id = g_strdup_printf ("%s-%s-%s",
				     cd_device_kind_to_string (device_kind),
				     manufacturer,
				     model);
	device_props = g_hash_table_new_full (g_str_hash, g_str_equal,
					      g_free, g_free);
	g_hash_table_insert (device_props,
			     g_strdup ("Kind"),
			     g_strdup (cd_device_kind_to_string (device_kind)));
	g_hash_table_insert (device_props,
			     g_strdup ("Mode"),
			     g_strdup (cd_device_mode_to_string (CD_DEVICE_MODE_VIRTUAL)));
	g_hash_table_insert (device_props,
			     g_strdup ("Colorspace"),
			     g_strdup (cd_colorspace_to_string (CD_COLORSPACE_RGB)));
	g_hash_table_insert (device_props,
			     g_strdup ("Model"),
			     g_strdup (model));
	g_hash_table_insert (device_props,
			     g_strdup ("Vendor"),
			     g_strdup (manufacturer));
	device = cd_client_create_device_sync (panel->priv->client,
					       device_id,
					       CD_OBJECT_SCOPE_DISK,
					       device_props,
					       panel->priv->cancellable,
					       &error);
	if (device == NULL) {
		/* TRANSLATORS: could not add virtual device */
		cc_color_panel_error_dialog (panel,
					     _("Failed to add create virtual device"),
					     error->message);
		g_error_free (error);
		goto out;
	}
out:
	g_hash_table_unref (device_props);
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "dialog_virtual"));
	gtk_widget_hide (widget);
	g_free (device_id);
}

/**
 * cc_color_panel_button_virtual_cancel_cb:
 **/
static void
cc_color_panel_button_virtual_cancel_cb (GtkWidget *widget, CcColorPanel *panel)
{
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "dialog_virtual"));
	gtk_widget_hide (widget);
}

/**
 * cc_color_panel_virtual_delete_event_cb:
 **/
static gboolean
cc_color_panel_virtual_delete_event_cb (GtkWidget *widget,
					GdkEvent *event,
					CcColorPanel *panel)
{
	cc_color_panel_button_virtual_cancel_cb (widget, panel);
	return TRUE;
}

/**
 * cc_color_panel_button_assign_cancel_cb:
 **/
static void
cc_color_panel_button_assign_cancel_cb (GtkWidget *widget, CcColorPanel *panel)
{
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "dialog_assign"));
	gtk_widget_hide (widget);
}

/**
 * cc_color_panel_button_assign_ok_cb:
 **/
static void
cc_color_panel_button_assign_ok_cb (GtkWidget *widget, CcColorPanel *panel)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	CdProfile *profile = NULL;
	gboolean ret = FALSE;
	GError *error = NULL;

	/* hide window */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "dialog_assign"));
	gtk_widget_hide (widget);

	/* get entry */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "combobox_profile"));
	ret = gtk_combo_box_get_active_iter (GTK_COMBO_BOX(widget), &iter);
	if (!ret)
		goto out;
	model = gtk_combo_box_get_model (GTK_COMBO_BOX(widget));
	gtk_tree_model_get (model, &iter,
			    GCM_PREFS_COMBO_COLUMN_PROFILE, &profile,
			    -1);

	/* just add it, the list store will get ::changed */
	ret = cd_device_add_profile_sync (panel->priv->current_device,
					  CD_DEVICE_RELATION_HARD,
					  profile,
					  panel->priv->cancellable,
					  &error);
	if (!ret) {
		g_warning ("failed to add: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* make it default */
	ret = cd_device_make_profile_default_sync (panel->priv->current_device,
						   profile,
						   panel->priv->cancellable,
						   &error);
	if (!ret) {
		g_warning ("failed to set default: %s", error->message);
		g_error_free (error);
		goto out;
	}
out:
	if (profile != NULL)
		g_object_unref (profile);
}

/**
 * cc_color_panel_profile_delete_event_cb:
 **/
static gboolean
cc_color_panel_profile_delete_event_cb (GtkWidget *widget,
					GdkEvent *event,
					CcColorPanel *panel)
{
	cc_color_panel_button_assign_cancel_cb (widget, panel);
	return TRUE;
}

/**
 * cc_color_panel_delete_cb:
 **/
static void
cc_color_panel_delete_cb (GtkWidget *widget, CcColorPanel *panel)
{
	gboolean ret = FALSE;
	GError *error = NULL;

	/* try to delete device */
	ret = cd_client_delete_device_sync (panel->priv->client,
					    cd_device_get_id (panel->priv->current_device),
					    panel->priv->cancellable,
					    &error);
	if (!ret) {
		/* TRANSLATORS: could not delete virtual device */
		cc_color_panel_error_dialog (panel, _("Failed to delete device"), error->message);
		g_error_free (error);
	}
}

/**
 * cc_color_panel_add_devices_columns:
 **/
static void
cc_color_panel_add_devices_columns (CcColorPanel *panel,
				    GtkTreeView *treeview)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	/* image */
	renderer = gtk_cell_renderer_pixbuf_new ();
	g_object_set (renderer, "stock-size", GTK_ICON_SIZE_DND, NULL);
	column = gtk_tree_view_column_new_with_attributes ("", renderer,
							   "icon-name", CD_DEVICES_COLUMN_ICON,
							   NULL);
	gtk_tree_view_append_column (treeview, column);

	/* column for text */
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer,
		      "wrap-mode", PANGO_WRAP_WORD,
		      NULL);
	column = gtk_tree_view_column_new_with_attributes ("", renderer,
							   "markup", CD_DEVICES_COLUMN_TITLE,
							   NULL);
	gtk_tree_view_column_set_sort_column_id (column, CD_DEVICES_COLUMN_SORT);
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (panel->priv->list_store_devices), CD_DEVICES_COLUMN_SORT, GTK_SORT_ASCENDING);
	gtk_tree_view_append_column (treeview, column);
	gtk_tree_view_column_set_expand (column, TRUE);
}

/**
 * cc_color_panel_add_assign_columns:
 **/
static void
cc_color_panel_add_assign_columns (CcColorPanel *panel,
				   GtkTreeView *treeview)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	/* column for text */
	renderer = gcm_cell_renderer_profile_text_new ();
	g_object_set (renderer,
		      "wrap-mode", PANGO_WRAP_WORD,
		      NULL);
	column = gtk_tree_view_column_new_with_attributes ("", renderer,
							   "profile", GCM_LIST_STORE_PROFILES_COLUMN_PROFILE,
							   "is-default", GCM_LIST_STORE_PROFILES_COLUMN_IS_DEFAULT,
							   NULL);
	gtk_tree_view_column_set_sort_column_id (column, GCM_LIST_STORE_PROFILES_COLUMN_SORT);
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (panel->priv->list_store_profiles), GCM_LIST_STORE_PROFILES_COLUMN_SORT, GTK_SORT_ASCENDING);
	gtk_tree_view_append_column (treeview, column);
	gtk_tree_view_column_set_expand (column, TRUE);

	/* column for icon */
	renderer = gcm_cell_renderer_profile_icon_new ();
	g_object_set (renderer, "stock-size", GTK_ICON_SIZE_BUTTON, NULL);
	column = gtk_tree_view_column_new_with_attributes ("", renderer,
							   "profile", GCM_LIST_STORE_PROFILES_COLUMN_PROFILE,
							   NULL);
	gtk_tree_view_append_column (treeview, column);
	gtk_tree_view_column_set_expand (column, TRUE);
}

/**
 * cc_color_panel_set_calibrate_button_sensitivity:
 **/
static void
cc_color_panel_set_calibrate_button_sensitivity (CcColorPanel *panel)
{
	gboolean ret = FALSE;
	GtkWidget *widget;
	const gchar *tooltip;
	CdDeviceKind kind;
	gboolean has_vte = TRUE;

	/* TRANSLATORS: this is when the button is sensitive */
	tooltip = _("Create a color profile for the selected device");

	/* no device selected */
	if (panel->priv->current_device == NULL) {
		/* TRANSLATORS: this is when the button is insensitive */
		tooltip = _("Cannot create profile: No device is selected");
		goto out;
	}

#ifndef HAVE_VTE
	has_vte = FALSE;
#endif

	/* no VTE support */
	if (!has_vte) {
		/* TRANSLATORS: this is when the button is insensitive because the distro compiled GCM without VTE */
		tooltip = _("Cannot create profile: Virtual console support is missing");
		goto out;
	}

	/* are we a display */
	kind = cd_device_get_kind (panel->priv->current_device);
	if (kind == CD_DEVICE_KIND_DISPLAY) {

		/* find whether we have hardware installed */
		ret = gcm_sensor_client_get_present (panel->priv->sensor_client);
		if (!ret) {
			/* TRANSLATORS: this is when the button is insensitive */
			tooltip = _("Cannot create profile: The measuring instrument is not plugged in");
			goto out;
		}
	} else if (kind == CD_DEVICE_KIND_SCANNER ||
		   kind == CD_DEVICE_KIND_CAMERA) {

		/* TODO: find out if we can scan using gnome-scan */
		ret = TRUE;

	} else if (kind == CD_DEVICE_KIND_PRINTER) {

		/* find whether we have hardware installed */
		ret = gcm_sensor_client_get_present (panel->priv->sensor_client);
		if (!ret) {
			/* TRANSLATORS: this is when the button is insensitive */
			tooltip = _("Cannot create profile: The measuring instrument is not plugged in");
			goto out;
		}

		/* find whether we have hardware installed */
		ret = gcm_sensor_supports_printer (gcm_sensor_client_get_sensor (panel->priv->sensor_client));
		if (!ret) {
			/* TRANSLATORS: this is when the button is insensitive */
			tooltip = _("Cannot create profile: The measuring instrument does not support printer profiling");
			goto out;
		}

	} else {

		/* TRANSLATORS: this is when the button is insensitive */
		tooltip = _("Cannot create a profile for this type of device");
	}
out:
	/* control the tooltip and sensitivity of the button */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "button_calibrate"));
	gtk_widget_set_tooltip_text (widget, tooltip);
	gtk_widget_set_sensitive (widget, ret);
}

/**
 * cc_color_panel_devices_treeview_clicked_cb:
 **/
static void
cc_color_panel_devices_treeview_clicked_cb (GtkTreeSelection *selection,
					    CcColorPanel *panel)
{
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkWidget *widget;
	GtkTreeIter iter;
	CdDeviceMode device_mode;

	/* This will only work in single or browse selection mode! */
	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
		g_debug ("no row selected");
		goto out;
	}

	/* get current device */
	if (panel->priv->current_device != NULL)
		g_object_unref (panel->priv->current_device);
	gtk_tree_model_get (model, &iter,
			    CD_DEVICES_COLUMN_DEVICE, &panel->priv->current_device,
			    -1);

	/* we have a new device */
	g_debug ("selected device is: %s",
		 cd_device_get_id (panel->priv->current_device));
	if (panel->priv->current_device == NULL)
		goto out;

	/* set new device */
	gcm_list_store_profiles_set_from_device (panel->priv->list_store_profiles,
						 panel->priv->current_device);

	/* select the default profile to display */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "treeview_assign"));
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
	path = gtk_tree_path_new_from_string ("0");
	gtk_tree_selection_select_path (selection, path);
	gtk_tree_path_free (path);

	/* make sure selectable */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "combobox_profile"));
	gtk_widget_set_sensitive (widget, TRUE);

	/* can we delete this device? */
	device_mode = cd_device_get_mode (panel->priv->current_device);
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "button_delete"));
	gtk_widget_set_sensitive (widget, device_mode == CD_DEVICE_MODE_VIRTUAL);

	/* can this device calibrate */
	cc_color_panel_set_calibrate_button_sensitivity (panel);
out:
	return;
}

/**
 * cc_color_panel_profile_treeview_row_activated_cb:
 **/
static void
cc_color_panel_profile_treeview_row_activated_cb (GtkTreeView *tree_view,
						  GtkTreePath *path,
						  GtkTreeViewColumn *column,
						  CcColorPanel *panel)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean ret;

	/* get the iter */
	model = GTK_TREE_MODEL (panel->priv->list_store_profiles);
	ret = gtk_tree_model_get_iter (model, &iter, path);
	if (!ret)
		return;

	/* make this profile the default */
	cc_color_panel_profile_make_default_internal (panel, model, &iter);
}

/**
 * cc_color_panel_profile_treeview_clicked_cb:
 **/
static void
cc_color_panel_profile_treeview_clicked_cb (GtkTreeSelection *selection,
					    CcColorPanel *panel)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean is_default;
	GtkWidget *widget;
	CdProfile *profile;
	CdDeviceRelation relation;

	/* This will only work in single or browse selection mode! */
	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {

		widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "button_assign_make_default"));
		gtk_widget_set_sensitive (widget, FALSE);
		widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "button_assign_remove"));
		gtk_widget_set_sensitive (widget, FALSE);

		g_debug ("no row selected");
		return;
	}

	/* get profile */
	gtk_tree_model_get (model, &iter,
			    GCM_LIST_STORE_PROFILES_COLUMN_PROFILE, &profile,
			    GCM_LIST_STORE_PROFILES_COLUMN_IS_DEFAULT, &is_default,
			    GCM_LIST_STORE_PROFILES_COLUMN_RELATION, &relation,
			    -1);
	g_debug ("selected profile = %s",
		 cd_profile_get_filename (profile));

	/* is the element the first in the list */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "button_assign_make_default"));
	gtk_widget_set_sensitive (widget, !is_default);

	/* we can only remove hard relationships */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "button_assign_remove"));
	if (relation == CD_DEVICE_RELATION_HARD) {
		gtk_widget_set_tooltip_text (widget, "");
		gtk_widget_set_sensitive (widget, TRUE);
	} else {
		/* TRANSLATORS: this is when an auto-added profile cannot be removed */
		gtk_widget_set_tooltip_text (widget, _("Cannot remove automatically added profile"));
		gtk_widget_set_sensitive (widget, FALSE);
	}
	g_object_unref (profile);
}

/**
 * cd_device_kind_to_string:
 **/
static const gchar *
cc_color_panel_device_kind_to_string (CdDeviceKind kind)
{
	if (kind == CD_DEVICE_KIND_DISPLAY)
		return "1";
	if (kind == CD_DEVICE_KIND_SCANNER)
		return "2";
	if (kind == CD_DEVICE_KIND_CAMERA)
		return "3";
	if (kind == CD_DEVICE_KIND_PRINTER)
		return "4";
	return "5";
}

/**
 * gcm_device_get_title:
 **/
static gchar *
gcm_device_get_title (CdDevice *device)
{
	const gchar *model;
	const gchar *vendor;

	/* try to geta nice string */
	vendor = cd_device_get_vendor (device);
	model = cd_device_get_model (device);
	if (vendor != NULL && vendor[0] != '\0' &&
	    model != NULL && model[0] != '\0')
		return g_strdup_printf ("%s - %s", vendor, model);
	if (model != NULL && model[0] != '\0')
		return g_strdup (model);
	if (vendor != NULL && vendor[0] != '\0')
		return g_strdup (vendor);
	return g_strdup (cd_device_get_id (device));
}

/**
 * cc_color_panel_set_combo_simple_text:
 **/
static void
cc_color_panel_set_combo_simple_text (GtkWidget *combo_box)
{
	GtkCellRenderer *renderer;
	GtkListStore *store;

	store = gtk_list_store_new (4,
				    G_TYPE_STRING,
				    CD_TYPE_PROFILE,
				    G_TYPE_UINT,
				    G_TYPE_STRING);
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store),
					      GCM_PREFS_COMBO_COLUMN_SORTABLE,
					      GTK_SORT_ASCENDING);
	gtk_combo_box_set_model (GTK_COMBO_BOX (combo_box),
				 GTK_TREE_MODEL (store));
	g_object_unref (store);

	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer,
		      "ellipsize", PANGO_ELLIPSIZE_END,
		      "wrap-mode", PANGO_WRAP_WORD_CHAR,
		      "width-chars", 60,
		      NULL);
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo_box), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo_box), renderer,
					"text", GCM_PREFS_COMBO_COLUMN_TEXT,
					NULL);
}

/**
 * cc_color_panel_profile_combo_changed_cb:
 **/
static void
cc_color_panel_profile_combo_changed_cb (GtkWidget *widget,
					 CcColorPanel *panel)
{
	GFile *file = NULL;
	gboolean ret;
	CdProfile *profile = NULL;
	GtkTreeIter iter;
	GtkTreeModel *model;
	GcmPrefsEntryType entry_type;

	/* no devices */
	if (panel->priv->current_device == NULL)
		return;

	/* no selection */
	ret = gtk_combo_box_get_active_iter (GTK_COMBO_BOX(widget), &iter);
	if (!ret)
		return;

	/* get entry */
	model = gtk_combo_box_get_model (GTK_COMBO_BOX(widget));
	gtk_tree_model_get (model, &iter,
			    GCM_PREFS_COMBO_COLUMN_TYPE, &entry_type,
			    -1);

	/* import */
	if (entry_type == GCM_PREFS_ENTRY_TYPE_IMPORT) {
		file = cc_color_panel_file_chooser_get_icc_profile (panel);
		if (file == NULL) {
			g_warning ("failed to get ICC file");
			gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
			goto out;
		}

		/* import this */
		ret = cc_color_panel_profile_import_file (panel, file);
		if (!ret) {
			gchar *uri;
			/* set to 'None' */
			gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);

			uri = g_file_get_uri (file);
			g_debug ("%s did not import correctly", uri);
			g_free (uri);
			goto out;
		}

		/* add to combobox */
		gtk_list_store_append (GTK_LIST_STORE(model), &iter);
		gtk_list_store_set (GTK_LIST_STORE(model), &iter,
				    GCM_PREFS_COMBO_COLUMN_PROFILE, profile,
				    GCM_PREFS_COMBO_COLUMN_SORTABLE, "0",
				    -1);
		gtk_combo_box_set_active_iter (GTK_COMBO_BOX (widget), &iter);
	}
out:
	if (file != NULL)
		g_object_unref (file);
	if (profile != NULL)
		g_object_unref (profile);
}

/**
 * cc_color_panel_sensor_client_changed_cb:
 **/
static void
cc_color_panel_sensor_client_changed_cb (GcmSensorClient *sensor_client,
					 CcColorPanel *panel)
{
	gboolean present;
	const gchar *event_id;
	const gchar *message;

	present = gcm_sensor_client_get_present (sensor_client);

	if (present) {
		/* TRANSLATORS: this is a sound description */
		message = _("Device added");
		event_id = "device-added";
	} else {
		/* TRANSLATORS: this is a sound description */
		message = _("Device removed");
		event_id = "device-removed";
	}

	/* play sound from the naming spec */
	ca_context_play (ca_gtk_context_get (), 0,
			 CA_PROP_EVENT_ID, event_id,
			 /* TRANSLATORS: this is the application name for libcanberra */
			 CA_PROP_APPLICATION_NAME, _("GNOME Color Manager"),
			 CA_PROP_EVENT_DESCRIPTION, message, NULL);

	cc_color_panel_set_calibrate_button_sensitivity (panel);
}

/**
 * cc_color_panel_device_kind_to_icon_name:
 **/
static const gchar *
cc_color_panel_device_kind_to_icon_name (CdDeviceKind kind)
{
	if (kind == CD_DEVICE_KIND_DISPLAY)
		return "video-display";
	if (kind == CD_DEVICE_KIND_SCANNER)
		return "scanner";
	if (kind == CD_DEVICE_KIND_PRINTER)
		return "printer";
	if (kind == CD_DEVICE_KIND_CAMERA)
		return "camera-photo";
	return "image-missing";
}


/**
 * cc_color_panel_add_device:
 **/
static void
cc_color_panel_add_device (CcColorPanel *panel, CdDevice *device)
{
	CdDeviceKind kind;
	const gchar *icon_name;
	const gchar *id;
	gchar *sort = NULL;
	gchar *title = NULL;
	GtkTreeIter iter;

	/* get icon */
	kind = cd_device_get_kind (device);
	icon_name = cc_color_panel_device_kind_to_icon_name (kind);

	/* italic for non-connected devices */
	title = gcm_device_get_title (device);

	/* create sort order */
	sort = g_strdup_printf ("%s%s",
				cc_color_panel_device_kind_to_string (kind),
				title);

	/* add to list */
	id = cd_device_get_id (device);
	g_debug ("add %s to device list", id);
	gtk_list_store_append (panel->priv->list_store_devices, &iter);
	gtk_list_store_set (panel->priv->list_store_devices, &iter,
			    CD_DEVICES_COLUMN_DEVICE, device,
			    CD_DEVICES_COLUMN_ID, id,
			    CD_DEVICES_COLUMN_SORT, sort,
			    CD_DEVICES_COLUMN_TITLE, title,
			    CD_DEVICES_COLUMN_ICON, icon_name, -1);
	g_free (sort);
	g_free (title);
}

/**
 * cc_color_panel_remove_device:
 **/
static void
cc_color_panel_remove_device (CcColorPanel *panel, CdDevice *cd_device)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	const gchar *id;
	gchar *id_tmp;
	gboolean ret;

	/* remove */
	id = cd_device_get_id (cd_device);

	/* get first element */
	model = GTK_TREE_MODEL (panel->priv->list_store_devices);
	ret = gtk_tree_model_get_iter_first (model, &iter);
	if (!ret)
		return;

	/* get the other elements */
	do {
		gtk_tree_model_get (model, &iter,
				    CD_DEVICES_COLUMN_ID, &id_tmp,
				    -1);
		if (g_strcmp0 (id_tmp, id) == 0) {
			gtk_list_store_remove (GTK_LIST_STORE(model), &iter);
			g_free (id_tmp);
			break;
		}
		g_free (id_tmp);
	} while (gtk_tree_model_iter_next (model, &iter));
}

/**
 * cc_color_panel_device_added_cb:
 **/
static void
cc_color_panel_device_added_cb (CdClient *client,
				CdDevice *device,
				CcColorPanel *panel)
{
	/* remove the saved device if it's already there */
	cc_color_panel_remove_device (panel, device);

	/* add the device */
	cc_color_panel_add_device (panel, device);
}

/**
 * cc_color_panel_changed_cb:
 **/
static void
cc_color_panel_changed_cb (CdClient *client,
			   CdDevice *device,
			   CcColorPanel *panel)
{
	g_debug ("changed: %s (doing nothing)", cd_device_get_id (device));
}

/**
 * cc_color_panel_device_removed_cb:
 **/
static void
cc_color_panel_device_removed_cb (CdClient *client,
				  CdDevice *device,
				  CcColorPanel *panel)
{
	GtkTreeIter iter;
	GtkTreeSelection *selection;
	GtkWidget *widget;
	gboolean ret;

	/* remove from the UI */
	cc_color_panel_remove_device (panel, device);

	/* select the first device */
	ret = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (panel->priv->list_store_devices), &iter);
	if (!ret)
		return;

	/* click it */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "treeview_devices"));
	gtk_tree_view_set_model (GTK_TREE_VIEW (widget),
				 GTK_TREE_MODEL (panel->priv->list_store_devices));
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
	gtk_tree_selection_select_iter (selection, &iter);
}

/**
 * cc_color_panel_setup_space_combobox:
 **/
static void
cc_color_panel_setup_space_combobox (CcColorPanel *panel,
				     GtkWidget *widget,
				     CdColorspace colorspace,
				     const gchar *profile_filename)
{
	CdColorspace colorspace_tmp;
	CdProfile *profile;
	const gchar *filename;
	gboolean has_colorspace_description;
	gboolean has_profile = FALSE;
	gboolean has_vcgt;
	gchar *text = NULL;
	GError *error = NULL;
	GPtrArray *profile_array = NULL;
	GtkTreeIter iter;
	GtkTreeModel *model;
	guint i;

	/* get profiles */
	profile_array = cd_client_get_profiles_sync (panel->priv->client,
						     panel->priv->cancellable,
						     &error);
	if (profile_array == NULL) {
		g_warning ("failed to get profiles: %s",
			   error->message);
		g_error_free (error);
		goto out;
	}

	/* update each list */
	for (i=0; i<profile_array->len; i++) {
		profile = g_ptr_array_index (profile_array, i);

		/* only for correct kind */
		has_vcgt = cd_profile_get_has_vcgt (profile);
		has_colorspace_description = gcm_profile_has_colorspace_description (profile);
		colorspace_tmp = cd_profile_get_colorspace (profile);
		if (!has_vcgt &&
		    colorspace == colorspace_tmp &&
		    (colorspace != CD_COLORSPACE_RGB ||
		     has_colorspace_description)) {
			cc_color_panel_combobox_add_profile (widget, profile, GCM_PREFS_ENTRY_TYPE_PROFILE, &iter);

			/* set active option */
			filename = cd_profile_get_filename (profile);
			if (g_strcmp0 (filename, profile_filename) == 0)
				gtk_combo_box_set_active_iter (GTK_COMBO_BOX (widget), &iter);
			has_profile = TRUE;
		}
	}
	if (!has_profile) {
		/* TRANSLATORS: this is when there are no profiles that
		 * can be used; the search term is either "RGB" or "CMYK" */
		text = g_strdup_printf (_("No %s color spaces available"),
					cd_colorspace_to_localised_string (colorspace));
		model = gtk_combo_box_get_model (GTK_COMBO_BOX (widget));
		gtk_list_store_append (GTK_LIST_STORE(model), &iter);
		gtk_list_store_set (GTK_LIST_STORE(model), &iter,
				    GCM_PREFS_COMBO_COLUMN_TEXT, text,
				    -1);
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
		gtk_widget_set_sensitive (widget, FALSE);
	}
out:
	if (profile_array != NULL)
		g_ptr_array_unref (profile_array);
	g_free (text);
}

/**
 * cc_color_panel_space_combo_changed_cb:
 **/
static void
cc_color_panel_space_combo_changed_cb (GtkWidget *widget, CcColorPanel *panel)
{
	gboolean ret;
	GtkTreeIter iter;
	const gchar *filename;
	GtkTreeModel *model;
	CdProfile *profile = NULL;
	const gchar *key = g_object_get_data (G_OBJECT(widget), "GCM:GSettingsKey");

	/* no selection */
	ret = gtk_combo_box_get_active_iter (GTK_COMBO_BOX(widget), &iter);
	if (!ret)
		return;

	/* get profile */
	model = gtk_combo_box_get_model (GTK_COMBO_BOX(widget));
	gtk_tree_model_get (model, &iter,
			    GCM_PREFS_COMBO_COLUMN_PROFILE, &profile,
			    -1);
	if (profile == NULL)
		goto out;

	filename = cd_profile_get_filename (profile);
	g_debug ("changed working space %s", filename);
	g_settings_set_string (panel->priv->settings, key, filename);
out:
	if (profile != NULL)
		g_object_unref (profile);
}

/**
 * cc_color_panel_renderer_combo_changed_cb:
 **/
static void
cc_color_panel_renderer_combo_changed_cb (GtkWidget *widget, CcColorPanel *panel)
{
	gint active;
	const gchar *key = g_object_get_data (G_OBJECT(widget), "GCM:GSettingsKey");

	/* no selection */
	active = gtk_combo_box_get_active (GTK_COMBO_BOX(widget));
	if (active == -1)
		return;

	/* save to GSettings */
	g_debug ("changed rendering intent to %s", cd_rendering_intent_to_string (active+1));
	g_settings_set_enum (panel->priv->settings, key, active+1);
}

/**
 * cc_color_panel_setup_rendering_combobox:
 **/
static void
cc_color_panel_setup_rendering_combobox (GtkWidget *widget, CdRenderingIntent intent)
{
	guint i;
	gboolean ret = FALSE;
	gchar *label;

	for (i=1; i<CD_RENDERING_INTENT_LAST; i++) {
		label = g_strdup_printf ("%s - %s",
					 cd_rendering_intent_to_localized_text (i),
					 cd_rendering_intent_to_localized_description (i));
		gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (widget), label);
		g_free (label);
		if (i == intent) {
			ret = TRUE;
			gtk_combo_box_set_active (GTK_COMBO_BOX (widget), i-1);
		}
	}
	/* nothing matches, just set the first option */
	if (!ret)
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
}

/**
 * cc_color_panel_is_color_profiles_extra_installed_ready_cb:
 **/
static void
cc_color_panel_is_color_profiles_extra_installed_ready_cb (GObject *source_object,
							   GAsyncResult *res,
							   gpointer user_data)
{
	GVariant *response = NULL;
	GError *error = NULL;
	gboolean installed = TRUE;
	CcColorPanel *panel = CC_COLOR_PANEL (user_data);

	/* get details */
	response = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source_object),
						  res, &error);
	if (response == NULL) {
		/* TRANSLATORS: the DBus method failed */
		g_warning ("%s %s\n", _("The request failed:"),
			   error->message);
		g_error_free (error);
		goto out;
	}

	/* get value */
	g_variant_get (response, "(b)", &installed);

	/* show control */
	gtk_widget_set_visible (panel->priv->info_bar_profiles, !installed);
out:
	if (response != NULL)
		g_variant_unref (response);
}

/**
 * cc_color_panel_is_color_profiles_extra_installed:
 **/
static void
cc_color_panel_is_color_profiles_extra_installed (CcColorPanel *panel)
{
	GDBusConnection *connection;
	GVariant *args = NULL;
	GError *error = NULL;

#ifndef HAVE_PACKAGEKIT
	g_warning ("cannot query %s: this package was not compiled with --enable-packagekit",
		   package_name);
	return;
#endif

	/* get a session bus connection */
	connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
	if (connection == NULL) {
		/* TRANSLATORS: no DBus session bus */
		g_print ("%s %s\n", _("Failed to connect to session bus:"),
			 error->message);
		g_error_free (error);
		goto out;
	}

	/* execute sync method */
	args = g_variant_new ("(ss)",
			      GCM_PREFS_PACKAGE_NAME_COLOR_PROFILES_EXTRA,
			      "timeout=5");
	g_dbus_connection_call (connection,
				PK_DBUS_SERVICE,
				PK_DBUS_PATH,
				PK_DBUS_INTERFACE_QUERY,
				"IsInstalled",
				args,
				G_VARIANT_TYPE ("(b)"),
				G_DBUS_CALL_FLAGS_NONE,
				G_MAXINT,
				panel->priv->cancellable,
				cc_color_panel_is_color_profiles_extra_installed_ready_cb,
				panel);
out:
	if (args != NULL)
		g_variant_unref (args);
	return;
}

/**
 * cc_color_panel_startup_idle_cb:
 **/
static gboolean
cc_color_panel_startup_idle_cb (CcColorPanel *panel)
{
	gchar *colorspace_cmyk;
	gchar *colorspace_gray;
	gchar *colorspace_rgb;
	gint intent_display = -1;
	gint intent_softproof = -1;
	GtkWidget *widget;

	/* search the disk for profiles */
	g_signal_connect (panel->priv->client, "changed",
			  G_CALLBACK(cc_color_panel_profile_store_changed_cb), panel);

	/* setup RGB combobox */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "combobox_space_rgb"));
	colorspace_rgb = g_settings_get_string (panel->priv->settings,
						 GCM_SETTINGS_COLORSPACE_RGB);
	cc_color_panel_set_combo_simple_text (widget);
	cc_color_panel_setup_space_combobox (panel, widget,
					     CD_COLORSPACE_RGB,
					     colorspace_rgb);
	g_object_set_data (G_OBJECT(widget), "GCM:GSettingsKey",
			   (gpointer) GCM_SETTINGS_COLORSPACE_RGB);
	g_signal_connect (G_OBJECT (widget), "changed",
			  G_CALLBACK (cc_color_panel_space_combo_changed_cb), panel);

	/* setup CMYK combobox */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "combobox_space_cmyk"));
	colorspace_cmyk = g_settings_get_string (panel->priv->settings,
						 GCM_SETTINGS_COLORSPACE_CMYK);
	cc_color_panel_set_combo_simple_text (widget);
	cc_color_panel_setup_space_combobox (panel, widget,
					     CD_COLORSPACE_CMYK,
					     colorspace_cmyk);
	g_object_set_data (G_OBJECT(widget), "GCM:GSettingsKey",
			   (gpointer) GCM_SETTINGS_COLORSPACE_CMYK);
	g_signal_connect (G_OBJECT (widget), "changed",
			  G_CALLBACK (cc_color_panel_space_combo_changed_cb), panel);

	/* setup gray combobox */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "combobox_space_gray"));
	colorspace_gray = g_settings_get_string (panel->priv->settings,
						 GCM_SETTINGS_COLORSPACE_GRAY);
	cc_color_panel_set_combo_simple_text (widget);
	cc_color_panel_setup_space_combobox (panel, widget,
					     CD_COLORSPACE_GRAY,
					     colorspace_gray);
	g_object_set_data (G_OBJECT(widget), "GCM:GSettingsKey",
			   (gpointer) GCM_SETTINGS_COLORSPACE_GRAY);
	g_signal_connect (G_OBJECT (widget), "changed",
			  G_CALLBACK (cc_color_panel_space_combo_changed_cb), panel);

	/* setup rendering lists */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "combobox_rendering_display"));
	intent_display = g_settings_get_enum (panel->priv->settings,
					      GCM_SETTINGS_RENDERING_INTENT_DISPLAY);
	cc_color_panel_setup_rendering_combobox (widget, intent_display);
	g_object_set_data (G_OBJECT(widget), "GCM:GSettingsKey",
			   (gpointer) GCM_SETTINGS_RENDERING_INTENT_DISPLAY);
	g_signal_connect (G_OBJECT (widget), "changed",
			  G_CALLBACK (cc_color_panel_renderer_combo_changed_cb), panel);

	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "combobox_rendering_softproof"));
	intent_softproof = g_settings_get_enum (panel->priv->settings,
						GCM_SETTINGS_RENDERING_INTENT_SOFTPROOF);
	cc_color_panel_setup_rendering_combobox (widget, intent_softproof);
	g_object_set_data (G_OBJECT(widget), "GCM:GSettingsKey",
			   (gpointer) GCM_SETTINGS_RENDERING_INTENT_SOFTPROOF);
	g_signal_connect (G_OBJECT (widget), "changed",
			  G_CALLBACK (cc_color_panel_renderer_combo_changed_cb), panel);

	/* set calibrate button sensitivity */
	cc_color_panel_set_calibrate_button_sensitivity (panel);

	/* we're probably showing now */
	panel->priv->main_window = gtk_widget_get_toplevel (panel->priv->info_bar_profiles);

	/* do we show the shared-color-profiles-extra installer? */
	g_debug ("getting installed");
	cc_color_panel_is_color_profiles_extra_installed (panel);

	g_free (colorspace_rgb);
	g_free (colorspace_cmyk);
	return FALSE;
}

/**
 * cc_color_panel_setup_drag_and_drop:
 **/
static void
cc_color_panel_setup_drag_and_drop (GtkWidget *widget)
{
	GtkTargetEntry entry;

	/* setup a dummy entry */
	entry.target = g_strdup ("text/plain");
	entry.flags = GTK_TARGET_OTHER_APP;
	entry.info = 0;

	gtk_drag_dest_set (widget,
			   GTK_DEST_DEFAULT_ALL,
			   &entry,
			   1,
			   GDK_ACTION_MOVE | GDK_ACTION_COPY);
	g_free (entry.target);
}

/**
 * cc_color_panel_profile_store_changed_cb:
 **/
static void
cc_color_panel_profile_store_changed_cb (CdClient *client, CcColorPanel *panel)
{
	GtkTreeSelection *selection;
	GtkWidget *widget;

	/* re-get all the profiles for this device */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "treeview_devices"));
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
	if (selection == NULL)
		return;
	g_signal_emit_by_name (selection, "changed", panel);
}

/**
 * cc_color_panel_get_devices_cb:
 **/
static void
cc_color_panel_get_devices_cb (GObject *object,
			       GAsyncResult *res,
			       gpointer user_data)
{
	CcColorPanel *panel = (CcColorPanel *) user_data;
	CdClient *client = CD_CLIENT (object);
	CdDevice *device;
	GError *error = NULL;
	GPtrArray *devices;
	GtkTreePath *path;
	GtkWidget *widget;
	guint i;

	/* get devices and add them */
	devices = cd_client_get_devices_finish (client, res, &error);
	if (devices == NULL) {
		g_warning ("failed to add connected devices: %s",
			   error->message);
		g_error_free (error);
		goto out;
	}
	for (i=0; i<devices->len; i++) {
		device = g_ptr_array_index (devices, i);
		cc_color_panel_add_device (panel, device);
	}

	/* set the cursor on the first device */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "treeview_devices"));
	path = gtk_tree_path_new_from_string ("0");
	gtk_tree_view_set_cursor (GTK_TREE_VIEW (widget), path, NULL, FALSE);
	gtk_tree_path_free (path);
out:
	if (devices != NULL)
		g_ptr_array_unref (devices);
}

/**
 * cc_color_panel_info_bar_response_cb:
 **/
static void
cc_color_panel_info_bar_response_cb (GtkDialog *dialog,
				     GtkResponseType response,
				     CcColorPanel *panel)
{
	GtkWindow *window;
	gboolean ret;

	if (response == GTK_RESPONSE_APPLY) {
		/* install the extra profiles */
		window = GTK_WINDOW(panel->priv->main_window);
		ret = gcm_utils_install_package (GCM_PREFS_PACKAGE_NAME_COLOR_PROFILES_EXTRA,
						 window);
		if (ret)
			gtk_widget_hide (panel->priv->info_bar_profiles);
	}
}

/**
 * cd_device_kind_to_localised_string:
 **/
static const gchar *
cd_device_kind_to_localised_string (CdDeviceKind device_kind)
{
	if (device_kind == CD_DEVICE_KIND_DISPLAY) {
		/* TRANSLATORS: device type */
		return _("Display");
	}
	if (device_kind == CD_DEVICE_KIND_SCANNER) {
		/* TRANSLATORS: device type */
		return _("Scanner");
	}
	if (device_kind == CD_DEVICE_KIND_PRINTER) {
		/* TRANSLATORS: device type */
		return _("Printer");
	}
	if (device_kind == CD_DEVICE_KIND_CAMERA) {
		/* TRANSLATORS: device type */
		return _("Camera");
	}
	return NULL;
}

/**
 * cc_color_panel_setup_virtual_combobox:
 **/
static void
cc_color_panel_setup_virtual_combobox (GtkWidget *widget)
{
	guint i;
	const gchar *text;

	for (i=CD_DEVICE_KIND_SCANNER; i<CD_DEVICE_KIND_LAST; i++) {
		text = cd_device_kind_to_localised_string (i);
		gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT(widget), text);
	}
	gtk_combo_box_set_active (GTK_COMBO_BOX (widget), CD_DEVICE_KIND_PRINTER - 2);
}

/**
 * gpk_update_viewer_notify_network_state_cb:
 **/
static void
cc_color_panel_button_virtual_entry_changed_cb (GtkEntry *entry,
						GParamSpec *pspec,
						CcColorPanel *panel)
{
	const gchar *model;
	const gchar *manufacturer;
	GtkWidget *widget;

	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "entry_virtual_model"));
	model = gtk_entry_get_text (GTK_ENTRY (widget));
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "entry_virtual_manufacturer"));
	manufacturer = gtk_entry_get_text (GTK_ENTRY (widget));

	/* only set the add button sensitive if both sections have text */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "button_virtual_add"));
	gtk_widget_set_sensitive (widget,
				 (model != NULL &&
				  model[0] != '\0' &&
				  manufacturer != NULL &&
				  manufacturer[0] != '\0'));
}

static void
cc_color_panel_class_init (CcColorPanelClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	g_type_class_add_private (klass, sizeof (CcColorPanelPrivate));
	object_class->finalize = cc_color_panel_finalize;
}

static void
cc_color_panel_class_finalize (CcColorPanelClass *klass)
{
}

static gboolean
cc_color_panel_activate_link_cb (GtkLabel *label,
				 const gchar *uri,
				 CcColorPanel *panel)
{
	gboolean ret;
	GError *error = NULL;
	guint xid;
	gchar *command;

	/* get xid */
	xid = gdk_x11_window_get_xid (gtk_widget_get_window (GTK_WIDGET (panel->priv->main_window)));

	/* run with modal set */
	command = g_strdup_printf ("%s/gcm-viewer --parent-window %u", BINDIR, xid);
	g_debug ("running: %s", command);
	ret = g_spawn_command_line_async (command, &error);
	if (!ret) {
		g_warning ("failed to run prefs: %s", error->message);
		g_error_free (error);
	}
	g_free (command);
	return TRUE;
}

static void
cc_color_panel_finalize (GObject *object)
{
	CcColorPanel *panel = CC_COLOR_PANEL (object);

	g_cancellable_cancel (panel->priv->cancellable);
	g_object_unref (panel->priv->cancellable);
	if (panel->priv->current_device != NULL)
		g_object_unref (panel->priv->current_device);
	if (panel->priv->sensor_client != NULL)
		g_object_unref (panel->priv->sensor_client);
	if (panel->priv->settings != NULL)
		g_object_unref (panel->priv->settings);
	if (panel->priv->builder != NULL)
		g_object_unref (panel->priv->builder);
	if (panel->priv->client != NULL)
		g_object_unref (panel->priv->client);
	if (panel->priv->save_and_apply_id != 0)
		g_source_remove (panel->priv->save_and_apply_id);
	if (panel->priv->apply_all_devices_id != 0)
		g_source_remove (panel->priv->apply_all_devices_id);

	G_OBJECT_CLASS (cc_color_panel_parent_class)->finalize (object);
}

static void
cc_color_panel_init (CcColorPanel *panel)
{
	gboolean ret;
	GError *error = NULL;
	gint retval;
	GtkTreeSelection *selection;
	GtkWidget *info_bar_profiles_label;
	GtkWidget *main_window;
	GtkWidget *widget;
	gchar *text = NULL;

	panel->priv = CC_COLOR_PREFS_GET_PRIVATE (panel);
	panel->priv->cancellable = g_cancellable_new ();

	/* setup defaults */
	panel->priv->settings = g_settings_new (GCM_SETTINGS_SCHEMA);

	/* get UI */
	panel->priv->builder = gtk_builder_new ();
	gtk_builder_set_translation_domain (self->priv->builder, GETTEXT_PACKAGE);
	retval = gtk_builder_add_from_file (panel->priv->builder,
					    GCM_DATA "/gcm-prefs.ui",
					    &error);
	if (retval == 0) {
		g_error ("failed to load ui: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* reparent */
	main_window = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
							  "dialog_prefs"));
	widget = gtk_dialog_get_content_area (GTK_DIALOG (main_window));
	gtk_widget_reparent (widget, GTK_WIDGET (panel));

	/* add application specific icons to search path */
	gtk_icon_theme_append_search_path (gtk_icon_theme_get_default (),
	                                   GCM_DATA G_DIR_SEPARATOR_S "icons");

	/* create list stores */
	panel->priv->list_store_devices = gtk_list_store_new (CD_DEVICES_COLUMN_LAST,
							      G_TYPE_STRING,
							      G_TYPE_STRING,
							      G_TYPE_STRING,
							      G_TYPE_STRING,
							      CD_TYPE_DEVICE);
	panel->priv->list_store_profiles = gcm_list_store_profiles_new ();

	/* assign buttons */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "button_assign_add"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (cc_color_panel_profile_add_cb), panel);
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "button_assign_remove"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (cc_color_panel_profile_remove_cb), panel);
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "button_assign_make_default"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (cc_color_panel_profile_make_default_cb), panel);

	/* create device tree view */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "treeview_devices"));
	gtk_tree_view_set_model (GTK_TREE_VIEW (widget),
				 GTK_TREE_MODEL (panel->priv->list_store_devices));
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
	g_signal_connect (selection, "changed",
			  G_CALLBACK (cc_color_panel_devices_treeview_clicked_cb),
			  panel);

	/* add columns to the tree view */
	cc_color_panel_add_devices_columns (panel, GTK_TREE_VIEW (widget));
	gtk_tree_view_columns_autosize (GTK_TREE_VIEW (widget));

	/* create assign tree view */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "treeview_assign"));
	gtk_tree_view_set_model (GTK_TREE_VIEW (widget),
				 GTK_TREE_MODEL (panel->priv->list_store_profiles));
	g_signal_connect (GTK_TREE_VIEW (widget), "row-activated",
			  G_CALLBACK (cc_color_panel_profile_treeview_row_activated_cb),
			  panel);
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
	g_signal_connect (selection, "changed",
			  G_CALLBACK (cc_color_panel_profile_treeview_clicked_cb),
			  panel);

	/* add columns to the tree view */
	cc_color_panel_add_assign_columns (panel, GTK_TREE_VIEW (widget));
	gtk_tree_view_columns_autosize (GTK_TREE_VIEW (widget));
	gtk_tree_view_set_reorderable (GTK_TREE_VIEW (widget), TRUE);
	gtk_tree_view_set_tooltip_column (GTK_TREE_VIEW (widget),
					  GCM_LIST_STORE_PROFILES_COLUMN_TOOLTIP);

	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "button_default"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (cc_color_panel_default_cb), panel);
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "label_viewer"));
	g_signal_connect (widget, "activate-link",
			  G_CALLBACK (cc_color_panel_activate_link_cb), panel);
	/* TRANSLATORS: link to gcm-viewer */
	text = g_strdup_printf ("<a href=\"moo\">%s</a>",
				_("Compare profiles..."));
	gtk_label_set_markup (GTK_LABEL (widget), text);
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "button_delete"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (cc_color_panel_delete_cb), panel);
	gtk_widget_set_sensitive (widget, FALSE);
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "button_device_add"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (cc_color_panel_device_add_cb), panel);
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "button_calibrate"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (cc_color_panel_calibrate_cb), panel);

	/* set up virtual dialog */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "dialog_virtual"));
	g_signal_connect (widget, "delete-event",
			  G_CALLBACK (cc_color_panel_virtual_delete_event_cb),
			  panel);
	g_signal_connect (widget, "drag-data-received",
			  G_CALLBACK (cc_color_panel_virtual_drag_data_received_cb),
			  panel);
	cc_color_panel_setup_drag_and_drop (widget);

	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "button_virtual_add"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (cc_color_panel_button_virtual_add_cb),
			  panel);
	gtk_widget_set_sensitive (widget, FALSE);

	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "button_virtual_cancel"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (cc_color_panel_button_virtual_cancel_cb),
			  panel);
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "combobox_virtual_type"));
	cc_color_panel_setup_virtual_combobox (widget);

	/* set up assign dialog */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "dialog_assign"));
	g_signal_connect (widget, "delete-event",
			  G_CALLBACK (cc_color_panel_profile_delete_event_cb),
			  panel);
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "button_assign_cancel"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (cc_color_panel_button_assign_cancel_cb),
			  panel);
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "button_assign_ok"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (cc_color_panel_button_assign_ok_cb),
			  panel);

	/* disable the add button if nothing in either box */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "entry_virtual_model"));
	g_signal_connect (widget, "notify::text",
			  G_CALLBACK (cc_color_panel_button_virtual_entry_changed_cb),
			  panel);
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "entry_virtual_manufacturer"));
	g_signal_connect (widget, "notify::text",
			  G_CALLBACK (cc_color_panel_button_virtual_entry_changed_cb),
			  panel);

	/* setup icc profiles list */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "combobox_profile"));
	cc_color_panel_set_combo_simple_text (widget);
	gtk_widget_set_sensitive (widget, FALSE);
	g_signal_connect (G_OBJECT (widget), "changed",
			  G_CALLBACK (cc_color_panel_profile_combo_changed_cb),
			  panel);

	/* use a device client array */
	panel->priv->client = cd_client_new ();
	g_signal_connect (panel->priv->client, "device-added",
			  G_CALLBACK (cc_color_panel_device_added_cb),
			  panel);
	g_signal_connect (panel->priv->client, "device-removed",
			  G_CALLBACK (cc_color_panel_device_removed_cb),
			  panel);
	g_signal_connect (panel->priv->client, "changed",
			  G_CALLBACK (cc_color_panel_changed_cb),
			  panel);

	/* connect to colord */
	ret = cd_client_connect_sync (panel->priv->client,
				      panel->priv->cancellable,
				      &error);
	if (!ret) {
		g_warning ("failed to connect to colord: %s",
			   error->message);
		g_error_free (error);
		goto out;
	}

	/* get devices async */
	cd_client_get_devices (panel->priv->client,
			       panel->priv->cancellable,
			       cc_color_panel_get_devices_cb,
			       panel);

	/* use the color device */
	panel->priv->sensor_client = gcm_sensor_client_new ();
	g_signal_connect (panel->priv->sensor_client, "changed",
			  G_CALLBACK (cc_color_panel_sensor_client_changed_cb),
			  panel);

	/* use infobar */
	panel->priv->info_bar_profiles = gtk_info_bar_new ();
	g_signal_connect (panel->priv->info_bar_profiles, "response",
			  G_CALLBACK (cc_color_panel_info_bar_response_cb),
			  panel);

	/* TRANSLATORS: button to install extra profiles */
	gtk_info_bar_add_button (GTK_INFO_BAR(panel->priv->info_bar_profiles),
				 _("Install now"), GTK_RESPONSE_APPLY);

	/* TRANSLATORS: this is displayed when the profile is crap */
	info_bar_profiles_label = gtk_label_new (_("More color profiles could be automatically installed."));
	gtk_label_set_line_wrap (GTK_LABEL (info_bar_profiles_label), TRUE);
	gtk_info_bar_set_message_type (GTK_INFO_BAR(panel->priv->info_bar_profiles),
				       GTK_MESSAGE_INFO);
	widget = gtk_info_bar_get_content_area (GTK_INFO_BAR(panel->priv->info_bar_profiles));
	gtk_container_add (GTK_CONTAINER(widget), info_bar_profiles_label);
	gtk_widget_show (info_bar_profiles_label);

	/* add infobar to defaults pane */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder,
						     "vbox_working_spaces"));
	gtk_box_pack_start (GTK_BOX(widget),
			    panel->priv->info_bar_profiles,
			    TRUE, FALSE, 0);

	/* get main window */
	panel->priv->main_window = gtk_widget_get_toplevel (panel->priv->info_bar_profiles);

	/* connect up drags */
	g_signal_connect (panel->priv->main_window, "drag-data-received",
			  G_CALLBACK (cc_color_panel_drag_data_received_cb), panel);
if(0)	cc_color_panel_setup_drag_and_drop (GTK_WIDGET (panel->priv->main_window));

	/* do all this after the window has been set up */
	g_idle_add ((GSourceFunc) cc_color_panel_startup_idle_cb, panel);
out:
	g_free (text);
	return;
}

void
cc_color_panel_register (GIOModule *module)
{
	cc_color_panel_register_type (G_TYPE_MODULE (module));
	g_io_extension_point_implement (CC_SHELL_PANEL_EXTENSION_POINT,
					CC_TYPE_COLOR_PANEL,
					"color", 0);
}

/* GIO extension stuff */
void
g_io_module_load (GIOModule *module)
{
	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

	/* register the panel */
	cc_color_panel_register (module);
}

void
g_io_module_unload (GIOModule *module)
{
}

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
#include <math.h>
#include <unique/unique.h>
#include <glib/gstdio.h>
#include <gudev/gudev.h>
#include <libgnomeui/gnome-rr.h>
#include <gconf/gconf-client.h>
#include <locale.h>
#include <canberra-gtk.h>

#include "egg-debug.h"

#include "gcm-calibrate-argyll.h"
#include "gcm-cie-widget.h"
#include "gcm-client.h"
#include "gcm-colorimeter.h"
#include "gcm-device-xrandr.h"
#include "gcm-profile.h"
#include "gcm-profile-store.h"
#include "gcm-trc-widget.h"
#include "gcm-utils.h"
#include "gcm-xyz.h"

static GtkBuilder *builder = NULL;
static GtkListStore *list_store_devices = NULL;
static GtkListStore *list_store_profiles = NULL;
static GcmDevice *current_device = NULL;
static GcmProfileStore *profile_store = NULL;
static GcmClient *gcm_client = NULL;
static GcmColorimeter *colorimeter = NULL;
static gboolean setting_up_device = FALSE;
static GtkWidget *info_bar_loading = NULL;
static GtkWidget *info_bar_vcgt = NULL;
static GtkWidget *cie_widget = NULL;
static GtkWidget *trc_widget = NULL;
static GConfClient *gconf_client = NULL;

enum {
	GCM_DEVICES_COLUMN_ID,
	GCM_DEVICES_COLUMN_SORT,
	GCM_DEVICES_COLUMN_ICON,
	GCM_DEVICES_COLUMN_TITLE,
	GCM_DEVICES_COLUMN_LAST
};

enum {
	GCM_PROFILES_COLUMN_ID,
	GCM_PROFILES_COLUMN_SORT,
	GCM_PROFILES_COLUMN_ICON,
	GCM_PROFILES_COLUMN_TITLE,
	GCM_PROFILES_COLUMN_PROFILE,
	GCM_PROFILES_COLUMN_LAST
};

enum {
	GCM_PREFS_COMBO_COLUMN_TEXT,
	GCM_PREFS_COMBO_COLUMN_PROFILE,
	GCM_PREFS_COMBO_COLUMN_TYPE,
	GCM_PREFS_COMBO_COLUMN_LAST
};

typedef enum {
	GCM_PREFS_ENTRY_TYPE_PROFILE,
	GCM_PREFS_ENTRY_TYPE_NONE,
	GCM_PREFS_ENTRY_TYPE_IMPORT,
	GCM_PREFS_ENTRY_TYPE_LAST
} GcmPrefsEntryType;

static void gcm_prefs_devices_treeview_clicked_cb (GtkTreeSelection *selection, gpointer userdata);

/**
 * gcm_prefs_error_dialog:
 **/
static void
gcm_prefs_error_dialog (const gchar *title, const gchar *message)
{
	GtkWindow *window;
	GtkWidget *dialog;

	window = GTK_WINDOW(gtk_builder_get_object (builder, "dialog_prefs"));
	dialog = gtk_message_dialog_new (window, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "%s", title);
	gtk_window_set_icon_name (GTK_WINDOW (dialog), GCM_STOCK_ICON);
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", message);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

/**
 * gcm_prefs_close_cb:
 **/
static void
gcm_prefs_close_cb (GtkWidget *widget, gpointer data)
{
	GMainLoop *loop = (GMainLoop *) data;
	g_main_loop_quit (loop);
}

/**
 * gcm_prefs_set_default:
 **/
static gboolean
gcm_prefs_set_default (GcmDevice *device)
{
	GError *error = NULL;
	gboolean ret = FALSE;
	gchar *cmdline = NULL;
	gchar *filename = NULL;
	gchar *id = NULL;
	gchar *install_cmd = NULL;

	/* get device properties */
	g_object_get (device,
		      "profile-filename", &filename,
		      "id", &id,
		      NULL);

	/* nothing set */
	if (filename == NULL) {
		egg_debug ("no filename for %s", id);
		goto out;
	}

	/* run using PolicyKit */
	install_cmd = g_build_filename (SBINDIR, "gcm-install-system-wide", NULL);
	cmdline = g_strdup_printf ("pkexec %s --id %s \"%s\"", install_cmd, id, filename);
	egg_debug ("running: %s", cmdline);
	ret = g_spawn_command_line_sync (cmdline, NULL, NULL, NULL, &error);
	if (!ret) {
		/* TRANSLATORS: could not save for all users */
		gcm_prefs_error_dialog (_("Failed to save defaults for all users"), error->message);
		g_error_free (error);
		goto out;
	}
out:
	g_free (id);
	g_free (install_cmd);
	g_free (cmdline);
	g_free (filename);
	return ret;
}

/**
 * gcm_prefs_combobox_add_profile:
 **/
static void
gcm_prefs_combobox_add_profile (GtkWidget *widget, GcmProfile *profile, GcmPrefsEntryType entry_type)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *description;

	/* use description */
	if (entry_type == GCM_PREFS_ENTRY_TYPE_NONE) {
		/* TRANSLATORS: this is where no profile is selected */
		description = g_strdup (_("None"));
	} else if (entry_type == GCM_PREFS_ENTRY_TYPE_IMPORT) {
		/* TRANSLATORS: this is where the user can click and import a profile */
		description = g_strdup (_("Other profile…"));
	} else {
		g_object_get (profile,
			      "description", &description,
			      NULL);
	}

	/* also add profile */
	model = gtk_combo_box_get_model (GTK_COMBO_BOX(widget));
	gtk_list_store_append (GTK_LIST_STORE(model), &iter);
	gtk_list_store_set (GTK_LIST_STORE(model), &iter,
			    GCM_PREFS_COMBO_COLUMN_TEXT, description,
			    GCM_PREFS_COMBO_COLUMN_PROFILE, profile,
			    GCM_PREFS_COMBO_COLUMN_TYPE, entry_type,
			    -1);
	g_free (description);
}

/**
 * gcm_prefs_default_cb:
 **/
static void
gcm_prefs_default_cb (GtkWidget *widget, gpointer data)
{
	GPtrArray *array = NULL;
	GcmDevice *device;
	GcmDeviceTypeEnum type;
	gboolean ret;
	guint i;

	/* set for each output */
	array = gcm_client_get_devices (gcm_client);
	for (i=0; i<array->len; i++) {
		device = g_ptr_array_index (array, i);
		g_object_get (device,
			      "type", &type,
			      NULL);

		/* not a xrandr panel */
		if (type != GCM_DEVICE_TYPE_ENUM_DISPLAY)
			continue;

		/* set for this device */
		ret = gcm_prefs_set_default (device);
		if (!ret)
			break;
	}
	g_ptr_array_unref (array);
}

/**
 * gcm_prefs_help_cb:
 **/
static void
gcm_prefs_help_cb (GtkWidget *widget, gpointer data)
{
	gcm_gnome_help ("preferences");
}

/**
 * gcm_prefs_delete_event_cb:
 **/
static gboolean
gcm_prefs_delete_event_cb (GtkWidget *widget, GdkEvent *event, gpointer data)
{
	gcm_prefs_close_cb (widget, data);
	return FALSE;
}

/**
 * gcm_prefs_calibrate_display:
 **/
static gboolean
gcm_prefs_calibrate_display (GcmCalibrate *calibrate)
{
	gboolean ret = FALSE;
	gboolean ret_tmp;
	GError *error = NULL;
	GtkWindow *window;

	/* no device */
	if (current_device == NULL)
		goto out;

	/* set properties from the device */
	ret = gcm_calibrate_set_from_device (calibrate, current_device, &error);
	if (!ret) {
		egg_warning ("failed to calibrate: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* run each task in order */
	window = GTK_WINDOW(gtk_builder_get_object (builder, "dialog_prefs"));
	ret = gcm_calibrate_display (calibrate, window, &error);
	if (!ret) {
		egg_warning ("failed to calibrate: %s", error->message);
		g_error_free (error);
		goto out;
	}
out:
	/* need to set the gamma back to the default after calibration */
	error = NULL;
	ret_tmp = gcm_device_apply (current_device, &error);
	if (!ret_tmp) {
		egg_warning ("failed to apply profile: %s", error->message);
		g_error_free (error);
	}
	return ret;
}

/**
 * gcm_prefs_calibrate_device:
 **/
static gboolean
gcm_prefs_calibrate_device (GcmCalibrate *calibrate)
{
	gboolean ret = FALSE;
	GError *error = NULL;
	GtkWindow *window;

	/* set defaults from device */
	ret = gcm_calibrate_set_from_device (calibrate, current_device, &error);
	if (!ret) {
		egg_warning ("failed to calibrate: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* do each step */
	window = GTK_WINDOW(gtk_builder_get_object (builder, "dialog_prefs"));
	ret = gcm_calibrate_device (calibrate, window, &error);
	if (!ret) {
		if (error->code != GCM_CALIBRATE_ERROR_USER_ABORT) {
			/* TRANSLATORS: could not calibrate */
			gcm_prefs_error_dialog (_("Failed to calibrate device"), error->message);
		} else {
			egg_warning ("failed to calibrate: %s", error->message);
		}
		g_error_free (error);
		goto out;
	}
out:
	return ret;
}

/**
 * gcm_prefs_calibrate_printer:
 **/
static gboolean
gcm_prefs_calibrate_printer (GcmCalibrate *calibrate)
{
	gboolean ret = FALSE;
	GError *error = NULL;
	GtkWindow *window;

	/* set defaults from device */
	ret = gcm_calibrate_set_from_device (calibrate, current_device, &error);
	if (!ret) {
		egg_warning ("failed to calibrate: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* do each step */
	window = GTK_WINDOW(gtk_builder_get_object (builder, "dialog_prefs"));
	ret = gcm_calibrate_printer (calibrate, window, &error);
	if (!ret) {
		if (error->code != GCM_CALIBRATE_ERROR_USER_ABORT) {
			/* TRANSLATORS: could not calibrate */
			gcm_prefs_error_dialog (_("Failed to calibrate printer"), error->message);
		} else {
			egg_warning ("failed to calibrate: %s", error->message);
		}
		g_error_free (error);
		goto out;
	}
out:
	return ret;
}

/**
 * gcm_prefs_profile_type_to_icon_name:
 **/
static const gchar *
gcm_prefs_profile_type_to_icon_name (GcmProfileTypeEnum type)
{
	if (type == GCM_PROFILE_TYPE_ENUM_DISPLAY_DEVICE)
		return "video-display";
	if (type == GCM_PROFILE_TYPE_ENUM_INPUT_DEVICE)
		return "scanner";
	if (type == GCM_PROFILE_TYPE_ENUM_OUTPUT_DEVICE)
		return "printer";
	if (type == GCM_PROFILE_TYPE_ENUM_COLORSPACE_CONVERSION)
		return "view-refresh";
	return "image-missing";
}

/**
 * gcm_prefs_profile_get_sort_string:
 **/
static const gchar *
gcm_prefs_profile_get_sort_string (GcmProfileTypeEnum type)
{
	if (type == GCM_PROFILE_TYPE_ENUM_DISPLAY_DEVICE)
		return "1";
	if (type == GCM_PROFILE_TYPE_ENUM_INPUT_DEVICE)
		return "2";
	if (type == GCM_PROFILE_TYPE_ENUM_OUTPUT_DEVICE)
		return "3";
	return "4";
}

/**
 * gcm_prefs_update_profile_list:
 **/
static void
gcm_prefs_update_profile_list (void)
{
	GtkTreeIter iter;
	gchar *description;
	const gchar *icon_name;
	GcmProfileTypeEnum profile_type = GCM_PROFILE_TYPE_ENUM_UNKNOWN;
	GcmProfile *profile;
	guint i;
	gchar *filename = NULL;
	gchar *sort = NULL;
	GPtrArray *profile_array = NULL;

	egg_debug ("updating profile list");

	/* get new list */
	profile_array = gcm_profile_store_get_array (profile_store);

	/* clear existing list */
	gtk_list_store_clear (list_store_profiles);

	/* update each list */
	for (i=0; i<profile_array->len; i++) {
		profile = g_ptr_array_index (profile_array, i);
		g_object_get (profile,
			      "description", &description,
			      "type", &profile_type,
			      "filename", &filename,
			      NULL);

		egg_debug ("add %s to profiles list", filename);
		icon_name = gcm_prefs_profile_type_to_icon_name (profile_type);
		gtk_list_store_append (list_store_profiles, &iter);
		sort = g_strdup_printf ("%s%s",
					gcm_prefs_profile_get_sort_string (profile_type),
					description);
		gtk_list_store_set (list_store_profiles, &iter,
				    GCM_PROFILES_COLUMN_ID, filename,
				    GCM_PROFILES_COLUMN_SORT, sort,
				    GCM_PROFILES_COLUMN_TITLE, description,
				    GCM_PROFILES_COLUMN_ICON, icon_name,
				    GCM_PROFILES_COLUMN_PROFILE, profile,
				    -1);

		g_free (sort);
		g_free (filename);
		g_free (description);
	}
	if (profile_array != NULL)
		g_ptr_array_unref (profile_array);
}

/**
 * gcm_prefs_profile_delete_cb:
 **/
static void
gcm_prefs_profile_delete_cb (GtkWidget *widget, gpointer data)
{
	GtkWidget *dialog;
	GtkResponseType response;
	GtkWindow *window;
	gint retval;
	gchar *filename = NULL;
	GcmProfile *profile;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;

	/* ask the user to confirm */
	window = GTK_WINDOW(gtk_builder_get_object (builder, "dialog_prefs"));
	dialog = gtk_message_dialog_new (window, GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_QUESTION, GTK_BUTTONS_CANCEL,
					 /* TRANSLATORS: title, usually we can tell based on the EDID data or output name */
					 _("Permanently delete profile?"));
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  /* TRANSLATORS: dialog message */
						  _("Are you sure you want to remove this profile from your system permanently?"));
	gtk_window_set_icon_name (GTK_WINDOW (dialog), GCM_STOCK_ICON);
	/* TRANSLATORS: button, delete a profile */
	gtk_dialog_add_button (GTK_DIALOG (dialog), _("Delete"), GTK_RESPONSE_YES);
	response = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
	if (response != GTK_RESPONSE_YES)
		goto out;

	/* get the selected row */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "treeview_profiles"));
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
		egg_debug ("no row selected");
		goto out;
	}

	/* get profile */
	gtk_tree_model_get (model, &iter,
			    GCM_PROFILES_COLUMN_PROFILE, &profile,
			    -1);

	/* get device data */
	g_object_get (profile,
		      "filename", &filename,
		      NULL);

	/* try to remove file */
	retval = g_unlink (filename);
	if (retval != 0)
		goto out;
out:
	g_free (filename);
}

/**
 * gcm_prefs_file_chooser_get_icc_profile:
 **/
static GFile *
gcm_prefs_file_chooser_get_icc_profile (void)
{
	GtkWindow *window;
	GtkWidget *dialog;
	GFile *file = NULL;
	GtkFileFilter *filter;

	/* create new dialog */
	window = GTK_WINDOW(gtk_builder_get_object (builder, "dialog_prefs"));
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
 * gcm_prefs_profile_import_file:
 **/
static gboolean
gcm_prefs_profile_import_file (GFile *file)
{
	gboolean ret = FALSE;
	GError *error = NULL;
	GFile *destination = NULL;

	/* copy icc file to ~/.color/icc */
	destination = gcm_utils_get_profile_destination (file);
	ret = gcm_utils_mkdir_and_copy (file, destination, &error);
	if (!ret) {
		/* TRANSLATORS: could not read file */
		gcm_prefs_error_dialog (_("Failed to copy file"), error->message);
		g_error_free (error);
		goto out;
	}
out:
	g_object_unref (destination);
	return ret;
}

/**
 * gcm_prefs_profile_import_cb:
 **/
static void
gcm_prefs_profile_import_cb (GtkWidget *widget, gpointer data)
{
	GFile *file;

	/* get new file */
	file = gcm_prefs_file_chooser_get_icc_profile ();
	if (file == NULL) {
		egg_warning ("failed to get filename");
		goto out;
	}

	/* import this */
	gcm_prefs_profile_import_file (file);
out:
	if (file != NULL)
		g_object_unref (file);
}

/**
 * gcm_prefs_drag_data_received_cb:
 **/
static void
gcm_prefs_drag_data_received_cb (GtkWidget *widget, GdkDragContext *context, gint x, gint y, GtkSelectionData *data, guint _time, gpointer user_data)
{
	const guchar *filename;
	gchar **filenames = NULL;
	GFile *file = NULL;
	guint i;
	gboolean ret;

	/* get filenames */
	filename = gtk_selection_data_get_data (data);
	if (filename == NULL) {
		gtk_drag_finish (context, FALSE, FALSE, _time);
		goto out;
	}

	/* import this */
	egg_debug ("dropped: %p (%s)", data, filename);

	/* split, as multiple drag targets are accepted */
	filenames = g_strsplit_set ((const gchar *)filename, "\r\n", -1);
	for (i=0; filenames[i]!=NULL; i++) {

		/* blank entry */
		if (filenames[i][0] == '\0')
			continue;

		/* check this is an ICC profile */
		egg_debug ("trying to import %s", filenames[i]);
		file = g_file_new_for_uri (filenames[i]);
		ret = gcm_utils_is_icc_profile (file);
		if (!ret) {
			egg_debug ("%s is not a ICC profile", filenames[i]);
			gtk_drag_finish (context, FALSE, FALSE, _time);
			goto out;
		}

		/* try to import it */
		ret = gcm_prefs_profile_import_file (file);
		if (!ret) {
			egg_debug ("%s did not import correctly", filenames[i]);
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
 * gcm_prefs_ensure_argyllcms_installed:
 **/
static gboolean
gcm_prefs_ensure_argyllcms_installed (void)
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

#ifndef GCM_USE_PACKAGEKIT
	egg_warning ("cannot install: this package was not compiled with --enable-packagekit");
	goto out;
#endif

	/* ask the user to confirm */
	window = GTK_WINDOW(gtk_builder_get_object (builder, "dialog_prefs"));
	dialog = gtk_message_dialog_new (window, GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
					 /* TRANSLATORS: title, usually we can tell based on the EDID data or output name */
					 _("Install missing calibration software?"));

	string = g_string_new ("");
	/* TRANSLATORS: dialog message saying the argyllcms is not installed */
	g_string_append_printf (string, "%s\n", _("Calibration software is not installed on this computer."));
	/* TRANSLATORS: dialog message saying the color targets are not installed */
	g_string_append_printf (string, "%s\n\n", _("These tools are required to build color profiles for devices."));
	/* TRANSLATORS: dialog message, asking if it's okay to install it */
	g_string_append_printf (string, "%s", _("Do you want them to be automatically installed?"));

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", string->str);
	gtk_window_set_icon_name (GTK_WINDOW (dialog), GCM_STOCK_ICON);
	/* TRANSLATORS: button, install a package */
	gtk_dialog_add_button (GTK_DIALOG (dialog), _("Install"), GTK_RESPONSE_YES);
	/* TRANSLATORS: button, skip installing a package */
	gtk_dialog_add_button (GTK_DIALOG (dialog), _("Do not install"), GTK_RESPONSE_CANCEL);
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
 * gcm_prefs_calibrate_cb:
 **/
static void
gcm_prefs_calibrate_cb (GtkWidget *widget, gpointer data)
{
	GcmCalibrate *calibrate = NULL;
	GcmDeviceTypeEnum type;
	gboolean ret;
	GError *error = NULL;
	gchar *filename = NULL;
	guint i;
	gchar *name;
	GcmProfile *profile;
	GPtrArray *profile_array = NULL;
	GFile *file = NULL;
	GFile *dest = NULL;
	gchar *destination = NULL;

	/* ensure argyllcms is installed */
	ret = gcm_prefs_ensure_argyllcms_installed ();
	if (!ret)
		goto out;

	/* get the type */
	g_object_get (current_device,
		      "type", &type,
		      NULL);

	/* create new calibration object */
	calibrate = GCM_CALIBRATE(gcm_calibrate_argyll_new ());

	/* choose the correct type of calibration */
	switch (type) {
	case GCM_DEVICE_TYPE_ENUM_DISPLAY:
		ret = gcm_prefs_calibrate_display (calibrate);
		break;
	case GCM_DEVICE_TYPE_ENUM_SCANNER:
	case GCM_DEVICE_TYPE_ENUM_CAMERA:
		ret = gcm_prefs_calibrate_device (calibrate);
		break;
	case GCM_DEVICE_TYPE_ENUM_PRINTER:
		ret = gcm_prefs_calibrate_printer (calibrate);
		break;
	default:
		egg_warning ("calibration not supported for this device");
		goto out;
	}

	/* we failed to calibrate */
	if (!ret)
		goto out;

	/* finish */
	g_object_get (calibrate,
		      "filename-result", &filename,
		      NULL);

	/* failed to get profile */
	if (filename == NULL) {
		egg_warning ("failed to get filename from calibration");
		goto out;
	}

	/* copy the ICC file to the proper location */
	file = g_file_new_for_path (filename);
	dest = gcm_utils_get_profile_destination (file);
	ret = gcm_utils_mkdir_and_copy (file, dest, &error);
	if (!ret) {
		egg_warning ("failed to calibrate: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* get new list */
	profile_array = gcm_profile_store_get_array (profile_store);

	/* find an existing profile of this name */
	destination = g_file_get_path (dest);
	for (i=0; i<profile_array->len; i++) {
		profile = g_ptr_array_index (profile_array, i);
		g_object_get (profile,
			      "filename", &name,
			      NULL);
		if (g_strcmp0 (name, destination) == 0) {
			egg_debug ("found existing profile: %s", destination);
			g_free (name);
			break;
		}
		g_free (name);
	}

	/* we didn't find an existing profile */
	if (i == profile_array->len) {
		egg_debug ("adding: %s", destination);

		/* set this default */
		g_object_set (current_device,
			      "profile-filename", destination,
			      NULL);
		ret = gcm_device_save (current_device, &error);
		if (!ret) {
			egg_warning ("failed to save default: %s", error->message);
			g_error_free (error);
			goto out;
		}
	}

	/* remove temporary file */
	g_unlink (filename);

	/* play sound from the naming spec */
	ca_context_play (ca_gtk_context_get (), 0,
			 CA_PROP_EVENT_ID, "complete",
			 /* TRANSLATORS: this is the application name for libcanberra */
			 CA_PROP_APPLICATION_NAME, _("GNOME Color Manager"),
			 /* TRANSLATORS: this is the sound description */
			 CA_PROP_EVENT_DESCRIPTION, _("Calibration completed"), NULL);
out:
	g_free (filename);
	g_free (destination);
	if (profile_array != NULL)
		g_ptr_array_unref (profile_array);
	if (calibrate != NULL)
		g_object_unref (calibrate);
	if (file != NULL)
		g_object_unref (file);
	if (dest != NULL)
		g_object_unref (dest);
}

/**
 * gcm_prefs_delete_cb:
 **/
static void
gcm_prefs_delete_cb (GtkWidget *widget, gpointer data)
{
	gboolean ret;
	GError *error = NULL;

	/* try to delete device */
	ret = gcm_client_delete_device (gcm_client, current_device, &error);
	if (!ret) {
		/* TRANSLATORS: could not read file */
		gcm_prefs_error_dialog (_("Failed to delete file"), error->message);
		g_error_free (error);
	}
}

/**
 * gcm_prefs_reset_cb:
 **/
static void
gcm_prefs_reset_cb (GtkWidget *widget, gpointer data)
{
	setting_up_device = TRUE;
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hscale_gamma"));
	gtk_range_set_value (GTK_RANGE (widget), 1.0f);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hscale_brightness"));
	gtk_range_set_value (GTK_RANGE (widget), 0.0f);
	setting_up_device = FALSE;
	/* we only want one save, not three */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hscale_contrast"));
	gtk_range_set_value (GTK_RANGE (widget), 1.0f);
}

/**
 * gcm_prefs_message_received_cb
 **/
static UniqueResponse
gcm_prefs_message_received_cb (UniqueApp *app, UniqueCommand command, UniqueMessageData *message_data, guint time_ms, gpointer data)
{
	GtkWindow *window;
	if (command == UNIQUE_ACTIVATE) {
		window = GTK_WINDOW (gtk_builder_get_object (builder, "dialog_prefs"));
		gtk_window_present (window);
	}
	return UNIQUE_RESPONSE_OK;
}

/**
 * gcm_window_set_parent_xid:
 **/
static void
gcm_window_set_parent_xid (GtkWindow *window, guint32 xid)
{
	GdkDisplay *display;
	GdkWindow *parent_window;
	GdkWindow *our_window;

	display = gdk_display_get_default ();
	parent_window = gdk_window_foreign_new_for_display (display, xid);
	our_window = gtk_widget_get_window (GTK_WIDGET (window));

	/* set this above our parent */
	gtk_window_set_modal (window, TRUE);
	gdk_window_set_transient_for (our_window, parent_window);
}

/**
 * gcm_prefs_add_devices_columns:
 **/
static void
gcm_prefs_add_devices_columns (GtkTreeView *treeview)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	/* image */
	renderer = gtk_cell_renderer_pixbuf_new ();
	g_object_set (renderer, "stock-size", GTK_ICON_SIZE_DIALOG, NULL);
	column = gtk_tree_view_column_new_with_attributes ("", renderer,
							   "icon-name", GCM_DEVICES_COLUMN_ICON, NULL);
	gtk_tree_view_append_column (treeview, column);

	/* column for text */
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer,
		      "ellipsize", PANGO_ELLIPSIZE_END,
		      "wrap-mode", PANGO_WRAP_WORD_CHAR,
		      "width-chars", 40,
		      NULL);
	column = gtk_tree_view_column_new_with_attributes ("", renderer,
							   "markup", GCM_DEVICES_COLUMN_TITLE, NULL);
	gtk_tree_view_column_set_sort_column_id (column, GCM_DEVICES_COLUMN_SORT);
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (list_store_devices), GCM_DEVICES_COLUMN_SORT, GTK_SORT_ASCENDING);
	gtk_tree_view_append_column (treeview, column);
	gtk_tree_view_column_set_expand (column, TRUE);
}

/**
 * gcm_prefs_add_profiles_columns:
 **/
static void
gcm_prefs_add_profiles_columns (GtkTreeView *treeview)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	/* image */
	renderer = gtk_cell_renderer_pixbuf_new ();
	g_object_set (renderer, "stock-size", GTK_ICON_SIZE_DND, NULL);
	column = gtk_tree_view_column_new_with_attributes ("", renderer,
							   "icon-name", GCM_PROFILES_COLUMN_ICON, NULL);
	gtk_tree_view_append_column (treeview, column);

	/* column for text */
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer,
		      "ellipsize", PANGO_ELLIPSIZE_END,
		      "wrap-mode", PANGO_WRAP_WORD_CHAR,
		      "width-chars", 50,
		      NULL);
	column = gtk_tree_view_column_new_with_attributes ("", renderer,
							   "markup", GCM_PROFILES_COLUMN_TITLE, NULL);
	gtk_tree_view_column_set_sort_column_id (column, GCM_PROFILES_COLUMN_SORT);
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (list_store_profiles), GCM_PROFILES_COLUMN_SORT, GTK_SORT_ASCENDING);
	gtk_tree_view_append_column (treeview, column);
	gtk_tree_view_column_set_expand (column, TRUE);
}

/**
 * gcm_prefs_set_calibrate_button_sensitivity:
 **/
static void
gcm_prefs_set_calibrate_button_sensitivity (void)
{
	gboolean ret = FALSE;
	GtkWidget *widget;
	const gchar *tooltip;
	GcmDeviceTypeEnum type;
	gboolean connected;

	/* TRANSLATORS: this is when the button is sensitive */
	tooltip = _("Create a color profile for the selected device");

	/* no device selected */
	if (current_device == NULL) {
		/* TRANSLATORS: this is when the button is insensitive */
		tooltip = _("Cannot calibrate: No device is selected");
		goto out;
	}

	/* get current device properties */
	g_object_get (current_device,
		      "type", &type,
		      "connected", &connected,
		      NULL);

	/* are we a display */
	if (type == GCM_DEVICE_TYPE_ENUM_DISPLAY) {

		/* are we disconnected */
		if (!connected) {
			/* TRANSLATORS: this is when the button is insensitive */
			tooltip = _("Cannot calibrate: The device is not connected");
			goto out;
		}

		/* find whether we have hardware installed */
		ret = gcm_colorimeter_get_present (colorimeter);
#ifndef GCM_HARDWARE_DETECTION
		egg_debug ("overriding device presence %i with TRUE", ret);
		ret = TRUE;
#endif
		if (!ret) {
			/* TRANSLATORS: this is when the button is insensitive */
			tooltip = _("Cannot calibrate: The colorimeter is not plugged in");
			goto out;
		}
	} else if (type == GCM_DEVICE_TYPE_ENUM_SCANNER ||
		   type == GCM_DEVICE_TYPE_ENUM_CAMERA) {

		/* TODO: find out if we can scan using gnome-scan */
		ret = TRUE;

	} else if (type == GCM_DEVICE_TYPE_ENUM_PRINTER) {

		/* find whether we have hardware installed */
		ret = gcm_colorimeter_get_present (colorimeter);
		if (!ret) {
			/* TRANSLATORS: this is when the button is insensitive */
			tooltip = _("Cannot calibrate: The colorimeter is not plugged in");
			goto out;
		}

		/* find whether we have hardware installed */
		ret = gcm_colorimeter_supports_printer (colorimeter);
		if (!ret) {
			/* TRANSLATORS: this is when the button is insensitive */
			tooltip = _("Cannot calibrate: The colorimeter does not support printer profiling");
			goto out;
		}

	} else {

		/* TRANSLATORS: this is when the button is insensitive */
		tooltip = _("Cannot calibrate this type of device");
	}
out:
	/* control the tooltip and sensitivity of the button */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "button_calibrate"));
	gtk_widget_set_tooltip_text (widget, tooltip);
	gtk_widget_set_sensitive (widget, ret);
}

/**
 * gcm_prefs_is_profile_suitable_for_device:
 **/
static gboolean
gcm_prefs_is_profile_suitable_for_device (GcmProfile *profile, GcmDevice *device)
{
	GcmProfileTypeEnum profile_type_tmp;
	GcmProfileTypeEnum profile_type;
	GcmColorspaceEnum profile_colorspace;
	GcmColorspaceEnum device_colorspace;
	gboolean ret = FALSE;
	GcmDeviceTypeEnum device_type;

	g_object_get (device,
		      "type", &device_type,
		      "colorspace", &device_colorspace,
		      NULL);

	/* get properties */
	g_object_get (profile,
		      "type", &profile_type_tmp,
		      "colorspace", &profile_colorspace,
		      NULL);

	/* not the right colorspace */
	if (device_colorspace != profile_colorspace)
		goto out;

	/* not the correct type */
	profile_type = gcm_utils_device_type_to_profile_type (device_type);
	if (profile_type_tmp != profile_type)
		goto out;

	/* success */
	ret = TRUE;
out:
	return ret;
}

/**
 * gcm_prefs_add_profiles_suitable_for_devices:
 **/
static void
gcm_prefs_add_profiles_suitable_for_devices (GtkWidget *widget, const gchar *profile_filename)
{
	GtkTreeModel *model;
	guint i;
	guint added_count = 1;
	gchar *filename;
	gboolean ret;
	gboolean set_active = FALSE;
	GcmProfile *profile;
	GPtrArray *profile_array = NULL;

	/* clear existing entries */
	model = gtk_combo_box_get_model (GTK_COMBO_BOX (widget));
	gtk_list_store_clear (GTK_LIST_STORE (model));

	/* add a 'None' entry */
	gcm_prefs_combobox_add_profile (widget, NULL, GCM_PREFS_ENTRY_TYPE_NONE);

	/* get new list */
	profile_array = gcm_profile_store_get_array (profile_store);

	/* add profiles of the right type */
	for (i=0; i<profile_array->len; i++) {
		profile = g_ptr_array_index (profile_array, i);

		/* only add correct types */
		ret = gcm_prefs_is_profile_suitable_for_device (profile, current_device);
		if (ret) {
			/* add */
			gcm_prefs_combobox_add_profile (widget, profile, GCM_PREFS_ENTRY_TYPE_PROFILE);

			/* set active option */
			g_object_get (profile,
				      "filename", &filename,
				      NULL);
			if (g_strcmp0 (filename, profile_filename) == 0) {
				gtk_combo_box_set_active (GTK_COMBO_BOX (widget), added_count);
				set_active = TRUE;
			}
			g_free (filename);

			/* keep a list so we can set active correctly */
			added_count++;
		}
	}

	/* add a import entry */
	gcm_prefs_combobox_add_profile (widget, NULL, GCM_PREFS_ENTRY_TYPE_IMPORT);

	/* select 'None' if there was no match */
	if (!set_active) {
		if (profile_filename != NULL)
			egg_warning ("no match for %s", profile_filename);
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
	}
	if (profile_array != NULL)
		g_ptr_array_unref (profile_array);
}

/**
 * gcm_prefs_devices_treeview_clicked_cb:
 **/
static void
gcm_prefs_devices_treeview_clicked_cb (GtkTreeSelection *selection, gpointer userdata)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *profile_filename = NULL;
	GtkWidget *widget;
	gfloat localgamma;
	gfloat brightness;
	gfloat contrast;
	gboolean connected;
	gchar *id = NULL;
	gboolean ret;
	GcmDeviceTypeEnum type;
	gchar *device_serial = NULL;
	gchar *device_model = NULL;
	gchar *device_manufacturer = NULL;

	/* This will only work in single or browse selection mode! */
	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
		egg_debug ("no row selected");
		goto out;
	}

	/* get id */
	gtk_tree_model_get (model, &iter,
			    GCM_DEVICES_COLUMN_ID, &id,
			    -1);

	/* we have a new device */
	egg_debug ("selected device is: %s", id);
	if (current_device != NULL) {
		g_object_unref (current_device);
		current_device = NULL;
	}
	current_device = gcm_client_get_device_by_id (gcm_client, id);
	if (current_device == NULL)
		goto out;
	g_object_get (current_device,
		      "type", &type,
		      NULL);

	/* not a xrandr device */
	if (type != GCM_DEVICE_TYPE_ENUM_DISPLAY) {
		widget = GTK_WIDGET (gtk_builder_get_object (builder, "expander_fine_tuning"));
		gtk_widget_set_sensitive (widget, FALSE);
		widget = GTK_WIDGET (gtk_builder_get_object (builder, "button_reset"));
		gtk_widget_set_sensitive (widget, FALSE);
	} else {
		/* show more UI */
		widget = GTK_WIDGET (gtk_builder_get_object (builder, "expander_fine_tuning"));
		gtk_widget_set_sensitive (widget, TRUE);
		widget = GTK_WIDGET (gtk_builder_get_object (builder, "button_reset"));
		gtk_widget_set_sensitive (widget, TRUE);
	}

	/* show broken devices */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "label_problems"));
	if (type == GCM_DEVICE_TYPE_ENUM_DISPLAY) {
		g_object_get (current_device,
			      "xrandr-fallback", &ret,
			      NULL);
		if (ret) {
			/* TRANSLATORS: this is when the user is using a binary blob */
			gtk_label_set_label (GTK_LABEL (widget), _("Per-device settings not supported. Check your display driver."));
			gtk_widget_show (widget);
		} else {
			gtk_widget_hide (widget);
		}
	} else {
		gtk_widget_hide (widget);
	}

	g_object_get (current_device,
		      "profile-filename", &profile_filename,
		      "gamma", &localgamma,
		      "brightness", &brightness,
		      "contrast", &contrast,
		      "connected", &connected,
		      "serial", &device_serial,
		      "model", &device_model,
		      "manufacturer", &device_manufacturer,
		      NULL);

	/* set device labels */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox_serial"));
	if (device_serial != NULL) {
		gtk_widget_show (widget);
		widget = GTK_WIDGET (gtk_builder_get_object (builder, "label_serial"));
		gtk_label_set_label (GTK_LABEL (widget), device_serial);
	} else {
		gtk_widget_hide (widget);
	}
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox_model"));
	if (device_model != NULL) {
		gtk_widget_show (widget);
		widget = GTK_WIDGET (gtk_builder_get_object (builder, "label_model"));
		gtk_label_set_label (GTK_LABEL (widget), device_model);
	} else {
		gtk_widget_hide (widget);
	}
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox_manufacturer"));
	if (device_manufacturer != NULL) {
		gtk_widget_show (widget);
		widget = GTK_WIDGET (gtk_builder_get_object (builder, "label_manufacturer"));
		gtk_label_set_label (GTK_LABEL (widget), device_manufacturer);
	} else {
		gtk_widget_hide (widget);
	}
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "vbox_device_details"));
	gtk_widget_show (widget);

	/* set adjustments */
	setting_up_device = TRUE;
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hscale_gamma"));
	gtk_range_set_value (GTK_RANGE (widget), localgamma);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hscale_brightness"));
	gtk_range_set_value (GTK_RANGE (widget), brightness);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hscale_contrast"));
	gtk_range_set_value (GTK_RANGE (widget), contrast);
	setting_up_device = FALSE;

	/* add profiles of the right type */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "combobox_profile"));
	gcm_prefs_add_profiles_suitable_for_devices (widget, profile_filename);

	/* make sure selectable */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "combobox_profile"));
	gtk_widget_set_sensitive (widget, TRUE);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "button_reset"));
	gtk_widget_set_sensitive (widget, TRUE);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "label_profile"));
	gtk_widget_set_sensitive (widget, TRUE);

	/* can we delete this device? */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "button_delete"));
	gtk_widget_set_sensitive (widget, !connected);

	/* can this device calibrate */
	gcm_prefs_set_calibrate_button_sensitivity ();
out:
	g_free (device_serial);
	g_free (device_model);
	g_free (device_manufacturer);
	g_free (id);
	g_free (profile_filename);
}

/**
 * gcm_prefs_profile_type_to_string:
 **/
static gchar *
gcm_prefs_profile_type_to_string (GcmProfileTypeEnum type)
{
	if (type == GCM_PROFILE_TYPE_ENUM_INPUT_DEVICE) {
		/* TRANSLATORS: this the ICC profile type */
		return _("Input device");
	}
	if (type == GCM_PROFILE_TYPE_ENUM_DISPLAY_DEVICE) {
		/* TRANSLATORS: this the ICC profile type */
		return _("Display device");
	}
	if (type == GCM_PROFILE_TYPE_ENUM_OUTPUT_DEVICE) {
		/* TRANSLATORS: this the ICC profile type */
		return _("Output device");
	}
	if (type == GCM_PROFILE_TYPE_ENUM_DEVICELINK) {
		/* TRANSLATORS: this the ICC profile type */
		return _("Devicelink");
	}
	if (type == GCM_PROFILE_TYPE_ENUM_COLORSPACE_CONVERSION) {
		/* TRANSLATORS: this the ICC profile type */
		return _("Colorspace conversion");
	}
	if (type == GCM_PROFILE_TYPE_ENUM_ABSTRACT) {
		/* TRANSLATORS: this the ICC profile type */
		return _("Abstract");
	}
	if (type == GCM_PROFILE_TYPE_ENUM_NAMED_COLOR) {
		/* TRANSLATORS: this the ICC profile type */
		return _("Named color");
	}
	/* TRANSLATORS: this the ICC profile type */
	return _("Unknown");
}

/**
 * gcm_prefs_profile_colorspace_to_string:
 **/
static gchar *
gcm_prefs_profile_colorspace_to_string (GcmColorspaceEnum type)
{
	if (type == GCM_COLORSPACE_ENUM_XYZ) {
		/* TRANSLATORS: this the ICC colorspace type */
		return _("XYZ");
	}
	if (type == GCM_COLORSPACE_ENUM_LAB) {
		/* TRANSLATORS: this the ICC colorspace type */
		return _("LAB");
	}
	if (type == GCM_COLORSPACE_ENUM_LUV) {
		/* TRANSLATORS: this the ICC colorspace type */
		return _("LUV");
	}
	if (type == GCM_COLORSPACE_ENUM_YCBCR) {
		/* TRANSLATORS: this the ICC colorspace type */
		return _("YCbCr");
	}
	if (type == GCM_COLORSPACE_ENUM_YXY) {
		/* TRANSLATORS: this the ICC colorspace type */
		return _("Yxy");
	}
	if (type == GCM_COLORSPACE_ENUM_RGB) {
		/* TRANSLATORS: this the ICC colorspace type */
		return _("RGB");
	}
	if (type == GCM_COLORSPACE_ENUM_GRAY) {
		/* TRANSLATORS: this the ICC colorspace type */
		return _("Gray");
	}
	if (type == GCM_COLORSPACE_ENUM_HSV) {
		/* TRANSLATORS: this the ICC colorspace type */
		return _("HSV");
	}
	if (type == GCM_COLORSPACE_ENUM_CMYK) {
		/* TRANSLATORS: this the ICC colorspace type */
		return _("CMYK");
	}
	if (type == GCM_COLORSPACE_ENUM_CMY) {
		/* TRANSLATORS: this the ICC colorspace type */
		return _("CMY");
	}
	/* TRANSLATORS: this the ICC colorspace type */
	return _("Unknown");
}

/**
 * gcm_prefs_profiles_treeview_clicked_cb:
 **/
static void
gcm_prefs_profiles_treeview_clicked_cb (GtkTreeSelection *selection, gpointer userdata)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkWidget *widget;
	GcmProfile *profile;
	GcmClut *clut = NULL;
	GcmXyz *white;
	GcmXyz *red;
	GcmXyz *green;
	GcmXyz *blue;
	gchar *profile_copyright = NULL;
	gchar *profile_manufacturer = NULL;
	gchar *profile_model = NULL;
	gchar *profile_datetime = NULL;
	gchar *temp;
	gchar *filename = NULL;
	gchar *basename = NULL;
	gchar *size_text = NULL;
	GcmProfileTypeEnum profile_type;
	GcmColorspaceEnum profile_colorspace;
	const gchar *profile_type_text;
	const gchar *profile_colorspace_text;
	gboolean ret;
	gboolean has_vcgt;
	guint size = 0;
	guint filesize;
	gfloat x;
	gboolean show_section = FALSE;

	/* This will only work in single or browse selection mode! */
	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
		egg_debug ("no row selected");
		return;
	}

	/* get profile */
	gtk_tree_model_get (model, &iter,
			    GCM_PROFILES_COLUMN_PROFILE, &profile,
			    -1);

	/* get the new details from the profile */
	g_object_get (profile,
		      "filename", &filename,
		      "size", &filesize,
		      "has-vcgt", &has_vcgt,
		      "copyright", &profile_copyright,
		      "manufacturer", &profile_manufacturer,
		      "model", &profile_model,
		      "datetime", &profile_datetime,
		      "type", &profile_type,
		      "colorspace", &profile_colorspace,
		      "white-point", &white,
		      "luminance-red", &red,
		      "luminance-green", &green,
		      "luminance-blue", &blue,
		      NULL);

	/* check we have enough data for the CIE widget */
	x = gcm_xyz_get_x (red);
	if (x > 0.001) {
		g_object_set (cie_widget,
			      "white", white,
			      "red", red,
			      "green", green,
			      "blue", blue,
			      NULL);
		gtk_widget_show (cie_widget);
		show_section = TRUE;
	} else {
		gtk_widget_hide (cie_widget);
	}

	/* get curve data */
	clut = gcm_profile_generate_curve (profile, 256);

	/* get size */
	if (clut != NULL) {
		g_object_get (clut,
			      "size", &size,
			      NULL);
	}

	/* only show if there is useful information */
	if (size > 0) {
		g_object_set (trc_widget,
			      "clut", clut,
			      NULL);
		gtk_widget_show (trc_widget);
		show_section = TRUE;
	} else {
		gtk_widget_hide (trc_widget);
	}

	/* set type */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox_type"));
	if (profile_type == GCM_PROFILE_TYPE_ENUM_UNKNOWN) {
		gtk_widget_hide (widget);
	} else {
		gtk_widget_show (widget);
		widget = GTK_WIDGET (gtk_builder_get_object (builder, "label_type"));
		profile_type_text = gcm_prefs_profile_type_to_string (profile_type);
		gtk_label_set_label (GTK_LABEL (widget), profile_type_text);
	}

	/* set colorspace */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox_colorspace"));
	if (profile_colorspace == GCM_COLORSPACE_ENUM_UNKNOWN) {
		gtk_widget_hide (widget);
	} else {
		gtk_widget_show (widget);
		widget = GTK_WIDGET (gtk_builder_get_object (builder, "label_colorspace"));
		profile_colorspace_text = gcm_prefs_profile_colorspace_to_string (profile_colorspace);
		gtk_label_set_label (GTK_LABEL (widget), profile_colorspace_text);
	}

	/* set vcgt */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox_vcgt"));
	gtk_widget_set_visible (widget, (profile_type == GCM_PROFILE_TYPE_ENUM_DISPLAY_DEVICE));
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "label_vcgt"));
	if (has_vcgt) {
		/* TRANSLATORS: if the device has a VCGT profile */
		gtk_label_set_label (GTK_LABEL (widget), _("Yes"));
	} else {
		/* TRANSLATORS: if the device has a VCGT profile */
		gtk_label_set_label (GTK_LABEL (widget), _("No"));
	}

	/* set basename */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "label_filename"));
	basename = g_path_get_basename (filename);
	gtk_label_set_label (GTK_LABEL (widget), basename);

	/* set size */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox_size"));
	if (filesize == 0) {
		gtk_widget_hide (widget);
	} else {
		gtk_widget_show (widget);
		widget = GTK_WIDGET (gtk_builder_get_object (builder, "label_size"));
		size_text = g_format_size_for_display (filesize);
		gtk_label_set_label (GTK_LABEL (widget), size_text);
	}

	/* set new copyright */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox_copyright"));
	if (profile_copyright == NULL) {
		gtk_widget_hide (widget);
	} else {
		gtk_widget_show (widget);
		widget = GTK_WIDGET (gtk_builder_get_object (builder, "label_copyright"));
		temp = gcm_utils_linkify (profile_copyright);
		gtk_label_set_label (GTK_LABEL (widget), temp);
		g_free (temp);
	}

	/* set new manufacturer */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox_profile_manufacturer"));
	if (profile_manufacturer == NULL) {
		gtk_widget_hide (widget);
	} else {
		gtk_widget_show (widget);
		widget = GTK_WIDGET (gtk_builder_get_object (builder, "label_profile_manufacturer"));
		temp = gcm_utils_linkify (profile_manufacturer);
		gtk_label_set_label (GTK_LABEL (widget), temp);
		g_free (temp);
	}

	/* set new model */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox_profile_model"));
	if (profile_model == NULL) {
		gtk_widget_hide (widget);
	} else {
		gtk_widget_show (widget);
		widget = GTK_WIDGET (gtk_builder_get_object (builder, "label_profile_model"));
		gtk_label_set_label (GTK_LABEL(widget), profile_model);
	}

	/* set new datetime */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox_datetime"));
	if (profile_datetime == NULL) {
		gtk_widget_hide (widget);
	} else {
		gtk_widget_show (widget);
		widget = GTK_WIDGET (gtk_builder_get_object (builder, "label_datetime"));
		gtk_label_set_label (GTK_LABEL(widget), profile_datetime);
	}

	/* set delete sensitivity */
	ret = (filename != NULL && g_str_has_prefix (filename, "/home/"));
	egg_debug ("filename: %s", filename);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "button_profile_delete"));
	gtk_widget_set_sensitive (widget, ret);

	/* should we show the pane at all */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "expander_profile_graphs"));
	gtk_widget_set_visible (widget, show_section);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "vbox_profile_info"));
	gtk_widget_set_visible (widget, TRUE);

	if (clut != NULL)
		g_object_unref (clut);
	g_object_unref (white);
	g_object_unref (red);
	g_object_unref (green);
	g_object_unref (blue);
	g_free (size_text);
	g_free (filename);
	g_free (basename);
	g_free (profile_copyright);
	g_free (profile_manufacturer);
	g_free (profile_model);
	g_free (profile_datetime);
}

/**
 * gcm_device_type_enum_to_string:
 **/
static const gchar *
gcm_prefs_device_type_to_string (GcmDeviceTypeEnum type)
{
	if (type == GCM_DEVICE_TYPE_ENUM_DISPLAY)
		return "1";
	if (type == GCM_DEVICE_TYPE_ENUM_SCANNER)
		return "2";
	if (type == GCM_DEVICE_TYPE_ENUM_CAMERA)
		return "3";
	if (type == GCM_DEVICE_TYPE_ENUM_PRINTER)
		return "4";
	return "5";
}

/**
 * gcm_prefs_add_device_xrandr:
 **/
static void
gcm_prefs_add_device_xrandr (GcmDevice *device)
{
	GtkTreeIter iter;
	gchar *title_tmp = NULL;
	gchar *title = NULL;
	gchar *sort = NULL;
	gchar *id = NULL;
	gboolean ret;
	gboolean connected;
	GError *error = NULL;

	/* sanity check */
	if (!GCM_IS_DEVICE_XRANDR (device)) {
		egg_warning ("not a xrandr device");
		goto out;
	}

	/* get details */
	g_object_get (device,
		      "id", &id,
		      "connected", &connected,
		      "title", &title_tmp,
		      NULL);

	/* italic for non-connected devices */
	if (connected) {
		/* set the gamma on the device */
		ret = gcm_device_apply (device, &error);
		if (!ret) {
			egg_warning ("failed to apply profile: %s", error->message);
			g_error_free (error);
		}

		/* use a different title if we have crap xorg drivers */
		if (ret) {
			title = g_strdup (title_tmp);
		} else {
			/* TRANSLATORS: this is where an output is not settable, but we are showing it in the UI */
			title = g_strdup_printf ("%s\n(%s)", title_tmp, _("No hardware support"));
		}
	} else {
		/* TRANSLATORS: this is where the device has been setup but is not connected */
		title = g_strdup_printf ("%s <i>[%s]</i>", title_tmp, _("disconnected"));
	}

	/* create sort order */
	sort = g_strdup_printf ("%s%s",
				gcm_prefs_device_type_to_string (GCM_DEVICE_TYPE_ENUM_DISPLAY),
				title);

	/* add to list */
	egg_debug ("add %s to device list", id);
	gtk_list_store_append (list_store_devices, &iter);
	gtk_list_store_set (list_store_devices, &iter,
			    GCM_DEVICES_COLUMN_ID, id,
			    GCM_DEVICES_COLUMN_SORT, sort,
			    GCM_DEVICES_COLUMN_TITLE, title,
			    GCM_DEVICES_COLUMN_ICON, "video-display", -1);
out:
	g_free (id);
	g_free (sort);
	g_free (title_tmp);
	g_free (title);
}

/**
 * gcm_prefs_set_combo_simple_text:
 **/
static void
gcm_prefs_set_combo_simple_text (GtkWidget *combo_box)
{
	GtkCellRenderer *renderer;
	GtkListStore *store;

	store = gtk_list_store_new (3, G_TYPE_STRING, GCM_TYPE_PROFILE, G_TYPE_UINT);
	gtk_combo_box_set_model (GTK_COMBO_BOX (combo_box), GTK_TREE_MODEL (store));
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
 * gcm_prefs_profile_combo_changed_cb:
 **/
static void
gcm_prefs_profile_combo_changed_cb (GtkWidget *widget, gpointer data)
{
	gchar *profile_old = NULL;
	GFile *file = NULL;
	GFile *dest = NULL;
	gboolean ret;
	GError *error = NULL;
	GcmProfile *profile = NULL;
	GcmProfile *profile_tmp = NULL;
	gboolean changed;
	GcmDeviceTypeEnum type;
	GtkTreeIter iter;
	GtkTreeModel *model;
	GcmPrefsEntryType entry_type;
	gboolean has_vcgt;
	gchar *filename = NULL;

	/* no devices */
	if (current_device == NULL)
		return;

	/* no selection */
	ret = gtk_combo_box_get_active_iter (GTK_COMBO_BOX(widget), &iter);
	if (!ret)
		return;

	/* get entry */
	model = gtk_combo_box_get_model (GTK_COMBO_BOX(widget));
	gtk_tree_model_get (model, &iter,
			    GCM_PREFS_COMBO_COLUMN_PROFILE, &profile,
			    GCM_PREFS_COMBO_COLUMN_TYPE, &entry_type,
			    -1);

	/* import */
	if (entry_type == GCM_PREFS_ENTRY_TYPE_IMPORT) {
		file = gcm_prefs_file_chooser_get_icc_profile ();
		if (file == NULL) {
			egg_warning ("failed to get ICC file");
			gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
			goto out;
		}

		/* check the file is suitable */
		profile_tmp = gcm_profile_default_new ();
		filename = g_file_get_path (file);
		ret = gcm_profile_parse (profile_tmp, file, &error);
		if (!ret) {
			/* set to 'None' */
			gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);

			egg_warning ("failed to parse ICC file: %s", error->message);
			g_error_free (error);
			goto out;
		}
		ret = gcm_prefs_is_profile_suitable_for_device (profile_tmp, current_device);
		if (!ret) {
			/* set to 'None' */
			gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);

			/* TRANSLATORS: the profile was of the wrong sort for this device */
			gcm_prefs_error_dialog (_("Could not import profile"), _("The profile was of the wrong type for this device"));
			goto out;
		}

		/* actually set this as the default */
		ret = gcm_prefs_profile_import_file (file);
		if (!ret) {
			gchar *uri;
			/* set to 'None' */
			gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);

			uri = g_file_get_uri (file);
			egg_debug ("%s did not import correctly", uri);
			g_free (uri);
			goto out;
		}

		/* now use the new profile as the device default */
		dest = gcm_utils_get_profile_destination (file);
		filename = g_file_get_path (dest);
	}

	/* get the device type */
	g_object_get (current_device,
		      "type", &type,
		      NULL);

	/* get profile filename */
	if (entry_type == GCM_PREFS_ENTRY_TYPE_PROFILE) {
		g_object_get (profile,
			      "filename", &filename,
			      "has-vcgt", &has_vcgt,
			      NULL);

		/* show a warning if the profile is crap */
		if (type == GCM_DEVICE_TYPE_ENUM_DISPLAY && !has_vcgt && filename != NULL) {
			gtk_widget_show (info_bar_vcgt);
		} else {
			gtk_widget_hide (info_bar_vcgt);
		}
	} else {
		gtk_widget_hide (info_bar_vcgt);
	}

	/* see if it's changed */
	g_object_get (current_device,
		      "profile-filename", &profile_old,
		      "type", &type,
		      NULL);
	egg_debug ("old: %s, new:%s", profile_old, filename);
	changed = ((g_strcmp0 (profile_old, filename) != 0));

	/* set new profile */
	if (changed) {
		g_object_set (current_device,
			      "profile-filename", filename,
			      NULL);

		/* save new profile */
		ret = gcm_device_save (current_device, &error);
		if (!ret) {
			egg_warning ("failed to save config: %s", error->message);
			g_error_free (error);
			goto out;
		}

		/* set the profile */
		ret = gcm_device_apply (current_device, &error);
		if (!ret) {
			egg_warning ("failed to apply profile: %s", error->message);
			g_error_free (error);
			goto out;
		}
	}
out:
	if (file != NULL)
		g_object_unref (file);
	if (dest != NULL)
		g_object_unref (dest);
	if (profile_tmp != NULL)
		g_object_unref (profile_tmp);
	g_free (profile_old);
	g_free (filename);
}

/**
 * gcm_prefs_slider_changed_cb:
 **/
static void
gcm_prefs_slider_changed_cb (GtkRange *range, gpointer *user_data)
{
	gfloat localgamma;
	gfloat brightness;
	gfloat contrast;
	GtkWidget *widget;
	gboolean ret;
	GError *error = NULL;

	/* we're just setting up the device, not moving the slider */
	if (setting_up_device)
		return;

	/* get values */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hscale_gamma"));
	localgamma = gtk_range_get_value (GTK_RANGE (widget));
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hscale_brightness"));
	brightness = gtk_range_get_value (GTK_RANGE (widget));
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hscale_contrast"));
	contrast = gtk_range_get_value (GTK_RANGE (widget));

	g_object_set (current_device,
		      "gamma", localgamma,
		      "brightness", brightness * 100.0f,
		      "contrast", contrast * 100.0f,
		      NULL);

	/* save new profile */
	ret = gcm_device_save (current_device, &error);
	if (!ret) {
		egg_warning ("failed to save config: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* actually set the new profile */
	ret = gcm_device_apply (current_device, &error);
	if (!ret) {
		egg_warning ("failed to apply profile: %s", error->message);
		g_error_free (error);
		goto out;
	}
out:
	return;
}

/**
 * gcm_prefs_colorimeter_changed_cb:
 **/
static void
gcm_prefs_colorimeter_changed_cb (GcmColorimeter *_colorimeter, gpointer user_data)
{
	gboolean present;
	const gchar *event_id;
	const gchar *message;

	present = gcm_colorimeter_get_present (_colorimeter);

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

	gcm_prefs_set_calibrate_button_sensitivity ();
}

/**
 * gcm_prefs_device_type_to_icon_name:
 **/
static const gchar *
gcm_prefs_device_type_to_icon_name (GcmDeviceTypeEnum type)
{
	if (type == GCM_DEVICE_TYPE_ENUM_DISPLAY)
		return "video-display";
	if (type == GCM_DEVICE_TYPE_ENUM_SCANNER)
		return "scanner";
	if (type == GCM_DEVICE_TYPE_ENUM_PRINTER)
		return "printer";
	if (type == GCM_DEVICE_TYPE_ENUM_CAMERA)
		return "camera-photo";
	return "image-missing";
}

/**
 * gcm_prefs_add_device_type:
 **/
static void
gcm_prefs_add_device_type (GcmDevice *device)
{
	GtkTreeIter iter;
	gchar *title;
	GString *string;
	gchar *id;
	gchar *sort = NULL;
	GcmDeviceTypeEnum type;
	const gchar *icon_name;
	gboolean connected;

	/* get details */
	g_object_get (device,
		      "id", &id,
		      "connected", &connected,
		      "title", &title,
		      "type", &type,
		      NULL);

	/* get icon */
	icon_name = gcm_prefs_device_type_to_icon_name (type);

	/* create a title for the device */
	string = g_string_new (title);

	/* italic for non-connected devices */
	if (!connected) {
		/* TRANSLATORS: this is where the device has been setup but is not connected */
		g_string_append_printf (string, "\n<i>[%s]</i>", _("disconnected"));
	}

	/* create sort order */
	sort = g_strdup_printf ("%s%s",
				gcm_prefs_device_type_to_string (type),
				string->str);

	/* add to list */
	gtk_list_store_append (list_store_devices, &iter);
	gtk_list_store_set (list_store_devices, &iter,
			    GCM_DEVICES_COLUMN_ID, id,
			    GCM_DEVICES_COLUMN_SORT, sort,
			    GCM_DEVICES_COLUMN_TITLE, string->str,
			    GCM_DEVICES_COLUMN_ICON, icon_name, -1);
	g_free (id);
	g_free (title);
	g_free (sort);
	g_string_free (string, TRUE);
}


/**
 * gcm_prefs_remove_device:
 **/
static void
gcm_prefs_remove_device (GcmDevice *gcm_device)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	const gchar *id;
	gchar *id_tmp;
	gboolean ret;

	/* remove */
	id = gcm_device_get_id (gcm_device);
	egg_debug ("removing: %s", id);

	/* get first element */
	model = GTK_TREE_MODEL (list_store_devices);
	ret = gtk_tree_model_get_iter_first (model, &iter);
	if (!ret)
		return;

	/* get the other elements */
	do {
		gtk_tree_model_get (model, &iter,
				    GCM_DEVICES_COLUMN_ID, &id_tmp,
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
 * gcm_prefs_added_idle_cb:
 **/
static gboolean
gcm_prefs_added_idle_cb (GcmDevice *device)
{
	GcmDeviceTypeEnum type;
	egg_debug ("added: %s", gcm_device_get_id (device));

	/* remove the saved device if it's already there */
	gcm_prefs_remove_device (device);

	/* get the type of the device */
	g_object_get (device,
		      "type", &type,
		      NULL);

	/* add the device */
	if (type == GCM_DEVICE_TYPE_ENUM_DISPLAY)
		gcm_prefs_add_device_xrandr (device);
	else
		gcm_prefs_add_device_type (device);

	/* unref the instance */
	g_object_unref (device);
	return FALSE;
}

/**
 * gcm_prefs_added_cb:
 **/
static void
gcm_prefs_added_cb (GcmClient *gcm_client_, GcmDevice *gcm_device, gpointer user_data)
{
	g_idle_add ((GSourceFunc) gcm_prefs_added_idle_cb, g_object_ref (gcm_device));
}

/**
 * gcm_prefs_removed_cb:
 **/
static void
gcm_prefs_removed_cb (GcmClient *gcm_client_, GcmDevice *gcm_device, gpointer user_data)
{
	gboolean connected;
	GtkTreeIter iter;
	GtkTreeSelection *selection;
	GtkWidget *widget;
	gboolean ret;

	/* remove from the UI */
	gcm_prefs_remove_device (gcm_device);

	/* get device properties */
	g_object_get (gcm_device,
		      "connected", &connected,
		      NULL);

	/* ensure this device is re-added if it's been saved */
	if (connected)
		gcm_client_add_saved (gcm_client, NULL);

	/* select the first device */
	ret = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (list_store_devices), &iter);
	if (!ret)
		return;

	/* click it */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "treeview_devices"));
	gtk_tree_view_set_model (GTK_TREE_VIEW (widget), GTK_TREE_MODEL (list_store_devices));
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
	gtk_tree_selection_select_iter (selection, &iter);
}

/**
 * gcm_prefs_startup_phase2_idle_cb:
 **/
static gboolean
gcm_prefs_startup_phase2_idle_cb (gpointer user_data)
{
	GtkWidget *widget;
	GtkTreeSelection *selection;
	GtkTreePath *path;

	/* update list of profiles */
	gcm_prefs_update_profile_list ();

	/* select a profile to display */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "treeview_profiles"));
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
	path = gtk_tree_path_new_from_string ("0");
	gtk_tree_selection_select_path (selection, path);
	gtk_tree_path_free (path);

	return FALSE;
}

/**
 * gcm_prefs_colorspace_to_localised_string:
 **/
static const gchar *
gcm_prefs_colorspace_to_localised_string (GcmColorspaceEnum colorspace)
{
	if (colorspace == GCM_COLORSPACE_ENUM_RGB) {
		/* TRANSLATORS: this is the colorspace, e.g. red, green, blue */
		return _("RGB");
	}
	if (colorspace == GCM_COLORSPACE_ENUM_CMYK) {
		/* TRANSLATORS: this is the colorspace, e.g. cyan, magenta, yellow, black */
		return _("CMYK");
	}
	return NULL;
}

/**
 * gcm_prefs_setup_space_combobox:
 **/
static void
gcm_prefs_setup_space_combobox (GtkWidget *widget, GcmColorspaceEnum colorspace, const gchar *profile_filename)
{
	GcmProfile *profile;
	guint i;
	gchar *filename;
	gchar *description;
	GcmColorspaceEnum colorspace_tmp;
	guint added_count = 0;
	gboolean has_vcgt;
	gchar *text = NULL;
	const gchar *search = "RGB";
	GPtrArray *profile_array = NULL;

	/* search is a way to reduce to number of profiles */
	if (colorspace == GCM_COLORSPACE_ENUM_CMYK)
		search = "CMYK";

	/* get new list */
	profile_array = gcm_profile_store_get_array (profile_store);

	/* update each list */
	for (i=0; i<profile_array->len; i++) {
		profile = g_ptr_array_index (profile_array, i);
		g_object_get (profile,
			      "has-vcgt", &has_vcgt,
			      "filename", &filename,
			      "description", &description,
			      "colorspace", &colorspace_tmp,
			      NULL);

		/* only for correct type */
		if (!has_vcgt &&
		    colorspace == colorspace_tmp &&
		    (colorspace == GCM_COLORSPACE_ENUM_CMYK ||
		     g_strstr_len (description, -1, search) != NULL)) {
			gcm_prefs_combobox_add_profile (widget, profile, GCM_PREFS_ENTRY_TYPE_PROFILE);

			/* set active option */
			if (g_strcmp0 (filename, profile_filename) == 0)
				gtk_combo_box_set_active (GTK_COMBO_BOX (widget), added_count);
			added_count++;
		}
		g_free (filename);
		g_free (description);
	}
	if (profile_array != NULL)
		g_ptr_array_unref (profile_array);
	if (added_count == 0) {
		/* TRANSLATORS: this is when there are no profiles that can be used; the search term is either "RGB" or "CMYK" */
		text = g_strdup_printf (_("No %s color spaces available"),
					gcm_prefs_colorspace_to_localised_string (colorspace));
		gtk_combo_box_append_text (GTK_COMBO_BOX(widget), text);
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), added_count);
		gtk_widget_set_sensitive (widget, FALSE);
	}
	g_free (text);
}

/**
 * gcm_prefs_space_combo_changed_cb:
 **/
static void
gcm_prefs_space_combo_changed_cb (GtkWidget *widget, gpointer data)
{
	gboolean ret;
	GtkTreeIter iter;
	gchar *filename = NULL;
	GtkTreeModel *model;
	GcmProfile *profile = NULL;
	const gchar *gconf_key = GCM_SETTINGS_COLORSPACE_RGB;

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
	g_object_get (profile,
		      "filename", &filename,
		      NULL);

	if (data != NULL)
		gconf_key = GCM_SETTINGS_COLORSPACE_CMYK;

	egg_debug ("changed working space %s", filename);
	gconf_client_set_string (gconf_client, gconf_key, filename, NULL);
out:
	if (profile != NULL)
		g_object_unref (profile);
	g_free (filename);
}


/**
 * gcm_prefs_renderer_combo_changed_cb:
 **/
static void
gcm_prefs_renderer_combo_changed_cb (GtkWidget *widget, gpointer data)
{
	gint active;
	const gchar *gconf_key = GCM_SETTINGS_RENDERING_INTENT_DISPLAY;
	const gchar *value;

	/* no selection */
	active = gtk_combo_box_get_active (GTK_COMBO_BOX(widget));
	if (active == -1)
		return;

	if (data != NULL)
		gconf_key = GCM_SETTINGS_RENDERING_INTENT_SOFTPROOF;

	/* save to GConf */
	value = gcm_intent_enum_to_string (active+1);
	egg_debug ("changed rendering intent to %s", value);
	gconf_client_set_string (gconf_client, gconf_key, value, NULL);
}

/**
 * gcm_prefs_setup_rendering_combobox:
 **/
static void
gcm_prefs_setup_rendering_combobox (GtkWidget *widget, const gchar *intent)
{
	guint i;
	gboolean ret = FALSE;
	const gchar *text;

	for (i=1; i<GCM_INTENT_ENUM_LAST; i++) {
		text = gcm_intent_enum_to_localized_text (i);
		gtk_combo_box_append_text (GTK_COMBO_BOX (widget), text);
		text = gcm_intent_enum_to_string (i);
		if (g_strcmp0 (text, intent) == 0) {
			ret = TRUE;
			gtk_combo_box_set_active (GTK_COMBO_BOX (widget), i-1);
		}
	}
	/* nothing matches, just set the first option */
	if (!ret)
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
}

/**
 * gcm_prefs_startup_phase1_idle_cb:
 **/
static gboolean
gcm_prefs_startup_phase1_idle_cb (gpointer user_data)
{
	GtkWidget *widget;
	gboolean ret;
	GError *error = NULL;
	gchar *colorspace_rgb;
	gchar *colorspace_cmyk;
	gchar *intent_display;
	gchar *intent_softproof;

	/* do we show the fine tuning box */
	ret = gconf_client_get_bool (gconf_client, GCM_SETTINGS_SHOW_FINE_TUNING, NULL);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "expander_fine_tuning"));
	gtk_widget_set_visible (widget, ret);

	/* setup RGB combobox */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "combobox_space_rgb"));
	colorspace_rgb = gconf_client_get_string (gconf_client, GCM_SETTINGS_COLORSPACE_RGB, NULL);
	gcm_prefs_set_combo_simple_text (widget);
	gcm_prefs_setup_space_combobox (widget, GCM_COLORSPACE_ENUM_RGB, colorspace_rgb);
	g_signal_connect (G_OBJECT (widget), "changed",
			  G_CALLBACK (gcm_prefs_space_combo_changed_cb), NULL);

	/* setup CMYK combobox */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "combobox_space_cmyk"));
	colorspace_cmyk = gconf_client_get_string (gconf_client, GCM_SETTINGS_COLORSPACE_CMYK, NULL);
	gcm_prefs_set_combo_simple_text (widget);
	gcm_prefs_setup_space_combobox (widget, GCM_COLORSPACE_ENUM_CMYK, colorspace_cmyk);
	g_signal_connect (G_OBJECT (widget), "changed",
			  G_CALLBACK (gcm_prefs_space_combo_changed_cb), (gpointer) "cmyk");

	/* setup rendering lists */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "combobox_rendering_display"));
	gcm_prefs_set_combo_simple_text (widget);
	intent_display = gconf_client_get_string (gconf_client, GCM_SETTINGS_RENDERING_INTENT_DISPLAY, NULL);
	gcm_prefs_setup_rendering_combobox (widget, intent_display);
	g_signal_connect (G_OBJECT (widget), "changed",
			  G_CALLBACK (gcm_prefs_renderer_combo_changed_cb), NULL);

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "combobox_rendering_softproof"));
	gcm_prefs_set_combo_simple_text (widget);
	intent_softproof = gconf_client_get_string (gconf_client, GCM_SETTINGS_RENDERING_INTENT_SOFTPROOF, NULL);
	gcm_prefs_setup_rendering_combobox (widget, intent_softproof);
	g_signal_connect (G_OBJECT (widget), "changed",
			  G_CALLBACK (gcm_prefs_renderer_combo_changed_cb), (gpointer) "softproof");

	/* coldplug plugged in devices */
	ret = gcm_client_add_connected (gcm_client, &error);
	if (!ret) {
		egg_warning ("failed to add connected devices: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* coldplug saved devices */
	ret = gcm_client_add_saved (gcm_client, &error);
	if (!ret) {
		egg_warning ("failed to add saved devices: %s", error->message);
		g_clear_error (&error);
		/* do not fail */
	}

	/* set calibrate button sensitivity */
	gcm_prefs_set_calibrate_button_sensitivity ();

	/* start phase 2 of the startup */
	g_idle_add ((GSourceFunc) gcm_prefs_startup_phase2_idle_cb, NULL);

out:
	g_free (intent_display);
	g_free (intent_softproof);
	g_free (colorspace_rgb);
	g_free (colorspace_cmyk);
	return FALSE;
}

/**
 * gcm_prefs_reset_devices_idle_cb:
 **/
static gboolean
gcm_prefs_reset_devices_idle_cb (gpointer user_data)
{
	GPtrArray *array = NULL;
	GcmDevice *device;
	GError *error = NULL;
	gboolean ret;
	guint i;

	/* set for each output */
	array = gcm_client_get_devices (gcm_client);
	for (i=0; i<array->len; i++) {
		device = g_ptr_array_index (array, i);

		/* set gamma for device */
		ret = gcm_device_apply (device, &error);
		if (!ret) {
			egg_warning ("failed to set profile: %s", error->message);
			g_error_free (error);
			break;
		}
	}
	g_ptr_array_unref (array);
	return FALSE;
}

/**
 * gcm_prefs_checkbutton_global_cb:
 **/
static void
gcm_prefs_checkbutton_global_cb (GtkWidget *widget, gpointer user_data)
{
	gboolean ret;

	/* get state */
	ret = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(widget));

	/* save new preference */
	gconf_client_set_bool (gconf_client, GCM_SETTINGS_GLOBAL_DISPLAY_CORRECTION, ret, NULL);

	/* set the new setting */
	g_idle_add ((GSourceFunc) gcm_prefs_reset_devices_idle_cb, NULL);
}

/**
 * gcm_prefs_checkbutton_profile_cb:
 **/
static void
gcm_prefs_checkbutton_profile_cb (GtkWidget *widget, gpointer user_data)
{
	gboolean ret;

	/* get state */
	ret = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(widget));

	/* save new preference */
	gconf_client_set_bool (gconf_client, GCM_SETTINGS_SET_ICC_PROFILE_ATOM, ret, NULL);

	/* set the new setting */
	g_idle_add ((GSourceFunc) gcm_prefs_reset_devices_idle_cb, NULL);
}

/**
 * gcm_prefs_setup_drag_and_drop:
 **/
static void
gcm_prefs_setup_drag_and_drop (GtkWidget *widget)
{
	GtkTargetEntry entry;

	/* setup a dummy entry */
	entry.target = g_strdup ("text/plain");
	entry.flags = GTK_TARGET_OTHER_APP;
	entry.info = 0;

	gtk_drag_dest_set (widget, GTK_DEST_DEFAULT_ALL, &entry, 1, GDK_ACTION_MOVE | GDK_ACTION_COPY);
	g_free (entry.target);
}

/**
 * gcm_prefs_profile_store_changed_cb:
 **/
static void
gcm_prefs_profile_store_changed_cb (GcmProfileStore *_profile_store, gpointer user_data)
{
	GtkTreeSelection *selection;
	GtkWidget *widget;

	/* clear and update the profile list */
	gcm_prefs_update_profile_list ();

	/* re-get all the profiles for this device */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "treeview_devices"));
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
	if (selection == NULL)
		return;
	g_signal_emit_by_name (selection, "changed", NULL);
}

/**
 * gcm_prefs_select_first_device_idle_cb:
 **/
static gboolean
gcm_prefs_select_first_device_idle_cb (gpointer data)
{
	GtkTreePath *path;
	GtkWidget *widget;

	/* set the cursor on the first device */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "treeview_devices"));
	path = gtk_tree_path_new_from_string ("0");
	gtk_tree_view_set_cursor (GTK_TREE_VIEW (widget), path, NULL, FALSE);
	gtk_tree_path_free (path);

	return FALSE;
}

/**
 * gcm_prefs_client_notify_loading_cb:
 **/
static void
gcm_prefs_client_notify_loading_cb (GcmClient *client, GParamSpec *pspec, gpointer data)
{
	gboolean loading;

	/* get the new state */
	g_object_get (client, "loading", &loading, NULL);

	/*if loading show the bar */
	if (loading) {
		gtk_widget_show (info_bar_loading);
		return;
	}

	/* otherwise clear the loading widget */
	gtk_widget_hide (info_bar_loading);

	/* idle callback */
	g_idle_add (gcm_prefs_select_first_device_idle_cb, NULL);
}

/**
 * gcm_prefs_info_bar_response_cb:
 **/
static void
gcm_prefs_info_bar_response_cb (GtkDialog *dialog, GtkResponseType response, gpointer user_data)
{
	if (response == GTK_RESPONSE_HELP) {
		/* open the help file in the right place */
		gcm_gnome_help ("faq-missing-vcgt");
	}
}

/**
 * main:
 **/
int
main (int argc, char **argv)
{
	guint retval = 0;
	GOptionContext *context;
	GtkWidget *main_window;
	GtkWidget *widget;
	UniqueApp *unique_app;
	guint xid = 0;
	GError *error = NULL;
	GMainLoop *loop;
	gboolean use_global;
	gboolean use_atom;
	GtkTreeSelection *selection;
	GtkWidget *info_bar_loading_label;
	GtkWidget *info_bar_vcgt_label;
	GtkSizeGroup *size_group = NULL;
	GtkSizeGroup *size_group2 = NULL;
	GdkScreen *screen;

	const GOptionEntry options[] = {
		{ "parent-window", 'p', 0, G_OPTION_ARG_INT, &xid,
		  /* TRANSLATORS: we can make this modal (stay on top of) another window */
		  _("Set the parent window to make this modal"), NULL },
		{ NULL}
	};

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	gtk_init (&argc, &argv);

	context = g_option_context_new ("gnome-color-manager prefs program");
	g_option_context_add_main_entries (context, options, NULL);
	g_option_context_add_group (context, egg_debug_get_option_group ());
	g_option_context_add_group (context, gtk_get_option_group (TRUE));
	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);

	/* block in a loop */
	loop = g_main_loop_new (NULL, FALSE);

	/* are we already activated? */
	unique_app = unique_app_new ("org.gnome.ColorManager.Prefs", NULL);
	if (unique_app_is_running (unique_app)) {
		egg_debug ("You have another instance running. This program will now close");
		unique_app_send_message (unique_app, UNIQUE_ACTIVATE, NULL);
		goto out;
	}
	g_signal_connect (unique_app, "message-received",
			  G_CALLBACK (gcm_prefs_message_received_cb), NULL);

	/* get UI */
	builder = gtk_builder_new ();
	retval = gtk_builder_add_from_file (builder, GCM_DATA "/gcm-prefs.ui", &error);
	if (retval == 0) {
		egg_warning ("failed to load ui: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* add application specific icons to search path */
	gtk_icon_theme_append_search_path (gtk_icon_theme_get_default (),
	                                   GCM_DATA G_DIR_SEPARATOR_S "icons");

	/* maintain a list of profiles */
	profile_store = gcm_profile_store_new ();
	g_signal_connect (profile_store, "changed", G_CALLBACK(gcm_prefs_profile_store_changed_cb), NULL);

	/* create list stores */
	list_store_devices = gtk_list_store_new (GCM_DEVICES_COLUMN_LAST, G_TYPE_STRING, G_TYPE_STRING,
						 G_TYPE_STRING, G_TYPE_STRING);
	list_store_profiles = gtk_list_store_new (GCM_PROFILES_COLUMN_LAST, G_TYPE_STRING, G_TYPE_STRING,
						  G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER);

	/* create device tree view */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "treeview_devices"));
	gtk_tree_view_set_model (GTK_TREE_VIEW (widget),
				 GTK_TREE_MODEL (list_store_devices));
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
	g_signal_connect (selection, "changed",
			  G_CALLBACK (gcm_prefs_devices_treeview_clicked_cb), NULL);

	/* add columns to the tree view */
	gcm_prefs_add_devices_columns (GTK_TREE_VIEW (widget));
	gtk_tree_view_columns_autosize (GTK_TREE_VIEW (widget));

	/* create profile tree view */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "treeview_profiles"));
	gtk_tree_view_set_model (GTK_TREE_VIEW (widget),
				 GTK_TREE_MODEL (list_store_profiles));
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
	g_signal_connect (selection, "changed",
			  G_CALLBACK (gcm_prefs_profiles_treeview_clicked_cb), NULL);

	/* add columns to the tree view */
	gcm_prefs_add_profiles_columns (GTK_TREE_VIEW (widget));
	gtk_tree_view_columns_autosize (GTK_TREE_VIEW (widget));

	main_window = GTK_WIDGET (gtk_builder_get_object (builder, "dialog_prefs"));

	/* Hide window first so that the dialogue resizes itself without redrawing */
	gtk_widget_hide (main_window);
	gtk_window_set_icon_name (GTK_WINDOW (main_window), GCM_STOCK_ICON);
	g_signal_connect (main_window, "delete_event",
			  G_CALLBACK (gcm_prefs_delete_event_cb), loop);
	g_signal_connect (main_window, "drag-data-received",
			  G_CALLBACK (gcm_prefs_drag_data_received_cb), loop);
	gcm_prefs_setup_drag_and_drop (GTK_WIDGET(main_window));

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "button_close"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gcm_prefs_close_cb), loop);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "button_default"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gcm_prefs_default_cb), loop);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "button_help"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gcm_prefs_help_cb), NULL);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "button_reset"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gcm_prefs_reset_cb), NULL);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "button_delete"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gcm_prefs_delete_cb), NULL);
	gtk_widget_set_sensitive (widget, FALSE);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "button_calibrate"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gcm_prefs_calibrate_cb), NULL);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "button_profile_delete"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gcm_prefs_profile_delete_cb), NULL);
	gtk_widget_set_sensitive (widget, FALSE);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "button_profile_import"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gcm_prefs_profile_import_cb), NULL);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "expander_fine_tuning"));
	gtk_widget_set_sensitive (widget, FALSE);

	/* hidden until a profile is selected */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "expander_profile_graphs"));
	gtk_widget_set_visible (widget, FALSE);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "vbox_profile_info"));
	gtk_widget_set_visible (widget, FALSE);

	/* hide widgets by default */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "vbox_device_details"));
	gtk_widget_hide (widget);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "label_profile"));
	gtk_widget_set_sensitive (widget, FALSE);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox_manufacturer"));
	gtk_widget_hide (widget);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox_model"));
	gtk_widget_hide (widget);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox_serial"));
	gtk_widget_hide (widget);

	/* setup icc profiles list */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "combobox_profile"));
	gcm_prefs_set_combo_simple_text (widget);
	gtk_widget_set_sensitive (widget, FALSE);
	g_signal_connect (G_OBJECT (widget), "changed",
			  G_CALLBACK (gcm_prefs_profile_combo_changed_cb), NULL);

	/* set ranges */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hscale_gamma"));
	gtk_range_set_range (GTK_RANGE (widget), 0.1f, 5.0f);
	gtk_scale_add_mark (GTK_SCALE (widget), 1.0f, GTK_POS_TOP, "");
	gtk_scale_add_mark (GTK_SCALE (widget), 1.8f, GTK_POS_TOP, "");
	gtk_scale_add_mark (GTK_SCALE (widget), 2.2f, GTK_POS_TOP, "");

	/* set ranges */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hscale_brightness"));
	gtk_range_set_range (GTK_RANGE (widget), 0.0f, 0.9f);
//	gtk_scale_add_mark (GTK_SCALE (widget), 0.0f, GTK_POS_TOP, "");

	/* set ranges */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hscale_contrast"));
	gtk_range_set_range (GTK_RANGE (widget), 0.1f, 1.0f);
//	gtk_scale_add_mark (GTK_SCALE (widget), 1.0f, GTK_POS_TOP, "");

	/* set alignment for left */
	size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox5"));
	gtk_size_group_add_widget (size_group, widget);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox10"));
	gtk_size_group_add_widget (size_group, widget);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox6"));
	gtk_size_group_add_widget (size_group, widget);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox21"));
	gtk_size_group_add_widget (size_group, widget);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox22"));
	gtk_size_group_add_widget (size_group, widget);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox23"));
	gtk_size_group_add_widget (size_group, widget);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox30"));
	gtk_size_group_add_widget (size_group, widget);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox32"));
	gtk_size_group_add_widget (size_group, widget);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox34"));
	gtk_size_group_add_widget (size_group, widget);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox36"));
	gtk_size_group_add_widget (size_group, widget);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox39"));
	gtk_size_group_add_widget (size_group, widget);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox48"));
	gtk_size_group_add_widget (size_group, widget);

	/* set alignment for right */
	size_group2 = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox24"));
	gtk_size_group_add_widget (size_group2, widget);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox25"));
	gtk_size_group_add_widget (size_group2, widget);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox26"));
	gtk_size_group_add_widget (size_group2, widget);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox11"));
	gtk_size_group_add_widget (size_group2, widget);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox12"));
	gtk_size_group_add_widget (size_group2, widget);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox18"));
	gtk_size_group_add_widget (size_group2, widget);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox31"));
	gtk_size_group_add_widget (size_group2, widget);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox33"));
	gtk_size_group_add_widget (size_group2, widget);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox35"));
	gtk_size_group_add_widget (size_group2, widget);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox37"));
	gtk_size_group_add_widget (size_group2, widget);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox40"));
	gtk_size_group_add_widget (size_group2, widget);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox49"));
	gtk_size_group_add_widget (size_group2, widget);

	/* use a device client array */
	gcm_client = gcm_client_new ();
	gcm_client_set_use_threads (gcm_client, TRUE);
	g_signal_connect (gcm_client, "added", G_CALLBACK (gcm_prefs_added_cb), NULL);
	g_signal_connect (gcm_client, "removed", G_CALLBACK (gcm_prefs_removed_cb), NULL);
	g_signal_connect (gcm_client, "notify::loading",
			  G_CALLBACK (gcm_prefs_client_notify_loading_cb), NULL);

	/* use the color device */
	colorimeter = gcm_colorimeter_new ();
	g_signal_connect (colorimeter, "changed", G_CALLBACK (gcm_prefs_colorimeter_changed_cb), NULL);

	/* set the parent window if it is specified */
	if (xid != 0) {
		egg_debug ("Setting xid %i", xid);
		gcm_window_set_parent_xid (GTK_WINDOW (main_window), xid);
	}

	/* use cie widget */
	cie_widget = gcm_cie_widget_new ();
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox_cie_widget"));
	gtk_box_pack_start (GTK_BOX(widget), cie_widget, TRUE, TRUE, 0);
	gtk_box_reorder_child (GTK_BOX(widget), cie_widget, 0);

	/* use trc widget */
	trc_widget = gcm_trc_widget_new ();
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox_trc_widget"));
	gtk_box_pack_start (GTK_BOX(widget), trc_widget, TRUE, TRUE, 0);
	gtk_box_reorder_child (GTK_BOX(widget), trc_widget, 0);

	/* do we set a default size to make the window larger? */
	screen = gdk_screen_get_default ();
	if (gdk_screen_get_width (screen) < 1024 ||
	    gdk_screen_get_height (screen) < 768) {
		gtk_widget_set_size_request (cie_widget, 50, 50);
		gtk_widget_set_size_request (trc_widget, 50, 50);
	} else {
		gtk_widget_set_size_request (cie_widget, 200, 200);
		gtk_widget_set_size_request (trc_widget, 200, 200);
	}

	/* use infobar */
	info_bar_loading = gtk_info_bar_new ();
	info_bar_vcgt = gtk_info_bar_new ();
	g_signal_connect (info_bar_vcgt, "response",
			  G_CALLBACK (gcm_prefs_info_bar_response_cb), NULL);

	/* TRANSLATORS: button for more details about the vcgt failure */
	gtk_info_bar_add_button (GTK_INFO_BAR(info_bar_vcgt), _("More information"), GTK_RESPONSE_HELP);

	/* TRANSLATORS: this is displayed while the devices are being probed */
	info_bar_loading_label = gtk_label_new (_("Loading list of devices…"));
	gtk_info_bar_set_message_type (GTK_INFO_BAR(info_bar_loading), GTK_MESSAGE_INFO);
	widget = gtk_info_bar_get_content_area (GTK_INFO_BAR(info_bar_loading));
	gtk_container_add (GTK_CONTAINER(widget), info_bar_loading_label);
	gtk_widget_show (info_bar_loading_label);

	/* TRANSLATORS: this is displayed when the profile is crap */
	info_bar_vcgt_label = gtk_label_new (_("This profile does not have the information required for whole-screen color correction."));
	gtk_info_bar_set_message_type (GTK_INFO_BAR(info_bar_vcgt), GTK_MESSAGE_INFO);
	widget = gtk_info_bar_get_content_area (GTK_INFO_BAR(info_bar_vcgt));
	gtk_container_add (GTK_CONTAINER(widget), info_bar_vcgt_label);
	gtk_widget_show (info_bar_vcgt_label);

	/* add infobar to devices pane */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "vbox_devices"));
	gtk_box_pack_start (GTK_BOX(widget), info_bar_loading, FALSE, FALSE, 0);

	/* add infobar to devices pane */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "vbox_sections"));
	gtk_box_pack_start (GTK_BOX(widget), info_bar_vcgt, FALSE, FALSE, 0);

	/* show main UI */
	gtk_window_set_default_size (GTK_WINDOW(main_window), 1000, 450);
	gtk_widget_show (main_window);

	/* connect up sliders */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hscale_contrast"));
	g_signal_connect (widget, "value-changed",
			  G_CALLBACK (gcm_prefs_slider_changed_cb), NULL);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hscale_brightness"));
	g_signal_connect (widget, "value-changed",
			  G_CALLBACK (gcm_prefs_slider_changed_cb), NULL);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hscale_gamma"));
	g_signal_connect (widget, "value-changed",
			  G_CALLBACK (gcm_prefs_slider_changed_cb), NULL);

	/* setup defaults */
	gconf_client = gconf_client_get_default ();
	use_global = gconf_client_get_bool (gconf_client, GCM_SETTINGS_GLOBAL_DISPLAY_CORRECTION, NULL);
	use_atom = gconf_client_get_bool (gconf_client, GCM_SETTINGS_SET_ICC_PROFILE_ATOM, NULL);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "checkbutton_display"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), use_global);
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gcm_prefs_checkbutton_global_cb), NULL);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "checkbutton_profile"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), use_atom);
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gcm_prefs_checkbutton_profile_cb), NULL);

	/* do all this after the window has been set up */
	g_idle_add (gcm_prefs_startup_phase1_idle_cb, NULL);

	/* wait */
	g_main_loop_run (loop);

out:
	g_object_unref (unique_app);
	g_main_loop_unref (loop);
	if (size_group != NULL)
		g_object_unref (size_group);
	if (size_group2 != NULL)
		g_object_unref (size_group2);
	if (current_device != NULL)
		g_object_unref (current_device);
	if (colorimeter != NULL)
		g_object_unref (colorimeter);
	if (gconf_client != NULL)
		g_object_unref (gconf_client);
	if (builder != NULL)
		g_object_unref (builder);
	if (profile_store != NULL)
		g_object_unref (profile_store);
	if (gcm_client != NULL)
		g_object_unref (gcm_client);
	return retval;
}


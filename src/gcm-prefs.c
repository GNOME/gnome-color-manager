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
 * GNU General Public License for more profile.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
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

#include "egg-debug.h"

#include "gcm-utils.h"
#include "gcm-profile.h"
#include "gcm-calibrate.h"
#include "gcm-brightness.h"
#include "gcm-client.h"
#include "gcm-xyz.h"
#include "gcm-cie-widget.h"
#include "gcm-trc-widget.h"

static GtkBuilder *builder = NULL;
static GtkListStore *list_store_devices = NULL;
static GtkListStore *list_store_profiles = NULL;
static GcmDevice *current_device = NULL;
static GnomeRRScreen *rr_screen = NULL;
static GPtrArray *profiles_array = NULL;
static GPtrArray *profiles_array_in_combo = NULL;
static GUdevClient *client = NULL;
static GcmClient *gcm_client = NULL;
static gboolean setting_up_device = FALSE;
static GtkWidget *info_bar = NULL;
static GtkWidget *cie_widget = NULL;
static GtkWidget *trc_widget = NULL;
static guint loading_refcount = 0;
static GConfClient *gconf_client = NULL;

enum {
	GPM_DEVICES_COLUMN_ID,
	GPM_DEVICES_COLUMN_SORT,
	GPM_DEVICES_COLUMN_ICON,
	GPM_DEVICES_COLUMN_TITLE,
	GPM_DEVICES_COLUMN_LAST
};

enum {
	GPM_PROFILES_COLUMN_ID,
	GPM_PROFILES_COLUMN_SORT,
	GPM_PROFILES_COLUMN_ICON,
	GPM_PROFILES_COLUMN_TITLE,
	GPM_PROFILES_COLUMN_PROFILE,
	GPM_PROFILES_COLUMN_LAST
};

static void gcm_prefs_devices_treeview_clicked_cb (GtkTreeSelection *selection, gpointer userdata);

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
 * gcm_prefs_delete_event_cb:
 **/
static gboolean
gcm_prefs_delete_event_cb (GtkWidget *widget, GdkEvent *event, gpointer data)
{
	gcm_prefs_close_cb (widget, data);
	return FALSE;
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
 * gcm_prefs_get_time:
 **/
static gchar *
gcm_prefs_get_time (void)
{
	gchar *text;
	time_t c_time;

	/* get the time now */
	time (&c_time);
	text = g_new0 (gchar, 255);

	/* format text */
	strftime (text, 254, "%H-%M-%S", localtime (&c_time));
	return text;
}

/**
 * gcm_prefs_calibrate_get_basename:
 **/
static gchar *
gcm_prefs_calibrate_get_basename (GcmDevice *device)
{
	gchar *serial = NULL;
	gchar *manufacturer = NULL;
	gchar *model = NULL;
	gchar *timespec = NULL;
	GDate *date = NULL;
	GString *basename;

	/* get device properties */
	g_object_get (device,
		      "serial", &serial,
		      "manufacturer", &manufacturer,
		      "model", &model,
		      NULL);

	/* create date and set it to now */
	date = g_date_new ();
	g_date_set_time_t (date, time (NULL));
	timespec = gcm_prefs_get_time ();

	/* form basename */
	basename = g_string_new ("GCM");
	if (manufacturer != NULL)
		g_string_append_printf (basename, " - %s", manufacturer);
	if (model != NULL)
		g_string_append_printf (basename, " - %s", model);
	if (serial != NULL)
		g_string_append_printf (basename, " - %s", serial);
	g_string_append_printf (basename, " (%04i-%02i-%02i)", date->year, date->month, date->day);

	/* maybe configure in GConf? */
	if (0)
		g_string_append_printf (basename, " [%s]", timespec);

	g_date_free (date);
	g_free (serial);
	g_free (manufacturer);
	g_free (model);
	g_free (timespec);
	return g_string_free (basename, FALSE);
}

/**
 * gcm_prefs_calibrate_display:
 **/
static gboolean
gcm_prefs_calibrate_display (GcmCalibrate *calib)
{
	GcmBrightness *brightness = NULL;
	gboolean ret = FALSE;
	GError *error = NULL;
	gchar *output_name = NULL;
	guint percentage = G_MAXUINT;
	gchar *basename = NULL;
	gchar *manufacturer = NULL;
	gchar *model = NULL;
	gchar *description = NULL;

	/* get the device */
	g_object_get (current_device,
		      "native-device-xrandr", &output_name,
		      "serial", &basename,
		      "manufacturer", &manufacturer,
		      "model", &model,
		      "title", &description,
		      NULL);
	if (output_name == NULL) {
		egg_warning ("failed to get output");
		goto out;
	}

	/* create new helper objects */
	brightness = gcm_brightness_new ();

	/* get a filename based on the serial number */
	basename = gcm_prefs_calibrate_get_basename (current_device);

	/* get model */
	if (model == NULL)
		model = g_strdup ("unknown model");

	/* get description */
	if (description == NULL)
		description = g_strdup ("calibrated monitor");

	/* get manufacturer */
	if (manufacturer == NULL)
		manufacturer = g_strdup ("unknown manufacturer");

	/* set the proper output name */
	g_object_set (calib,
		      "output-name", output_name,
		      "basename", basename,
		      "model", model,
		      "description", description,
		      "manufacturer", manufacturer,
		      NULL);

	/* step 0 */
	ret = gcm_calibrate_task (calib, GCM_CALIBRATE_TASK_DISPLAY_SETUP, &error);
	if (!ret) {
		egg_warning ("failed to calibrate: %s", error->message);
		g_error_free (error);
		goto finish_calibrate;
	}

	/* if we are an internal LCD, we need to set the brightness to maximum */
	ret = gcm_utils_output_is_lcd_internal (output_name);
	if (ret) {
		/* get the old brightness so we can restore state */
		ret = gcm_brightness_get_percentage (brightness, &percentage, &error);
		if (!ret) {
			egg_warning ("failed to get brightness: %s", error->message);
			g_error_free (error);
			/* not fatal */
			error = NULL;
		}

		/* set the new brightness */
		ret = gcm_brightness_set_percentage (brightness, 100, &error);
		if (!ret) {
			egg_warning ("failed to set brightness: %s", error->message);
			g_error_free (error);
			/* not fatal */
			error = NULL;
		}
	}

	/* step 1 */
	ret = gcm_calibrate_task (calib, GCM_CALIBRATE_TASK_DISPLAY_NEUTRALISE, &error);
	if (!ret) {
		egg_warning ("failed to calibrate: %s", error->message);
		g_error_free (error);
		goto finish_calibrate;
	}

	/* step 2 */
	ret = gcm_calibrate_task (calib, GCM_CALIBRATE_TASK_DISPLAY_GENERATE_PATCHES, &error);
	if (!ret) {
		egg_warning ("failed to calibrate: %s", error->message);
		g_error_free (error);
		goto finish_calibrate;
	}

	/* step 3 */
	ret = gcm_calibrate_task (calib, GCM_CALIBRATE_TASK_DISPLAY_DRAW_AND_MEASURE, &error);
	if (!ret) {
		egg_warning ("failed to calibrate: %s", error->message);
		g_error_free (error);
		goto finish_calibrate;
	}

	/* step 4 */
	ret = gcm_calibrate_task (calib, GCM_CALIBRATE_TASK_DISPLAY_GENERATE_PROFILE, &error);
	if (!ret) {
		egg_warning ("failed to calibrate: %s", error->message);
		g_error_free (error);
		goto finish_calibrate;
	}

finish_calibrate:
	/* need to set the gamma back to the default after calibration */
	error = NULL;
	ret = gcm_utils_set_gamma_for_device (current_device, &error);
	if (!ret) {
		egg_warning ("failed to set output gamma: %s", error->message);
		g_error_free (error);
	}

	/* restore brightness */
	if (percentage != G_MAXUINT) {
		/* set the new brightness */
		ret = gcm_brightness_set_percentage (brightness, percentage, &error);
		if (!ret) {
			egg_warning ("failed to restore brightness: %s", error->message);
			g_error_free (error);
			/* not fatal */
			error = NULL;
		}
	}
out:
	if (brightness != NULL)
		g_object_unref (brightness);
	g_free (output_name);
	g_free (basename);
	g_free (manufacturer);
	g_free (model);
	g_free (description);
	return ret;
}

/**
 * gcm_prefs_calibrate_device_get_scanned_profile:
 **/
static gchar *
gcm_prefs_calibrate_device_get_scanned_profile (const gchar *directory)
{
	gchar *filename = NULL;
	GtkWindow *window;
	GtkWidget *dialog;
	GtkFileFilter *filter;

	/* create new dialog */
	window = GTK_WINDOW(gtk_builder_get_object (builder, "dialog_prefs"));
	/* TRANSLATORS: dialog for file->open dialog */
	dialog = gtk_file_chooser_dialog_new (_("Select scanned reference file"), window,
					       GTK_FILE_CHOOSER_ACTION_OPEN,
					       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					       GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
					      NULL);
	gtk_window_set_icon_name (GTK_WINDOW (dialog), GCM_STOCK_ICON);
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER(dialog), directory);
	gtk_file_chooser_set_create_folders (GTK_FILE_CHOOSER(dialog), FALSE);

	/* setup the filter */
	filter = gtk_file_filter_new ();
	gtk_file_filter_add_mime_type (filter, "image/tiff");
	gtk_file_filter_add_mime_type (filter, "application/x-it87_2");
	/* TRANSLATORS: filter name on the file->open dialog */
	gtk_file_filter_set_name (filter, _("Supported images files"));
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER(dialog), filter);
//	g_object_unref (filter);

	/* did user choose file */
	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER(dialog));

	/* we're done */
	gtk_widget_destroy (dialog);

	/* or NULL for missing */
	return filename;
}

/**
 * gcm_prefs_calibrate_device_get_reference_data:
 **/
static gchar *
gcm_prefs_calibrate_device_get_reference_data (const gchar *directory)
{
	gchar *filename = NULL;
	GtkWindow *window;
	GtkWidget *dialog;
	GtkFileFilter *filter;

	/* create new dialog */
	window = GTK_WINDOW(gtk_builder_get_object (builder, "dialog_prefs"));
	/* TRANSLATORS: dialog for file->open dialog */
	dialog = gtk_file_chooser_dialog_new (_("Select CIE reference values file"), window,
					       GTK_FILE_CHOOSER_ACTION_OPEN,
					       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					       GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
					      NULL);
	gtk_window_set_icon_name (GTK_WINDOW (dialog), GCM_STOCK_ICON);
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER(dialog), directory);
	gtk_file_chooser_set_create_folders (GTK_FILE_CHOOSER(dialog), FALSE);

	/* setup the filter */
	filter = gtk_file_filter_new ();
	gtk_file_filter_add_pattern (filter, "*.txt");
	gtk_file_filter_add_pattern (filter, "*.TXT");
	/* TRANSLATORS: filter name on the file->open dialog */
	gtk_file_filter_set_name (filter, _("CIE values"));
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER(dialog), filter);
//	g_object_unref (filter);

	/* did user choose file */
	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER(dialog));

	/* we're done */
	gtk_widget_destroy (dialog);

	/* or NULL for missing */
	return filename;
}

/**
 * gcm_prefs_calibrate_device:
 **/
static gboolean
gcm_prefs_calibrate_device (GcmCalibrate *calib)
{
	gboolean ret = FALSE;
	gboolean has_shared_targets;
	GError *error = NULL;
	gchar *scanned_image = NULL;
	gchar *reference_data = NULL;
	gchar *basename = NULL;
	gchar *manufacturer = NULL;
	gchar *model = NULL;
	gchar *description = NULL;
	const gchar *directory;

	/* install shared-color-targets package */
	has_shared_targets = g_file_test ("/usr/share/shared-color-targets", G_FILE_TEST_IS_DIR);
	if (!has_shared_targets) {
		GtkWindow *window;
		GtkWidget *dialog;
		GtkResponseType response;
		GString *string;

		/* ask the user to confirm */
		window = GTK_WINDOW(gtk_builder_get_object (builder, "dialog_prefs"));
		dialog = gtk_message_dialog_new (window, GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
						 /* TRANSLATORS: title, usually we can tell based on the EDID data or output name */
						 _("Install missing files?"));
		string = g_string_new ("");
		/* TRANSLATORS: dialog message saying the color targets are not installed */
		g_string_append_printf (string, "%s\n", _("Common IT8 color target files are not installed on this computer."));
		/* TRANSLATORS: dialog message saying the color targets are not installed */
		g_string_append_printf (string, "%s\n\n", _("Color target files are needed to convert the image to a color profile."));
		/* TRANSLATORS: dialog message, asking if it's okay to install them */
		g_string_append_printf (string, "%s\n\n", _("Do you want them to be automatically installed?"));
		/* TRANSLATORS: dialog message, if the user has the target file on a CDROM then there's no need for this package */
		g_string_append_printf (string, "%s", _("If you have already have the required IT8 file then you can skip this step."));

		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", string->str);
		gtk_window_set_icon_name (GTK_WINDOW (dialog), GCM_STOCK_ICON);
		/* TRANSLATORS: button, install a package */
		gtk_dialog_add_button (GTK_DIALOG (dialog), _("Install"), GTK_RESPONSE_YES);
		/* TRANSLATORS: button, skip installing a package */
		gtk_dialog_add_button (GTK_DIALOG (dialog), _("Do not install"), GTK_RESPONSE_CANCEL);
		response = gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		/* only install if the user wanted to */
		if (response == GTK_RESPONSE_YES)
			has_shared_targets = gcm_utils_install_package ("shared-color-targets", window);
		g_string_free (string, TRUE);
	}

	/* get the device */
	g_object_get (current_device,
		      "model", &model,
		      "title", &description,
		      "manufacturer", &manufacturer,
		      NULL);

	/* step 0 */
	ret = gcm_calibrate_task (calib, GCM_CALIBRATE_TASK_DEVICE_SETUP, &error);
	if (!ret) {
		egg_warning ("failed to setup: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* get scanned image */
	directory = g_get_home_dir ();
	scanned_image = gcm_prefs_calibrate_device_get_scanned_profile (directory);
	if (scanned_image == NULL)
		goto out;

	/* get reference data */
	directory = has_shared_targets ? "/usr/share/color/targets" : "/media";
	reference_data = gcm_prefs_calibrate_device_get_reference_data (directory);
	if (reference_data == NULL)
		goto out;

	/* ensure we have data */
	basename = gcm_prefs_calibrate_get_basename (current_device);
	if (manufacturer == NULL)
		manufacturer = g_strdup ("Generic manufacturer");
	if (model == NULL)
		model = g_strdup ("Generic model");
	if (description == NULL)
		description = g_strdup ("Generic scanner");

	/* set the calibration parameters */
	g_object_set (calib,
		      "basename", basename,
		      "model", model,
		      "description", description,
		      "manufacturer", manufacturer,
		      "filename-source", scanned_image,
		      "filename-reference", reference_data,
		      NULL);

	/* step 1 */
	ret = gcm_calibrate_task (calib, GCM_CALIBRATE_TASK_DEVICE_COPY, &error);
	if (!ret) {
		egg_warning ("failed to calibrate: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* step 2 */
	ret = gcm_calibrate_task (calib, GCM_CALIBRATE_TASK_DEVICE_MEASURE, &error);
	if (!ret) {
		egg_warning ("failed to calibrate: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* step 3 */
	ret = gcm_calibrate_task (calib, GCM_CALIBRATE_TASK_DEVICE_GENERATE_PROFILE, &error);
	if (!ret) {
		egg_warning ("failed to calibrate: %s", error->message);
		g_error_free (error);
		goto out;
	}
out:
	g_free (basename);
	g_free (manufacturer);
	g_free (model);
	g_free (description);
	g_free (scanned_image);
	g_free (reference_data);
	return ret;
}

/**
 * gcm_prefs_profile_type_to_icon_name:
 **/
static const gchar *
gcm_prefs_profile_type_to_icon_name (GcmProfileType type)
{
	if (type == GCM_PROFILE_TYPE_DISPLAY_DEVICE)
		return "video-display";
	if (type == GCM_PROFILE_TYPE_INPUT_DEVICE)
		return "scanner";
	if (type == GCM_PROFILE_TYPE_OUTPUT_DEVICE)
		return "printer";
	if (type == GCM_PROFILE_TYPE_COLORSPACE_CONVERSION)
		return "view-refresh";
	return "image-missing";
}

/**
 * gcm_prefs_profile_get_sort_string:
 **/
static const gchar *
gcm_prefs_profile_get_sort_string (GcmProfileType type)
{
	if (type == GCM_PROFILE_TYPE_DISPLAY_DEVICE)
		return "1";
	if (type == GCM_PROFILE_TYPE_INPUT_DEVICE)
		return "2";
	if (type == GCM_PROFILE_TYPE_OUTPUT_DEVICE)
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
	GcmProfileType profile_type = GCM_PROFILE_TYPE_UNKNOWN;
	GcmProfile *profile;
	guint i;
	gchar *filename = NULL;
	gchar *sort = NULL;

	egg_debug ("updating profile list");

	/* clear existing list */
	gtk_list_store_clear (list_store_profiles);

	/* update each list */
	for (i=0; i<profiles_array->len; i++) {
		profile = g_ptr_array_index (profiles_array, i);
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
				    GPM_PROFILES_COLUMN_ID, filename,
				    GPM_PROFILES_COLUMN_SORT, sort,
				    GPM_PROFILES_COLUMN_TITLE, description,
				    GPM_PROFILES_COLUMN_ICON, icon_name,
				    GPM_PROFILES_COLUMN_PROFILE, profile,
				    -1);

		g_free (sort);
		g_free (filename);
		g_free (description);
	}
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
			    GPM_PROFILES_COLUMN_PROFILE, &profile,
			    -1);

	/* get device data */
	g_object_get (profile,
		      "filename", &filename,
		      NULL);

	/* try to remove file */
	retval = g_unlink (filename);
	if (retval != 0)
		goto out;

	/* find an existing profile of this name */
	g_ptr_array_remove (profiles_array, profile);

	/* update list of profiles */
	gcm_prefs_update_profile_list ();

	/* re-get all the profiles for this device */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "treeview_devices"));
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
	gcm_prefs_devices_treeview_clicked_cb (selection, NULL);
out:
	g_free (filename);
}

/**
 * gcm_prefs_calibrate_device_get_icc_profile:
 **/
static gchar *
gcm_prefs_calibrate_device_get_icc_profile (void)
{
	gchar *filename = NULL;
	GtkWindow *window;
	GtkWidget *dialog;
	GtkFileFilter *filter;

	/* create new dialog */
	window = GTK_WINDOW(gtk_builder_get_object (builder, "dialog_prefs"));
	/* TRANSLATORS: dialog for file->open dialog */
	dialog = gtk_file_chooser_dialog_new (_("Select ICC profile file"), window,
					       GTK_FILE_CHOOSER_ACTION_OPEN,
					       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					       GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
					      NULL);
	gtk_window_set_icon_name (GTK_WINDOW (dialog), GCM_STOCK_ICON);
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER(dialog), g_get_home_dir ());
	gtk_file_chooser_set_create_folders (GTK_FILE_CHOOSER(dialog), FALSE);

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
//	g_object_unref (filter);

	/* did user choose file */
	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER(dialog));

	/* we're done */
	gtk_widget_destroy (dialog);

	/* or NULL for missing */
	return filename;
}

/**
 * gcm_prefs_profile_import_cb:
 **/
static void
gcm_prefs_profile_import_cb (GtkWidget *widget, gpointer data)
{
	gchar *filename;
	GtkWidget *dialog;
	GError *error = NULL;
	gchar *destination = NULL;
	GtkWindow *window;
	GcmProfile *profile = NULL;
	GtkTreeSelection *selection;
	gboolean ret;

	/* get new file */
	filename = gcm_prefs_calibrate_device_get_icc_profile ();
	if (filename == NULL)
		goto out;

	/* copy icc file to ~/.color/icc */
	destination = gcm_utils_get_profile_destination (filename);
	ret = gcm_utils_mkdir_and_copy (filename, destination, &error);
	if (!ret) {
		/* TRANSLATORS: could not read file */
		window = GTK_WINDOW(gtk_builder_get_object (builder, "dialog_prefs"));
		dialog = gtk_message_dialog_new (window, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, _("Failed to copy file"));
		gtk_window_set_icon_name (GTK_WINDOW (dialog), GCM_STOCK_ICON);
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", error->message);
		gtk_dialog_run (GTK_DIALOG (dialog));
		g_error_free (error);
		gtk_widget_destroy (dialog);
		goto out;
	}

	/* add new profile */
	profile = gcm_profile_new ();

	/* parse the new file */
	ret = gcm_profile_parse (profile, destination, &error);
	if (!ret) {
		egg_warning ("failed to parse: %s", error->message);
		g_error_free (error);
		g_object_unref (profile);
		goto out;
	}

	/* add to arrays */
	g_ptr_array_add (profiles_array, g_object_ref (profile));

	/* update list of profiles */
	gcm_prefs_update_profile_list ();

	/* re-get all the profiles for this device */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "treeview_devices"));
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
	gcm_prefs_devices_treeview_clicked_cb (selection, NULL);
out:
	if (profile != NULL)
		g_object_unref (profile);
	g_free (filename);
	g_free (destination);
}

/**
 * gcm_prefs_calibrate_cb:
 **/
static void
gcm_prefs_calibrate_cb (GtkWidget *widget, gpointer data)
{
	GcmCalibrate *calib = NULL;
	GcmDeviceType type;
	gboolean ret;
	GError *error = NULL;
	GtkWindow *window;
	gchar *filename = NULL;
	guint i;
	gchar *name;
	gchar *destination = NULL;
	GcmProfile *profile;
	GtkTreeSelection *selection;

	/* get the type */
	g_object_get (current_device,
		      "type", &type,
		      NULL);

	/* create new calibration object */
	calib = gcm_calibrate_new ();

	/* run each task in order */
	window = GTK_WINDOW(gtk_builder_get_object (builder, "dialog_prefs"));
	ret = gcm_calibrate_setup (calib, window, &error);
	if (!ret) {
		egg_warning ("failed to setup: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* choose the correct type of calibration */
	switch (type) {
	case GCM_DEVICE_TYPE_DISPLAY:
		gcm_prefs_calibrate_display (calib);
		break;
	case GCM_DEVICE_TYPE_SCANNER:
	case GCM_DEVICE_TYPE_CAMERA:
		gcm_prefs_calibrate_device (calib);
		break;
	default:
		egg_warning ("calibration not supported for this device");
		goto out;
	}

	/* finish */
	filename = gcm_calibrate_finish (calib, &error);
	if (filename == NULL) {
		egg_warning ("failed to finish calibrate: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* copy the ICC file to the proper location */
	destination = gcm_utils_get_profile_destination (filename);
	ret = gcm_utils_mkdir_and_copy (filename, destination, &error);
	if (!ret) {
		egg_warning ("failed to calibrate: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* find an existing profile of this name */
	for (i=0; i<profiles_array->len; i++) {
		profile = g_ptr_array_index (profiles_array, i);
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
	if (i == profiles_array->len) {
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

		/* create a new instance */
		profile = gcm_profile_new ();

		/* parse the new file */
		ret = gcm_profile_parse (profile, destination, &error);
		if (!ret) {
			egg_warning ("failed to calibrate: %s", error->message);
			g_error_free (error);
			g_object_unref (profile);
			goto out;
		}

		/* add to arrays */
		g_ptr_array_add (profiles_array, g_object_ref (profile));

		/* update list of profiles */
		gcm_prefs_update_profile_list ();

		g_object_unref (profile);
	}

	/* re-get all the profiles for this device */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "treeview_devices"));
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
	gcm_prefs_devices_treeview_clicked_cb (selection, NULL);

	/* remove temporary file */
	g_unlink (filename);
out:
	g_free (filename);
	g_free (destination);
	if (calib != NULL)
		g_object_unref (calib);
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
		egg_warning ("failed to delete: %s", error->message);
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
							   "icon-name", GPM_DEVICES_COLUMN_ICON, NULL);
	gtk_tree_view_append_column (treeview, column);

	/* column for text */
	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes ("", renderer,
							   "markup", GPM_DEVICES_COLUMN_TITLE, NULL);
	gtk_tree_view_column_set_sort_column_id (column, GPM_DEVICES_COLUMN_SORT);
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (list_store_devices), GPM_DEVICES_COLUMN_SORT, GTK_SORT_ASCENDING);
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
							   "icon-name", GPM_PROFILES_COLUMN_ICON, NULL);
	gtk_tree_view_append_column (treeview, column);

	/* column for text */
	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes ("", renderer,
							   "markup", GPM_PROFILES_COLUMN_TITLE, NULL);
	gtk_tree_view_column_set_sort_column_id (column, GPM_PROFILES_COLUMN_SORT);
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (list_store_profiles), GPM_PROFILES_COLUMN_SORT, GTK_SORT_ASCENDING);
	gtk_tree_view_append_column (treeview, column);
	gtk_tree_view_column_set_expand (column, TRUE);
}

/**
 * gcm_prefs_has_hardware_device_attached:
 **/
static gboolean
gcm_prefs_has_hardware_device_attached (void)
{
	GList *devices;
	GList *l;
	GUdevDevice *device;
	gboolean ret = FALSE;

	/* get all USB devices */
	devices = g_udev_client_query_by_subsystem (client, "usb");
	for (l = devices; l != NULL; l = l->next) {
		device = l->data;
		ret = g_udev_device_get_property_as_boolean (device, "COLOR_MEASUREMENT_DEVICE");
		if (ret) {
			egg_debug ("found color management device: %s", g_udev_device_get_sysfs_path (device));
			break;
		}
	}

	g_list_foreach (devices, (GFunc) g_object_unref, NULL);
	g_list_free (devices);
	return ret;
}

/**
 * gcm_prefs_has_argyllcms_installed:
 **/
static gboolean
gcm_prefs_has_argyllcms_installed (void)
{
	gboolean ret;

	/* find whether argyllcms is installed using a tool which should exist */
	ret = g_file_test ("/usr/bin/dispcal", G_FILE_TEST_EXISTS);
	if (!ret)
		egg_debug ("ArgyllCMS not installed");

	return ret;
}

/**
 * gcm_prefs_set_calibrate_button_sensitivity:
 **/
static void
gcm_prefs_set_calibrate_button_sensitivity (void)
{
	gboolean ret = FALSE;
	GtkWidget *widget;
	GcmDeviceType type;
	gboolean connected;

	/* no device selected */
	if (current_device == NULL)
		goto out;

	/* get current device properties */
	g_object_get (current_device,
		      "type", &type,
		      "connected", &connected,
		      NULL);

	/* are we disconnected */
	if (!connected)
		goto out;

	/* are we a display */
	if (type == GCM_DEVICE_TYPE_DISPLAY) {

		/* find if ArgyllCMS is installed */
		ret = gcm_prefs_has_argyllcms_installed ();
		if (!ret)
			goto out;

		/* find whether we have hardware installed */
		ret = gcm_prefs_has_hardware_device_attached ();
#ifndef GCM_HARDWARE_DETECTION
		egg_debug ("overriding device presence %i with TRUE", ret);
		ret = TRUE;
#endif
	} else if (type == GCM_DEVICE_TYPE_SCANNER ||
		   type == GCM_DEVICE_TYPE_CAMERA) {

		/* find if ArgyllCMS is installed */
		ret = gcm_prefs_has_argyllcms_installed ();
		if (!ret)
			goto out;

		/* TODO: find out if we can scan using gnome-scan */
		ret = TRUE;
	} else {

		/* we can't calibrate this type of device */
		ret = FALSE;
	}
out:
	/* disable the button if no supported hardware is found */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "button_calibrate"));
	gtk_widget_set_sensitive (widget, ret);
}

/**
 * gcm_prefs_add_profiles_suitable_for_devices:
 **/
static void
gcm_prefs_add_profiles_suitable_for_devices (GcmDeviceType type)
{
	GtkTreeModel *combo_model;
	GtkWidget *widget;
	guint i;
	gchar *displayname;
	GcmProfile *profile;
	GcmProfileType profile_type;
	GcmProfileType profile_type_tmp;

	/* get the correct profile type for the device type */
	profile_type = gcm_utils_device_type_to_profile_type (type);

	/* clear existing entries */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "combobox_profile"));
	combo_model = gtk_combo_box_get_model (GTK_COMBO_BOX (widget));
	gtk_list_store_clear (GTK_LIST_STORE (combo_model));
	g_ptr_array_set_size (profiles_array_in_combo, 0);

	/* add profiles of the right type */
	for (i=0; i<profiles_array->len; i++) {
		profile = g_ptr_array_index (profiles_array, i);
		g_object_get (profile,
			      "description", &displayname,
			      "type", &profile_type_tmp,
			      NULL);
		if (profile_type_tmp == profile_type) {
			g_ptr_array_add (profiles_array_in_combo, g_object_ref (profile));
			gtk_combo_box_append_text (GTK_COMBO_BOX (widget), displayname);
		}
		g_free (displayname);
	}

	/* add a clear entry */
	gtk_combo_box_append_text (GTK_COMBO_BOX (widget), _("None"));
}

/**
 * gcm_prefs_devices_treeview_clicked_cb:
 **/
static void
gcm_prefs_devices_treeview_clicked_cb (GtkTreeSelection *selection, gpointer userdata)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	GcmProfile *profile;
	gchar *profile_filename = NULL;
	GtkWidget *widget;
	gfloat localgamma;
	gfloat brightness;
	gfloat contrast;
	gboolean connected;
	gchar *filename;
	guint i;
	gchar *id;
	GcmDeviceType type;
	gboolean ret = FALSE;
	gchar *device_serial = NULL;
	gchar *device_model = NULL;
	gchar *device_manufacturer = NULL;

	/* This will only work in single or browse selection mode! */
	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
		egg_debug ("no row selected");
		return;
	}

	/* get id */
	gtk_tree_model_get (model, &iter,
			    GPM_DEVICES_COLUMN_ID, &id,
			    -1);

	/* we have a new device */
	egg_debug ("selected device is: %s", id);
	if (current_device != NULL)
		g_object_unref (current_device);
	current_device = gcm_client_get_device_by_id (gcm_client, id);
	g_object_get (current_device,
		      "type", &type,
		      NULL);

	/* not a xrandr device */
	if (type != GCM_DEVICE_TYPE_DISPLAY) {
		widget = GTK_WIDGET (gtk_builder_get_object (builder, "expander1"));
		gtk_widget_set_sensitive (widget, FALSE);
		widget = GTK_WIDGET (gtk_builder_get_object (builder, "button_reset"));
		gtk_widget_set_sensitive (widget, FALSE);
	} else {
		/* show more UI */
		widget = GTK_WIDGET (gtk_builder_get_object (builder, "expander1"));
		gtk_widget_set_sensitive (widget, TRUE);
		widget = GTK_WIDGET (gtk_builder_get_object (builder, "button_reset"));
		gtk_widget_set_sensitive (widget, TRUE);
	}

	/* add profiles of the right type */
	gcm_prefs_add_profiles_suitable_for_devices (type);

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

	/* set correct profile */
	if (profile_filename == NULL) {
		egg_debug ("no profile selected");
		widget = GTK_WIDGET (gtk_builder_get_object (builder, "combobox_profile"));
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), profiles_array->len);
	} else {
		for (i=0; i<profiles_array_in_combo->len; i++) {
			profile = g_ptr_array_index (profiles_array_in_combo, i);
			g_object_get (profile,
				      "filename", &filename,
				      NULL);
			if (g_strcmp0 (filename, profile_filename) == 0) {
				widget = GTK_WIDGET (gtk_builder_get_object (builder, "combobox_profile"));
				gtk_combo_box_set_active (GTK_COMBO_BOX (widget), i);
				g_free (filename);
				ret = TRUE;
				break;
			}
			g_free (filename);
		}
	}

	/* failed to find profile in combobox */
	if (!ret) {
		egg_debug ("failed to find profile: %s", profile_filename);
		widget = GTK_WIDGET (gtk_builder_get_object (builder, "combobox_profile"));
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), profiles_array_in_combo->len);
	}

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

	g_free (device_serial);
	g_free (device_model);
	g_free (device_manufacturer);
	g_free (id);
	g_free (profile_filename);
}

/**
 * gcm_prefs_profile_type_to_text:
 **/
static gchar *
gcm_prefs_profile_type_to_text (GcmProfileType type)
{
	if (type == GCM_PROFILE_TYPE_INPUT_DEVICE) {
		/* TRANSLATORS: this the ICC profile type */
		return _("Input device");
	}
	if (type == GCM_PROFILE_TYPE_DISPLAY_DEVICE) {
		/* TRANSLATORS: this the ICC profile type */
		return _("Display device");
	}
	if (type == GCM_PROFILE_TYPE_OUTPUT_DEVICE) {
		/* TRANSLATORS: this the ICC profile type */
		return _("Output device");
	}
	if (type == GCM_PROFILE_TYPE_DEVICELINK) {
		/* TRANSLATORS: this the ICC profile type */
		return _("Devicelink");
	}
	if (type == GCM_PROFILE_TYPE_COLORSPACE_CONVERSION) {
		/* TRANSLATORS: this the ICC profile type */
		return _("Colorspace conversion");
	}
	if (type == GCM_PROFILE_TYPE_ABSTRACT) {
		/* TRANSLATORS: this the ICC profile type */
		return _("Abstract");
	}
	if (type == GCM_PROFILE_TYPE_NAMED_COLOR) {
		/* TRANSLATORS: this the ICC profile type */
		return _("Named color");
	}
	/* TRANSLATORS: this the ICC profile type */
	return _("Unknown");
}

/**
 * gcm_prefs_profile_colorspace_to_text:
 **/
static gchar *
gcm_prefs_profile_colorspace_to_text (GcmProfileColorspace type)
{
	if (type == GCM_PROFILE_COLORSPACE_XYZ) {
		/* TRANSLATORS: this the ICC colorspace type */
		return _("XYZ");
	}
	if (type == GCM_PROFILE_COLORSPACE_LAB) {
		/* TRANSLATORS: this the ICC colorspace type */
		return _("LAB");
	}
	if (type == GCM_PROFILE_COLORSPACE_LUV) {
		/* TRANSLATORS: this the ICC colorspace type */
		return _("LUV");
	}
	if (type == GCM_PROFILE_COLORSPACE_YCBCR) {
		/* TRANSLATORS: this the ICC colorspace type */
		return _("YCbCr");
	}
	if (type == GCM_PROFILE_COLORSPACE_YXY) {
		/* TRANSLATORS: this the ICC colorspace type */
		return _("Yxy");
	}
	if (type == GCM_PROFILE_COLORSPACE_RGB) {
		/* TRANSLATORS: this the ICC colorspace type */
		return _("RGB");
	}
	if (type == GCM_PROFILE_COLORSPACE_GRAY) {
		/* TRANSLATORS: this the ICC colorspace type */
		return _("Gray");
	}
	if (type == GCM_PROFILE_COLORSPACE_HSV) {
		/* TRANSLATORS: this the ICC colorspace type */
		return _("HSV");
	}
	if (type == GCM_PROFILE_COLORSPACE_CMYK) {
		/* TRANSLATORS: this the ICC colorspace type */
		return _("CMYK");
	}
	if (type == GCM_PROFILE_COLORSPACE_CMY) {
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
	gchar *filename = NULL;
	gchar *basename = NULL;
	gchar *size_text = NULL;
	GcmProfileType profile_type;
	GcmProfileColorspace profile_colorspace;
	const gchar *profile_type_text;
	const gchar *profile_colorspace_text;
	gboolean ret;
	guint size = 0;
	guint filesize;

	/* This will only work in single or browse selection mode! */
	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
		egg_debug ("no row selected");
		return;
	}

	/* get profile */
	gtk_tree_model_get (model, &iter,
			    GPM_PROFILES_COLUMN_PROFILE, &profile,
			    -1);

	/* get the new details from the profile */
	g_object_get (profile,
		      "filename", &filename,
		      "size", &filesize,
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

	g_object_set (cie_widget,
		      "white", white,
		      "red", red,
		      "green", green,
		      "blue", blue,
		      NULL);

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
	} else {
		gtk_widget_hide (trc_widget);
	}

	/* ensure showing */
	gtk_widget_show (cie_widget);

	/* set type */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox_type"));
	if (profile_type == GCM_PROFILE_TYPE_UNKNOWN) {
		gtk_widget_hide (widget);
	} else {
		gtk_widget_show (widget);
		widget = GTK_WIDGET (gtk_builder_get_object (builder, "label_type"));
		profile_type_text = gcm_prefs_profile_type_to_text (profile_type);
		gtk_label_set_label (GTK_LABEL (widget), profile_type_text);
	}

	/* set colorspace */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox_colorspace"));
	if (profile_colorspace == GCM_PROFILE_COLORSPACE_UNKNOWN) {
		gtk_widget_hide (widget);
	} else {
		gtk_widget_show (widget);
		widget = GTK_WIDGET (gtk_builder_get_object (builder, "label_colorspace"));
		profile_colorspace_text = gcm_prefs_profile_colorspace_to_text (profile_colorspace);
		gtk_label_set_label (GTK_LABEL (widget), profile_colorspace_text);
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
		gtk_label_set_label (GTK_LABEL(widget), profile_copyright);
	}

	/* set new manufacturer */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hbox_profile_manufacturer"));
	if (profile_manufacturer == NULL) {
		gtk_widget_hide (widget);
	} else {
		gtk_widget_show (widget);
		widget = GTK_WIDGET (gtk_builder_get_object (builder, "label_profile_manufacturer"));
		gtk_label_set_label (GTK_LABEL(widget), profile_manufacturer);
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
 * gcm_device_type_to_text:
 **/
static const gchar *
gcm_prefs_device_type_to_text (GcmDeviceType type)
{
	if (type == GCM_DEVICE_TYPE_DISPLAY)
		return "1";
	if (type == GCM_DEVICE_TYPE_SCANNER)
		return "2";
	if (type == GCM_DEVICE_TYPE_CAMERA)
		return "3";
	if (type == GCM_DEVICE_TYPE_PRINTER)
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
	gchar *title_tmp;
	gchar *title;
	gchar *sort = NULL;
	gchar *id;
	gboolean ret;
	gboolean connected;
	GError *error = NULL;

	/* get details */
	g_object_get (device,
		      "id", &id,
		      "connected", &connected,
		      "title", &title_tmp,
		      NULL);

	/* italic for non-connected devices */
	if (connected) {
		/* set the gamma on the device */
		ret = gcm_utils_set_gamma_for_device (device, &error);
		if (!ret) {
			egg_warning ("failed to set output gamma: %s", error->message);
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
				gcm_prefs_device_type_to_text (GCM_DEVICE_TYPE_DISPLAY),
				title);

	/* add to list */
	egg_debug ("add %s to device list", id);
	gtk_list_store_append (list_store_devices, &iter);
	gtk_list_store_set (list_store_devices, &iter,
			    GPM_DEVICES_COLUMN_ID, id,
			    GPM_DEVICES_COLUMN_SORT, sort,
			    GPM_DEVICES_COLUMN_TITLE, title,
			    GPM_DEVICES_COLUMN_ICON, "video-display", -1);
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
	GtkCellRenderer *cell;
	GtkListStore *store;

	store = gtk_list_store_new (1, G_TYPE_STRING);
	gtk_combo_box_set_model (GTK_COMBO_BOX (combo_box), GTK_TREE_MODEL (store));
	g_object_unref (store);

	cell = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo_box), cell, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo_box), cell,
					"text", 0,
					NULL);
}

/**
 * gcm_prefs_add_profiles:
 **/
static void
gcm_prefs_add_profiles (GtkWidget *widget)
{
	const gchar *filename;
	guint i;
	GcmProfile *profile;
	gboolean ret;
	GError *error = NULL;
	GPtrArray *filename_array;

	/* get profiles */
	filename_array = gcm_utils_get_profile_filenames ();
	for (i=0; i<filename_array->len; i++) {
		filename = g_ptr_array_index (filename_array, i);

		/* parse the profile name */
		profile = gcm_profile_new ();
		ret = gcm_profile_parse (profile, filename, &error);
		if (!ret) {
			egg_warning ("failed to add profile: %s", error->message);
			g_error_free (error);
			/* not fatal */
			error = NULL;
		} else {
			/* add to array */
			g_ptr_array_add (profiles_array, g_object_ref (profile));
		}
		g_object_unref (profile);
	}

	g_ptr_array_unref (filename_array);
}

/**
 * gcm_prefs_profile_combo_changed_cb:
 **/
static void
gcm_prefs_profile_combo_changed_cb (GtkWidget *widget, gpointer data)
{
	gint active;
	gchar *profile_old = NULL;
	gchar *filename = NULL;
	gboolean ret;
	GError *error = NULL;
	GcmProfile *profile = NULL;
	gboolean changed;
	GcmDeviceType type;

	/* no devices */
	if (current_device == NULL)
		return;

	/* no selection */
	active = gtk_combo_box_get_active (GTK_COMBO_BOX(widget));
	if (active == -1)
		return;

	/* get profile */
	if (active < (gint) profiles_array_in_combo->len)
		profile = g_ptr_array_index (profiles_array_in_combo, active);
	if (profile != NULL) {
		g_object_get (profile,
			      "filename", &filename,
			      NULL);
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

		/* set the gamma for display types */
		if (type == GCM_DEVICE_TYPE_DISPLAY) {
			ret = gcm_utils_set_gamma_for_device (current_device, &error);
			if (!ret) {
				egg_warning ("failed to set output gamma: %s", error->message);
				g_error_free (error);
				goto out;
			}
		}
	}
out:
	g_free (filename);
	g_free (profile_old);
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
	ret = gcm_utils_set_gamma_for_device (current_device, &error);
	if (!ret) {
		egg_warning ("failed to set output gamma: %s", error->message);
		g_error_free (error);
		goto out;
	}
out:
	return;
}

/**
 * gcm_prefs_uevent_cb:
 **/
static void
gcm_prefs_uevent_cb (GUdevClient *client_, const gchar *action, GUdevDevice *device, gpointer user_data)
{
	egg_debug ("uevent %s", action);
	gcm_prefs_set_calibrate_button_sensitivity ();
}

/**
 * gcm_prefs_device_type_to_icon_name:
 **/
static const gchar *
gcm_prefs_device_type_to_icon_name (GcmDeviceType type)
{
	if (type == GCM_DEVICE_TYPE_DISPLAY)
		return "video-display";
	if (type == GCM_DEVICE_TYPE_SCANNER)
		return "scanner";
	if (type == GCM_DEVICE_TYPE_PRINTER)
		return "printer";
	if (type == GCM_DEVICE_TYPE_CAMERA)
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
	GcmDeviceType type;
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
		g_string_append_printf (string, " <i>[%s]</i>", _("disconnected"));
	}

	/* use a different title for stuff that won't work yet */
	if (type != GCM_DEVICE_TYPE_DISPLAY) {
		/* TRANSLATORS: this is where the required software has not been written yet */
		g_string_append_printf (string, "\n(%s)", _("No software support"));
	}

	/* create sort order */
	sort = g_strdup_printf ("%s%s",
				gcm_prefs_device_type_to_text (type),
				string->str);

	/* add to list */
	gtk_list_store_append (list_store_devices, &iter);
	gtk_list_store_set (list_store_devices, &iter,
			    GPM_DEVICES_COLUMN_ID, id,
			    GPM_DEVICES_COLUMN_SORT, sort,
			    GPM_DEVICES_COLUMN_TITLE, string->str,
			    GPM_DEVICES_COLUMN_ICON, icon_name, -1);
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
				    GPM_DEVICES_COLUMN_ID, &id_tmp,
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
	GcmDeviceType type;
	GtkTreePath *path;
	GtkWidget *widget;
	egg_debug ("added: %s", gcm_device_get_id (device));

	/* remove the saved device if it's already there */
	gcm_prefs_remove_device (device);

	/* get the type of the device */
	g_object_get (device,
		      "type", &type,
		      NULL);

	/* add the device */
	if (type == GCM_DEVICE_TYPE_DISPLAY)
		gcm_prefs_add_device_xrandr (device);
	else
		gcm_prefs_add_device_type (device);

	/* clear loading widget */
	if (--loading_refcount == 0) {
		gtk_widget_hide (info_bar);

		/* set the cursor on the first device */
		widget = GTK_WIDGET (gtk_builder_get_object (builder, "treeview_devices"));
		path = gtk_tree_path_new_from_string ("0");
		gtk_tree_view_set_cursor (GTK_TREE_VIEW (widget), path, NULL, FALSE);
		gtk_tree_path_free (path);
	}

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
	gtk_widget_show (info_bar);
	g_idle_add ((GSourceFunc) gcm_prefs_added_idle_cb, g_object_ref (gcm_device));
	loading_refcount++;
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
 * gcm_prefs_startup_phase1_idle_cb:
 **/
static gboolean
gcm_prefs_startup_phase1_idle_cb (gpointer user_data)
{
	GtkWidget *widget;
	gboolean ret;
	GError *error = NULL;

	/* add profiles we can find */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "combobox_profile"));
	gcm_prefs_add_profiles (widget);

	/* coldplug plugged in devices */
	ret = gcm_client_add_connected (gcm_client, &error);
	if (!ret) {
		egg_warning ("failed to coldplug: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* coldplug saved devices */
	ret = gcm_client_add_saved (gcm_client, &error);
	if (!ret) {
		egg_warning ("failed to coldplug: %s", error->message);
		g_error_free (error);
		/* do not fail */
	}

	/* set calibrate button sensitivity */
	gcm_prefs_set_calibrate_button_sensitivity ();

	/* start phase 2 of the startup */
	g_idle_add ((GSourceFunc) gcm_prefs_startup_phase2_idle_cb, NULL);

out:
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
	GcmDeviceType type;
	GError *error = NULL;
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
		if (type != GCM_DEVICE_TYPE_DISPLAY)
			continue;

		/* set gamma for device */
		ret = gcm_utils_set_gamma_for_device (device, &error);
		if (!ret) {
			egg_warning ("failed to set gamma: %s", error->message);
			g_error_free (error);
			break;
		}
	}
	g_ptr_array_unref (array);
	return FALSE;
}

/**
 * gcm_prefs_radio_cb:
 **/
static void
gcm_prefs_radio_cb (GtkWidget *widget, gpointer user_data)
{
	const gchar *name;
	gboolean use_global = FALSE;
	gboolean use_atom = FALSE;

	/* find out what button was pressed */
	name = gtk_widget_get_name (widget);
	if (g_strcmp0 (name, "radiobutton_ouput_both") == 0) {
		use_global = TRUE;
		use_atom = TRUE;
	} else if (g_strcmp0 (name, "radiobutton_ouput_global") == 0) {
		use_global = TRUE;
	} else if (g_strcmp0 (name, "radiobutton_ouput_atom") == 0) {
		use_atom = TRUE;
	}

	/* save new preference */
	gconf_client_set_bool (gconf_client, GCM_SETTINGS_GLOBAL_DISPLAY_CORRECTION, use_global, NULL);
	gconf_client_set_bool (gconf_client, GCM_SETTINGS_SET_ICC_PROFILE_ATOM, use_atom, NULL);

	/* set the new setting */
	g_idle_add ((GSourceFunc) gcm_prefs_reset_devices_idle_cb, NULL);
}

/**
 * gcm_prefs_renderer_combo_changed_cb:
 **/
static void
gcm_prefs_renderer_combo_changed_cb (GtkWidget *widget, gpointer data)
{
	gint active;
	/* no selection */
	active = gtk_combo_box_get_active (GTK_COMBO_BOX(widget));
	if (active == -1)
		return;
	//TODO: need to save to GConf
	egg_debug ("changed rendering intent to %i", active);
}

/**
 * gcm_prefs_space_combo_changed_cb:
 **/
static void
gcm_prefs_space_combo_changed_cb (GtkWidget *widget, gpointer data)
{
	gint active;
	/* no selection */
	active = gtk_combo_box_get_active (GTK_COMBO_BOX(widget));
	if (active == -1)
		return;
	//TODO: need to save to GConf
	egg_debug ("changed working space %i", active);
}

/**
 * gcm_prefs_setup_rendering_combobox:
 **/
static void
gcm_prefs_setup_rendering_combobox (GtkWidget *widget)
{
	/* TRANSLATORS: rendering intent: you probably want to google this */
	gtk_combo_box_append_text (GTK_COMBO_BOX (widget), _("Perceptual"));

	/* TRANSLATORS: rendering intent: you probably want to google this */
	gtk_combo_box_append_text (GTK_COMBO_BOX (widget), _("Relative colormetric"));

	/* TRANSLATORS: rendering intent: you probably want to google this */
	gtk_combo_box_append_text (GTK_COMBO_BOX (widget), _("Saturation"));

	/* TRANSLATORS: rendering intent: you probably want to google this */
	gtk_combo_box_append_text (GTK_COMBO_BOX (widget), _("Absolute colormetric"));

	/* TRANSLATORS: rendering intent: you probably want to google this */
	gtk_combo_box_append_text (GTK_COMBO_BOX (widget), _("Disable soft proofing"));
}

/**
 * gcm_prefs_setup_space_combobox:
 **/
static void
gcm_prefs_setup_space_combobox (GtkWidget *widget)
{
	/* TODO: get from profiles */
	gtk_combo_box_append_text (GTK_COMBO_BOX (widget), "sRGB");
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
	const gchar *subsystems[] = {"usb", NULL};
	GtkWidget *info_bar_label;
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

	/* use GUdev to find the calibration device */
	client = g_udev_client_new (subsystems);
	g_signal_connect (client, "uevent",
			  G_CALLBACK (gcm_prefs_uevent_cb), NULL);

	/* create list stores */
	list_store_devices = gtk_list_store_new (GPM_DEVICES_COLUMN_LAST, G_TYPE_STRING, G_TYPE_STRING,
						 G_TYPE_STRING, G_TYPE_STRING);
	list_store_profiles = gtk_list_store_new (GPM_PROFILES_COLUMN_LAST, G_TYPE_STRING, G_TYPE_STRING,
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

	/* track in seporate arrays */
	profiles_array = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	profiles_array_in_combo = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);

	main_window = GTK_WIDGET (gtk_builder_get_object (builder, "dialog_prefs"));

	/* Hide window first so that the dialogue resizes itself without redrawing */
	gtk_widget_hide (main_window);
	gtk_window_set_icon_name (GTK_WINDOW (main_window), GCM_STOCK_ICON);
	g_signal_connect (main_window, "delete_event",
			  G_CALLBACK (gcm_prefs_delete_event_cb), loop);

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "button_close"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gcm_prefs_close_cb), loop);
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
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "button_profile_import"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gcm_prefs_profile_import_cb), NULL);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "expander1"));
	gtk_widget_set_sensitive (widget, FALSE);

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

	/* setup rendering lists */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "combobox_rendering_display"));
	gcm_prefs_set_combo_simple_text (widget);
	gtk_widget_set_sensitive (widget, FALSE);
	gcm_prefs_setup_rendering_combobox (widget);
	g_signal_connect (G_OBJECT (widget), "changed",
			  G_CALLBACK (gcm_prefs_renderer_combo_changed_cb), NULL);
	//TODO: need to get from GConf
	gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "combobox_rendering_softproof"));
	gcm_prefs_set_combo_simple_text (widget);
	gtk_widget_set_sensitive (widget, FALSE);
	gcm_prefs_setup_rendering_combobox (widget);
	g_signal_connect (G_OBJECT (widget), "changed",
			  G_CALLBACK (gcm_prefs_renderer_combo_changed_cb), NULL);
	//TODO: need to get from GConf
	gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "combobox_space_rgb"));
	gcm_prefs_set_combo_simple_text (widget);
	gtk_widget_set_sensitive (widget, FALSE);
	gcm_prefs_setup_space_combobox (widget);
	g_signal_connect (G_OBJECT (widget), "changed",
			  G_CALLBACK (gcm_prefs_space_combo_changed_cb), NULL);
	//TODO: need to get from GConf
	gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "combobox_space_cmyk"));
	gcm_prefs_set_combo_simple_text (widget);
	gtk_widget_set_sensitive (widget, FALSE);
	gcm_prefs_setup_space_combobox (widget);
	g_signal_connect (G_OBJECT (widget), "changed",
			  G_CALLBACK (gcm_prefs_space_combo_changed_cb), NULL);
	//TODO: need to get from GConf
	gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);

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

	/* get screen */
	rr_screen = gnome_rr_screen_new (gdk_screen_get_default (), NULL, NULL, &error);
	if (rr_screen == NULL) {
		egg_warning ("failed to get rr screen: %s", error->message);
		goto out;
	}

	/* use a device client array */
	gcm_client = gcm_client_new ();
	g_signal_connect (gcm_client, "added", G_CALLBACK (gcm_prefs_added_cb), NULL);
	g_signal_connect (gcm_client, "removed", G_CALLBACK (gcm_prefs_removed_cb), NULL);

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
	info_bar = gtk_info_bar_new ();

	/* TRANSLATORS: this is displayed while the devices are being probed */
	info_bar_label = gtk_label_new (_("Loading list of devices..."));
	gtk_info_bar_set_message_type (GTK_INFO_BAR(info_bar), GTK_MESSAGE_INFO);
	widget = gtk_info_bar_get_content_area (GTK_INFO_BAR(info_bar));
	gtk_container_add (GTK_CONTAINER(widget), info_bar_label);
	gtk_widget_show (info_bar_label);

	/* add infobar to devices pane */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "vbox_devices"));
	gtk_box_pack_start (GTK_BOX(widget), info_bar, FALSE, FALSE, 0);

	/* show main UI */
	gtk_window_set_default_size (GTK_WINDOW (main_window), 700, 280);
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
	if (use_global && use_atom) {
		widget = GTK_WIDGET (gtk_builder_get_object (builder, "radiobutton_ouput_both"));
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
	} else if (use_global) {
		widget = GTK_WIDGET (gtk_builder_get_object (builder, "radiobutton_ouput_global"));
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
	} else if (use_atom) {
		widget = GTK_WIDGET (gtk_builder_get_object (builder, "radiobutton_ouput_atom"));
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
	} else {
		widget = GTK_WIDGET (gtk_builder_get_object (builder, "radiobutton_ouput_disable"));
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
	}

	/* now connect radiobuttons */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "radiobutton_ouput_global"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gcm_prefs_radio_cb), NULL);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "radiobutton_ouput_atom"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gcm_prefs_radio_cb), NULL);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "radiobutton_ouput_both"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gcm_prefs_radio_cb), NULL);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "radiobutton_ouput_disable"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gcm_prefs_radio_cb), NULL);

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
	if (rr_screen != NULL)
		gnome_rr_screen_destroy (rr_screen);
	if (client != NULL)
		g_object_unref (client);
	if (gconf_client != NULL)
		g_object_unref (gconf_client);
	if (builder != NULL)
		g_object_unref (builder);
	if (profiles_array != NULL)
		g_ptr_array_unref (profiles_array);
	if (profiles_array_in_combo != NULL)
		g_ptr_array_unref (profiles_array_in_combo);
	if (gcm_client != NULL)
		g_object_unref (gcm_client);
	return retval;
}


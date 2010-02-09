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

#include "egg-debug.h"

#include "gcm-calibrate-argyll.h"
#include "gcm-cie-widget.h"
#include "gcm-client.h"
#include "gcm-color-device.h"
#include "gcm-device-xrandr.h"
#include "gcm-profile.h"
#include "gcm-profile-store.h"
#include "gcm-trc-widget.h"
#include "gcm-utils.h"
#include "gcm-xyz.h"

/* DISTROS: you will have to patch if you have changed the name of these packages */
#define GCM_PREFS_PACKAGE_NAME_SHARED_COLOR_TARGETS	"shared-color-targets"
#define GCM_PREFS_PACKAGE_NAME_ARGYLLCMS		"argyllcms"

static GtkBuilder *builder = NULL;
static GtkListStore *list_store_devices = NULL;
static GtkListStore *list_store_profiles = NULL;
static GcmDevice *current_device = NULL;
static GcmProfileStore *profile_store = NULL;
static GcmClient *gcm_client = NULL;
static GcmColorDevice *color_device = NULL;
static gboolean setting_up_device = FALSE;
static GtkWidget *info_bar = NULL;
static GtkWidget *cie_widget = NULL;
static GtkWidget *trc_widget = NULL;
static guint loading_refcount = 0;
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
		egg_warning ("failed to set default: %s", error->message);
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
		description = g_strdup (_("Other profile..."));
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
gcm_prefs_calibrate_display (GcmCalibrate *calibrate)
{
	gboolean ret = FALSE;
	gboolean ret_tmp;
	GError *error = NULL;
	gchar *output_name = NULL;
	gchar *basename = NULL;
	gchar *manufacturer = NULL;
	gchar *model = NULL;
	gchar *description = NULL;
	gchar *device = NULL;
	GtkWindow *window;

	/* no device */
	if (current_device == NULL)
		goto out;

	/* get the device */
	g_object_get (current_device,
		      "native-device", &output_name,
		      "serial", &basename,
		      "manufacturer", &manufacturer,
		      "model", &model,
		      "title", &description,
		      NULL);
	if (output_name == NULL) {
		egg_warning ("failed to get output");
		goto out;
	}

	/* get a filename based on the serial number */
	basename = gcm_prefs_calibrate_get_basename (current_device);

	/* get model */
	if (model == NULL) {
		/* TRANSLATORS: this is saved in the profile */
		model = g_strdup (_("Unknown model"));
	}

	/* get description */
	if (description == NULL) {
		/* TRANSLATORS: this is saved in the profile */
		description = g_strdup (_("Unknown display"));
	}

	/* get manufacturer */
	if (manufacturer == NULL) {
		/* TRANSLATORS: this is saved in the profile */
		manufacturer = g_strdup (_("Unknown manufacturer"));
	}

	/* get calibration device model */
	g_object_get (color_device,
		      "model", &device,
		      NULL);

	/* get device, harder */
	if (device == NULL) {
		/* TRANSLATORS: this is the formattted custom profile description. "Custom" refers to the fact that it's user generated */
		device = g_strdup (_("Custom"));
	}

	/* set the proper output name */
	g_object_set (calibrate,
		      "output-name", output_name,
		      "basename", basename,
		      "model", model,
		      "description", description,
		      "manufacturer", manufacturer,
		      "device", device,
		      NULL);

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
	ret_tmp = gcm_device_xrandr_set_gamma (GCM_DEVICE_XRANDR (current_device), &error);
	if (!ret_tmp) {
		egg_warning ("failed to set output gamma: %s", error->message);
		g_error_free (error);
	}

	g_free (device);
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
	gtk_file_filter_add_mime_type (filter, "application/x-it87");

	/* we can remove this when we depend on a new shared-mime-info */
	gtk_file_filter_add_pattern (filter, "*.txt");
	gtk_file_filter_add_pattern (filter, "*.TXT");
	gtk_file_filter_add_pattern (filter, "*.it8");
	gtk_file_filter_add_pattern (filter, "*.IT8");

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
 * gcm_prefs_get_device_for_it8_file:
 **/
static gchar *
gcm_prefs_get_device_for_it8_file (const gchar *filename)
{
	gchar *contents = NULL;
	gchar **lines = NULL;
	gchar *device = NULL;
	gboolean ret;
	GError *error = NULL;
	guint i;

	/* get contents */
	ret = g_file_get_contents (filename, &contents, NULL, &error);
	if (!ret) {
		egg_warning ("failed to get contents: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* split */
	lines = g_strsplit (contents, "\n", 15);
	for (i=0; lines[i] != NULL; i++) {
		if (!g_str_has_prefix (lines[i], "ORIGINATOR"))
			continue;

		/* copy, without the header or double quotes */
		device = g_strdup (lines[i]+12);
		g_strdelimit (device, "\"", '\0');
		break;
	}
out:
	g_free (contents);
	g_strfreev (lines);
	return device;
}

/**
 * gcm_prefs_reference_kind_to_localised_string:
 **/
static const gchar *
gcm_prefs_reference_kind_to_localised_string (GcmCalibrateArgyllReferenceKind kind)
{
	if (kind == GCM_CALIBRATE_ARGYLL_REFERENCE_KIND_CMP_DIGITAL_TARGET_3) {
		/* TRANSLATORS: this is probably a brand name */
		return _("CMP Digital Target 3");
	}
	if (kind == GCM_CALIBRATE_ARGYLL_REFERENCE_KIND_CMP_DT_003) {
		/* TRANSLATORS: this is probably a brand name */
		return _("CMP DT 003");
	}
	if (kind == GCM_CALIBRATE_ARGYLL_REFERENCE_KIND_COLOR_CHECKER) {
		/* TRANSLATORS: this is probably a brand name */
		return _("Color Checker");
	}
	if (kind == GCM_CALIBRATE_ARGYLL_REFERENCE_KIND_COLOR_CHECKER_DC) {
		/* TRANSLATORS: this is probably a brand name */
		return _("Color Checker DC");
	}
	if (kind == GCM_CALIBRATE_ARGYLL_REFERENCE_KIND_COLOR_CHECKER_SG) {
		/* TRANSLATORS: this is probably a brand name */
		return _("Color Checker SG");
	}
	if (kind == GCM_CALIBRATE_ARGYLL_REFERENCE_KIND_HUTCHCOLOR) {
		/* TRANSLATORS: this is probably a brand name */
		return _("Hutchcolor");
	}
	if (kind == GCM_CALIBRATE_ARGYLL_REFERENCE_KIND_I1_RGB_SCAN_1_4) {
		/* TRANSLATORS: this is probably a brand name */
		return _("i1 RGB Scan 1.4");
	}
	if (kind == GCM_CALIBRATE_ARGYLL_REFERENCE_KIND_IT8) {
		/* TRANSLATORS: this is probably a brand name */
		return _("IT8.7/2");
	}
	if (kind == GCM_CALIBRATE_ARGYLL_REFERENCE_KIND_LASER_SOFT_DC_PRO) {
		/* TRANSLATORS: this is probably a brand name */
		return _("Laser Soft DC Pro");
	}
	if (kind == GCM_CALIBRATE_ARGYLL_REFERENCE_KIND_QPCARD_201) {
		/* TRANSLATORS: this is probably a brand name */
		return _("QPcard 201");
	}
	return NULL;
}

/**
 * gcm_prefs_reference_kind_to_image_filename:
 **/
static const gchar *
gcm_prefs_reference_kind_to_image_filename (GcmCalibrateArgyllReferenceKind kind)
{
	if (kind == GCM_CALIBRATE_ARGYLL_REFERENCE_KIND_CMP_DIGITAL_TARGET_3)
		return "CMP-DigitalTarget3.png";
	if (kind == GCM_CALIBRATE_ARGYLL_REFERENCE_KIND_CMP_DT_003)
		return NULL;
	if (kind == GCM_CALIBRATE_ARGYLL_REFERENCE_KIND_COLOR_CHECKER)
		return "ColorChecker24.png";
	if (kind == GCM_CALIBRATE_ARGYLL_REFERENCE_KIND_COLOR_CHECKER_DC)
		return "ColorCheckerDC.png";
	if (kind == GCM_CALIBRATE_ARGYLL_REFERENCE_KIND_COLOR_CHECKER_SG)
		return NULL;
	if (kind == GCM_CALIBRATE_ARGYLL_REFERENCE_KIND_HUTCHCOLOR)
		return NULL;
	if (kind == GCM_CALIBRATE_ARGYLL_REFERENCE_KIND_I1_RGB_SCAN_1_4)
		return NULL;
	if (kind == GCM_CALIBRATE_ARGYLL_REFERENCE_KIND_IT8)
		return "IT872.png";
	if (kind == GCM_CALIBRATE_ARGYLL_REFERENCE_KIND_LASER_SOFT_DC_PRO)
		return NULL;
	if (kind == GCM_CALIBRATE_ARGYLL_REFERENCE_KIND_QPCARD_201)
		return "QPcard_201.png";
	return NULL;
}

/**
 * gcm_prefs_reference_kind_combobox_cb:
 **/
static void
gcm_prefs_reference_kind_combobox_cb (GtkComboBox *combo_box, GtkImage *image)
{
	GcmCalibrateArgyllReferenceKind reference_kind;
	const gchar *filename;
	gchar *path;

	/* not sorted so we can just use the index */
	reference_kind = gtk_combo_box_get_active (GTK_COMBO_BOX (combo_box));
	filename = gcm_prefs_reference_kind_to_image_filename (reference_kind);

	/* fallback */
	if (filename == NULL)
		filename = "unknown.png";

	path = g_build_filename (GCM_DATA, "targets", filename, NULL);
	gtk_image_set_from_file (GTK_IMAGE (image), path);
	g_free (path);
}

/**
 * gcm_prefs_get_reference_kind:
 **/
static GcmCalibrateArgyllReferenceKind
gcm_prefs_get_reference_kind ()
{
	GtkWindow *window;
	GtkResponseType response;
	GtkWidget *dialog;
	GtkWidget *vbox;
	GtkWidget *combo_box;
	GtkWidget *image;
	const gchar *title;
	const gchar *message;
	guint i;
	GcmCalibrateArgyllReferenceKind reference_kind = GCM_CALIBRATE_ARGYLL_REFERENCE_KIND_UNKNOWN;

	window = GTK_WINDOW(gtk_builder_get_object (builder, "dialog_prefs"));

	/* TRANSLATORS: this is the window title for when the user selects the chart type. A chart is a type of reference image the user has purchased. */
	title = _("Please select chart type");

	/* TRANSLATORS: this is the message body for the chart selection */
	message = _("Please select the chart type which corresponds to your reference file.");

	dialog = gtk_message_dialog_new (window, GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_QUESTION, GTK_BUTTONS_CANCEL, "%s", title);
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", message);
	gtk_window_set_icon_name (GTK_WINDOW (dialog), GCM_STOCK_ICON);
	/* TRANSLATORS: button, confirm the chart type */
	gtk_dialog_add_button (GTK_DIALOG (dialog), _("Use this type"), GTK_RESPONSE_YES);

	/* use image to display a picture of the target */
	image = gtk_image_new ();
	gtk_widget_set_size_request (image, 200, 140);

	/* create the combobox */
	combo_box = gtk_combo_box_new_text ();
	g_signal_connect (combo_box, "changed", G_CALLBACK (gcm_prefs_reference_kind_combobox_cb), image);

	/* add the list of charts */
	for (i = 0; i < GCM_CALIBRATE_ARGYLL_REFERENCE_KIND_UNKNOWN; i++) {
		gtk_combo_box_append_text (GTK_COMBO_BOX (combo_box),
					   gcm_prefs_reference_kind_to_localised_string (i));
	}

	/* use IT8 by default */
	gtk_combo_box_set_active (GTK_COMBO_BOX (combo_box), GCM_CALIBRATE_ARGYLL_REFERENCE_KIND_IT8);

	/* pack it */
	vbox = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

	gtk_box_pack_start (GTK_BOX(vbox), image, TRUE, TRUE, 6);
	gtk_box_pack_start (GTK_BOX(vbox), combo_box, TRUE, TRUE, 6);
	gtk_widget_show (combo_box);
	gtk_widget_show (image);

	/* run the dialog */
	response = gtk_dialog_run (GTK_DIALOG (dialog));

	/* not sorted so we can just use the index */
	reference_kind = gtk_combo_box_get_active (GTK_COMBO_BOX (combo_box));

	/* nuke the UI */
	gtk_widget_destroy (dialog);
	if (response != GTK_RESPONSE_YES) {
		reference_kind = GCM_CALIBRATE_ARGYLL_REFERENCE_KIND_UNKNOWN;
		goto out;
	}
out:
	return reference_kind;
}

/**
 * gcm_prefs_calibrate_device:
 **/
static gboolean
gcm_prefs_calibrate_device (GcmCalibrate *calibrate)
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
	gchar *device = NULL;
	const gchar *directory;
	GtkWindow *window;
	GString *string;
	GtkResponseType response;
	GtkWidget *dialog;
	const gchar *title;
	GcmCalibrateArgyllReferenceKind reference_kind;

	window = GTK_WINDOW(gtk_builder_get_object (builder, "dialog_prefs"));
	string = g_string_new ("");

	/* install shared-color-targets package */
	has_shared_targets = g_file_test ("/usr/share/shared-color-targets", G_FILE_TEST_IS_DIR);
	if (!has_shared_targets) {
#ifdef GCM_USE_PACKAGEKIT
		/* ask the user to confirm */
		dialog = gtk_message_dialog_new (window, GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
						 /* TRANSLATORS: title, usually we can tell based on the EDID data or output name */
						 _("Install missing files?"));

		/* TRANSLATORS: dialog message saying the color targets are not installed */
		g_string_append_printf (string, "%s ", _("Common color target files are not installed on this computer."));
		/* TRANSLATORS: dialog message saying the color targets are not installed */
		g_string_append_printf (string, "%s\n\n", _("Color target files are needed to convert the image to a color profile."));
		/* TRANSLATORS: dialog message, asking if it's okay to install them */
		g_string_append_printf (string, "%s\n\n", _("Do you want them to be automatically installed?"));
		/* TRANSLATORS: dialog message, if the user has the target file on a CDROM then there's no need for this package */
		g_string_append_printf (string, "%s", _("If you have already have the correct file then you can skip this step."));

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
			has_shared_targets = gcm_utils_install_package (GCM_PREFS_PACKAGE_NAME_SHARED_COLOR_TARGETS, window);
#else
		egg_warning ("cannot install: this package was not compiled with --enable-packagekit");
#endif
	}

	/* TRANSLATORS: title, we're setting up the device ready for calibration */
	title = _("Setting up device");
	g_string_set_size (string, 0);

	/* TRANSLATORS: dialog message, preface */
	g_string_append_printf (string, "%s\n", _("Before calibrating the device, you have to manually acquire a reference image and save it as a TIFF image file."));

	/* TRANSLATORS: dialog message, preface */
	g_string_append_printf (string, "%s\n", _("Ensure that the contrast and brightness is not changed and color correction profiles are not applied."));

	/* TRANSLATORS: dialog message, suffix */
	g_string_append_printf (string, "%s\n", _("The device sensor should have been cleaned prior to scanning and the output file resolution should be at least 200dpi."));

	/* TRANSLATORS: dialog message, suffix */
	g_string_append_printf (string, "\n%s\n", _("For best results, the reference image should also be less than two years old."));

	/* TRANSLATORS: dialog question */
	g_string_append_printf (string, "\n%s", _("Do you have a scanned TIFF file of the reference image?"));

	/* ask the user to confirm */
	window = GTK_WINDOW(gtk_builder_get_object (builder, "dialog_prefs"));
	dialog = gtk_message_dialog_new (window, GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_QUESTION, GTK_BUTTONS_CANCEL, "%s", title);
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", string->str);
	gtk_window_set_icon_name (GTK_WINDOW (dialog), GCM_STOCK_ICON);
	/* TRANSLATORS: button, confirm the user has a file */
	gtk_dialog_add_button (GTK_DIALOG (dialog), _("I have already scanned in a file"), GTK_RESPONSE_YES);
	response = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
	if (response != GTK_RESPONSE_YES)
		goto out;

	/* get the device */
	g_object_get (current_device,
		      "model", &model,
		      "title", &description,
		      "manufacturer", &manufacturer,
		      NULL);

	/* set the reference kind */
	reference_kind = gcm_prefs_get_reference_kind ();
	if (reference_kind == GCM_CALIBRATE_ARGYLL_REFERENCE_KIND_UNKNOWN) {
		ret = FALSE;
		goto out;
	}
	gcm_calibrate_argyll_set_reference_kind (GCM_CALIBRATE_ARGYLL (calibrate), reference_kind);

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

	/* use the ORIGINATOR in the it8 file */
	device = gcm_prefs_get_device_for_it8_file (reference_data);
	if (device == NULL)
		device = g_strdup ("IT8.7");

	/* set the calibration parameters */
	g_object_set (calibrate,
		      "basename", basename,
		      "model", model,
		      "description", description,
		      "manufacturer", manufacturer,
		      "filename-source", scanned_image,
		      "filename-reference", reference_data,
		      "device", device,
		      NULL);

	/* do each step */
	ret = gcm_calibrate_device (calibrate, window, &error);
	if (!ret) {
		egg_warning ("failed to calibrate: %s", error->message);
		g_error_free (error);
		goto out;
	}
out:
	if (string != NULL)
		g_string_free (string, TRUE);
	g_free (device);
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
static gchar *
gcm_prefs_file_chooser_get_icc_profile (void)
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
 * gcm_prefs_profile_import:
 **/
static gboolean
gcm_prefs_profile_import (const gchar *filename)
{
	GtkWidget *dialog;
	GError *error = NULL;
	gchar *destination = NULL;
	GtkWindow *window;
	gboolean ret;

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
out:
	g_free (destination);
	return ret;
}

/**
 * gcm_prefs_profile_import_uri:
 **/
static gboolean
gcm_prefs_profile_import_uri (const gchar *uri)
{
	GFile *file;
	gchar *path;
	gboolean ret = FALSE;

	/* resolve */
	file = g_file_new_for_uri (uri);
	path = g_file_get_path (file);
	if (path == NULL) {
		egg_warning ("failed to get path: %s", uri);
		goto out;
	}

	/* import */
	ret = gcm_prefs_profile_import (path);
out:
	g_free (path);
	g_object_unref (file);
	return ret;
}

/**
 * gcm_prefs_profile_import_cb:
 **/
static void
gcm_prefs_profile_import_cb (GtkWidget *widget, gpointer data)
{
	gchar *filename;

	/* get new file */
	filename = gcm_prefs_file_chooser_get_icc_profile ();
	if (filename == NULL)
		goto out;

	/* import this */
	gcm_prefs_profile_import (filename);

out:
	g_free (filename);
}

/**
 * gcm_prefs_drag_data_received_cb:
 **/
static void
gcm_prefs_drag_data_received_cb (GtkWidget *widget, GdkDragContext *context, gint x, gint y, GtkSelectionData *data, guint _time, gpointer user_data)
{
	const guchar *filename;
	gchar **filenames = NULL;
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

		/* check this is an ICC profile */
		ret = gcm_utils_is_icc_profile (filenames[i]);
		if (!ret) {
			egg_debug ("%s is not a ICC profile", filenames[i]);
			gtk_drag_finish (context, FALSE, FALSE, _time);
			goto out;
		}

		/* try to import it */
		ret = gcm_prefs_profile_import_uri (filenames[i]);
		if (!ret) {
			egg_debug ("%s did not import correctly", filenames[i]);
			gtk_drag_finish (context, FALSE, FALSE, _time);
			goto out;
		}
	}

	gtk_drag_finish (context, TRUE, FALSE, _time);
out:
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
	gchar *destination = NULL;
	GcmProfile *profile;
	GPtrArray *profile_array = NULL;

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
	destination = gcm_utils_get_profile_destination (filename);
	ret = gcm_utils_mkdir_and_copy (filename, destination, &error);
	if (!ret) {
		egg_warning ("failed to calibrate: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* get new list */
	profile_array = gcm_profile_store_get_array (profile_store);

	/* find an existing profile of this name */
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
out:
	g_free (filename);
	g_free (destination);
	if (profile_array != NULL)
		g_ptr_array_unref (profile_array);
	if (calibrate != NULL)
		g_object_unref (calibrate);
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
	GcmDeviceTypeEnum type;
	gboolean connected;

	/* no device selected */
	if (current_device == NULL)
		goto out;

	/* get current device properties */
	g_object_get (current_device,
		      "type", &type,
		      "connected", &connected,
		      NULL);

	/* are we a display */
	if (type == GCM_DEVICE_TYPE_ENUM_DISPLAY) {

		/* are we disconnected */
		if (!connected)
			goto out;

		/* find whether we have hardware installed */
		g_object_get (color_device,
			      "present", &ret,
			      NULL);
#ifndef GCM_HARDWARE_DETECTION
		egg_debug ("overriding device presence %i with TRUE", ret);
		ret = TRUE;
#endif
	} else if (type == GCM_DEVICE_TYPE_ENUM_SCANNER ||
		   type == GCM_DEVICE_TYPE_ENUM_CAMERA) {

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
 * gcm_prefs_is_profile_suitable_for_device:
 **/
static gboolean
gcm_prefs_is_profile_suitable_for_device (GcmProfile *profile, GcmDevice *device)
{
	GcmProfileTypeEnum profile_type_tmp;
	GcmProfileTypeEnum profile_type;
	GcmColorspaceEnum profile_colorspace;
	GcmColorspaceEnum device_colorspace;
	gboolean has_vcgt;
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
		      "has-vcgt", &has_vcgt,
		      NULL);

	/* not the right colorspace */
	if (device_colorspace != profile_colorspace)
		goto out;

	/* not the correct type */
	profile_type = gcm_utils_device_type_to_profile_type (device_type);
	if (profile_type_tmp != profile_type)
		goto out;

#if 0
	/* no VCGT for a display (is a crap profile) */
	if (profile_type_tmp == GCM_PROFILE_TYPE_ENUM_DISPLAY_DEVICE && !has_vcgt)
		goto out;
#endif

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
		gtk_label_set_label (GTK_LABEL (widget), _("Yes"));
	} else {
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
	gchar *title_tmp;
	gchar *title;
	gchar *sort = NULL;
	gchar *id;
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
		ret = gcm_device_xrandr_set_gamma (GCM_DEVICE_XRANDR (device), &error);
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
	gchar *filename = NULL;
	gboolean ret;
	GError *error = NULL;
	GcmProfile *profile = NULL;
	gboolean changed;
	GcmDeviceTypeEnum type;
	GtkTreeIter iter;
	GtkTreeModel *model;
	GcmPrefsEntryType entry_type;

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
		filename = gcm_prefs_file_chooser_get_icc_profile ();
		if (filename == NULL) {
			gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
			goto out;
		}
		gcm_prefs_profile_import (filename);
		goto out;
	}

	/* get profile filename */
	if (entry_type == GCM_PREFS_ENTRY_TYPE_PROFILE) {
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
		if (type == GCM_DEVICE_TYPE_ENUM_DISPLAY) {
			ret = gcm_device_xrandr_set_gamma (GCM_DEVICE_XRANDR (current_device), &error);
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
	ret = gcm_device_xrandr_set_gamma (GCM_DEVICE_XRANDR (current_device), &error);
	if (!ret) {
		egg_warning ("failed to set output gamma: %s", error->message);
		g_error_free (error);
		goto out;
	}
out:
	return;
}

/**
 * gcm_prefs_color_device_changed_cb:
 **/
static void
gcm_prefs_color_device_changed_cb (GcmColorDevice *_color_device, gpointer user_data)
{
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
	if (type == GCM_DEVICE_TYPE_ENUM_DISPLAY)
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
		text = g_strdup_printf (_("No %s color spaces available"), search);
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
	GcmDeviceTypeEnum type;
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
		if (type != GCM_DEVICE_TYPE_ENUM_DISPLAY)
			continue;

		/* set gamma for device */
		ret = gcm_device_xrandr_set_gamma (GCM_DEVICE_XRANDR (device), &error);
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
	g_signal_connect (gcm_client, "added", G_CALLBACK (gcm_prefs_added_cb), NULL);
	g_signal_connect (gcm_client, "removed", G_CALLBACK (gcm_prefs_removed_cb), NULL);

	/* use the color device */
	color_device = gcm_color_device_new ();
	g_signal_connect (color_device, "changed", G_CALLBACK (gcm_prefs_color_device_changed_cb), NULL);

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
	if (color_device != NULL)
		g_object_unref (color_device);
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


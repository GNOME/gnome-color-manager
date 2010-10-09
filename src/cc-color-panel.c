/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
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

#include "egg-debug.h"

#include "gcm-cell-renderer-profile.h"
#include "gcm-calibrate-argyll.h"
#include "gcm-cie-widget.h"
#include "gcm-client.h"
#include "gcm-sensor-client.h"
#include "gcm-device-xrandr.h"
#include "gcm-device-virtual.h"
#include "gcm-exif.h"
#include "gcm-profile.h"
#include "gcm-profile-store.h"
#include "gcm-trc-widget.h"
#include "gcm-utils.h"
#include "gcm-color.h"

#include "egg-debug.h"

#include "cc-color-panel.h"

struct _CcColorPanelPrivate {
	GtkBuilder		*builder;
	GtkListStore		*list_store_devices;
	GtkListStore		*list_store_assign;
	GcmDevice		*current_device;
	GcmProfileStore		*profile_store;
	GcmClient		*gcm_client;
	GcmSensorClient		*sensor_client;
	gboolean		 setting_up_device;
	GtkWidget		*main_window;
	GtkWidget		*info_bar_loading;
	GtkWidget		*info_bar_vcgt;
	GtkWidget		*info_bar_profiles;
	GSettings		*settings;
	guint			 save_and_apply_id;
	guint			 apply_all_devices_id;
};

G_DEFINE_DYNAMIC_TYPE (CcColorPanel, cc_color_panel, CC_TYPE_PANEL)

static void cc_color_panel_finalize (GObject *object);

#define CC_COLOR_PREFS_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_COLOR_PANEL, CcColorPanelPrivate))

enum {
	GCM_DEVICES_COLUMN_ID,
	GCM_DEVICES_COLUMN_SORT,
	GCM_DEVICES_COLUMN_ICON,
	GCM_DEVICES_COLUMN_TITLE,
	GCM_DEVICES_COLUMN_LAST
};

enum {
	GCM_ASSIGN_COLUMN_SORT,
	GCM_ASSIGN_COLUMN_PROFILE,
	GCM_ASSIGN_COLUMN_IS_DEFAULT,
	GCM_ASSIGN_COLUMN_LAST
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

static void cc_color_panel_devices_treeview_clicked_cb (GtkTreeSelection *selection, CcColorPanel *panel);
static void cc_color_panel_profile_store_changed_cb (GcmProfileStore *profile_store, CcColorPanel *panel);

/**
 * cc_color_panel_error_dialog:
 **/
static void
cc_color_panel_error_dialog (CcColorPanel *panel, const gchar *title, const gchar *message)
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
 * cc_color_panel_set_default:
 **/
static gboolean
cc_color_panel_set_default (CcColorPanel *panel, GcmDevice *device)
{
	GError *error = NULL;
	gboolean ret = FALSE;
	gchar *cmdline = NULL;
	const gchar *filename;
	const gchar *id;
	gchar *install_cmd = NULL;

	/* nothing set */
	id = gcm_device_get_id (device);
	filename = gcm_device_get_default_profile_filename (device);
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
		cc_color_panel_error_dialog (panel, _("Failed to save defaults for all users"), error->message);
		g_error_free (error);
		goto out;
	}
out:
	g_free (install_cmd);
	g_free (cmdline);
	return ret;
}

/**
 * cc_color_panel_combobox_add_profile:
 **/
static void
cc_color_panel_combobox_add_profile (GtkWidget *widget, GcmProfile *profile, GcmPrefsEntryType entry_type, GtkTreeIter *iter)
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
		description = gcm_profile_get_description (profile);
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
	GcmDevice *device;
	GcmDeviceKind kind;
	gboolean ret;
	guint i;

	/* set for each output */
	array = gcm_client_get_devices (panel->priv->gcm_client);
	for (i=0; i<array->len; i++) {
		device = g_ptr_array_index (array, i);

		/* not a xrandr panel */
		kind = gcm_device_get_kind (device);
		if (kind != GCM_DEVICE_KIND_DISPLAY)
			continue;

		/* set for this device */
		ret = cc_color_panel_set_default (panel, device);
		if (!ret)
			break;
	}
	g_ptr_array_unref (array);
}

/**
 * cc_color_panel_help_cb:
 **/
static void
cc_color_panel_help_cb (GtkWidget *widget, CcColorPanel *panel)
{
	gcm_gnome_help ("preferences");
}

/**
 * cc_color_panel_viewer_cb:
 **/
static void
cc_color_panel_viewer_cb (GtkWidget *widget, CcColorPanel *panel)
{
	gboolean ret;
	GError *error = NULL;
	guint xid;
	gchar *command;

	/* get xid */
	xid = gdk_x11_drawable_get_xid (gtk_widget_get_window (GTK_WIDGET (panel->priv->main_window)));

	/* run with modal set */
	command = g_strdup_printf ("%s/gcm-viewer --parent-window %u", BINDIR, xid);
	egg_debug ("running: %s", command);
	ret = g_spawn_command_line_async (command, &error);
	if (!ret) {
		egg_warning ("failed to run prefs: %s", error->message);
		g_error_free (error);
	}
	g_free (command);
}

/**
 * cc_color_panel_calibrate_display:
 **/
static gboolean
cc_color_panel_calibrate_display (CcColorPanel *panel, GcmCalibrate *calibrate)
{
	gboolean ret = FALSE;
	gboolean ret_tmp;
	GError *error = NULL;
	GtkWindow *window;

	/* no device */
	if (panel->priv->current_device == NULL)
		goto out;

	/* set properties from the device */
	ret = gcm_calibrate_set_from_device (calibrate, panel->priv->current_device, &error);
	if (!ret) {
		egg_warning ("failed to calibrate: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* run each task in order */
	window = GTK_WINDOW(panel->priv->main_window);
	ret = gcm_calibrate_display (calibrate, window, &error);
	if (!ret) {
		egg_warning ("failed to calibrate: %s", error->message);
		g_error_free (error);
		goto out;
	}
out:
	/* need to set the gamma back to the default after calibration */
	error = NULL;
	ret_tmp = gcm_device_apply (panel->priv->current_device, &error);
	if (!ret_tmp) {
		egg_warning ("failed to apply profile: %s", error->message);
		g_error_free (error);
	}
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

	/* set defaults from device */
	ret = gcm_calibrate_set_from_device (calibrate, panel->priv->current_device, &error);
	if (!ret) {
		egg_warning ("failed to calibrate: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* do each step */
	window = GTK_WINDOW(panel->priv->main_window);
	ret = gcm_calibrate_device (calibrate, window, &error);
	if (!ret) {
		if (error->code != GCM_CALIBRATE_ERROR_USER_ABORT) {
			/* TRANSLATORS: could not calibrate */
			cc_color_panel_error_dialog (panel, _("Failed to calibrate device"), error->message);
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
 * cc_color_panel_calibrate_printer:
 **/
static gboolean
cc_color_panel_calibrate_printer (CcColorPanel *panel, GcmCalibrate *calibrate)
{
	gboolean ret = FALSE;
	GError *error = NULL;
	GtkWindow *window;

	/* set defaults from device */
	ret = gcm_calibrate_set_from_device (calibrate, panel->priv->current_device, &error);
	if (!ret) {
		egg_warning ("failed to calibrate: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* do each step */
	window = GTK_WINDOW(panel->priv->main_window);
	ret = gcm_calibrate_printer (calibrate, window, &error);
	if (!ret) {
		if (error->code != GCM_CALIBRATE_ERROR_USER_ABORT) {
			/* TRANSLATORS: could not calibrate */
			cc_color_panel_error_dialog (panel, _("Failed to calibrate printer"), error->message);
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
	window = GTK_WINDOW(gtk_builder_get_object (panel->priv->builder, "dialog_assign"));
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
		egg_debug ("not a ICC profile");
		goto out;
	}

	/* copy icc file to ~/.color/icc */
	destination = gcm_utils_get_profile_destination (file);
	ret = gcm_utils_mkdir_and_copy (file, destination, &error);
	if (!ret) {
		/* TRANSLATORS: could not read file */
		cc_color_panel_error_dialog (panel, _("Failed to copy file"), error->message);
		g_error_free (error);
		goto out;
	}
out:
	if (destination != NULL)
		g_object_unref (destination);
	return ret;
}

/**
 * cc_color_panel_profile_add_virtual_file:
 **/
static gboolean
cc_color_panel_profile_add_virtual_file (CcColorPanel *panel, GFile *file)
{
	gboolean ret;
	GcmExif *exif;
	GError *error = NULL;
	GcmDevice *device = NULL;

	/* parse file */
	exif = gcm_exif_new ();
	ret = gcm_exif_parse (exif, file, &error);
	if (!ret) {
		/* TRANSLATORS: could not add virtual device */
		if (error->domain != GCM_EXIF_ERROR ||
		    error->code != GCM_EXIF_ERROR_NO_SUPPORT)
			cc_color_panel_error_dialog (panel, _("Failed to get metadata from image"), error->message);
		else
			egg_debug ("not a supported image format: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* create device */
	device = gcm_device_virtual_new	();
	ret = gcm_device_virtual_create_from_params (GCM_DEVICE_VIRTUAL (device),
						     gcm_exif_get_device_kind (exif),
						     gcm_exif_get_model (exif),
						     gcm_exif_get_manufacturer (exif),
						     gcm_exif_get_serial (exif),
						     GCM_COLORSPACE_RGB);
	if (!ret) {
		/* TRANSLATORS: could not add virtual device */
		cc_color_panel_error_dialog (panel, _("Failed to create virtual device"), NULL);
		goto out;
	}

	/* save what we've got */
	ret = gcm_device_save (device, &error);
	if (!ret) {
		/* TRANSLATORS: could not add virtual device */
		cc_color_panel_error_dialog (panel, _("Failed to save virtual device"), error->message);
		g_error_free (error);
		goto out;
	}

	/* add to the device list */
	ret = gcm_client_add_device (panel->priv->gcm_client, device, &error);
	if (!ret) {
		/* TRANSLATORS: could not add virtual device */
		cc_color_panel_error_dialog (panel, _("Failed to add virtual device"), error->message);
		g_error_free (error);
		goto out;
	}
out:
	g_object_unref (exif);
	if (device != NULL)
		g_object_unref (device);
	return ret;
}

/**
 * cc_color_panel_drag_data_received_cb:
 **/
static void
cc_color_panel_drag_data_received_cb (GtkWidget *widget, GdkDragContext *context, gint x, gint y, GtkSelectionData *data, guint _time, CcColorPanel *panel)
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
	egg_debug ("dropped: %p (%s)", data, filename);

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

		/* try to add a virtual profile with it */
		ret = cc_color_panel_profile_add_virtual_file (panel, file);
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
		egg_warning ("failed to parse file: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* set model and manufacturer */
	model = gcm_exif_get_model (exif);
	if (model != NULL) {
		widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "entry_virtual_model"));
		gtk_entry_set_text (GTK_ENTRY (widget), model);
	}
	manufacturer = gcm_exif_get_manufacturer (exif);
	if (manufacturer != NULL) {
		widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "entry_virtual_manufacturer"));
		gtk_entry_set_text (GTK_ENTRY (widget), manufacturer);
	}

	/* set type */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "combobox_virtual_type"));
	gtk_combo_box_set_active (GTK_COMBO_BOX(widget), GCM_DEVICE_KIND_CAMERA - 2);
out:
	g_object_unref (exif);
	return ret;
}

/**
 * cc_color_panel_virtual_drag_data_received_cb:
 **/
static void
cc_color_panel_virtual_drag_data_received_cb (GtkWidget *widget, GdkDragContext *context, gint x, gint y,
					 GtkSelectionData *data, guint info, guint _time, CcColorPanel *panel)
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
	egg_debug ("dropped: %p (%s)", data, filename);

	/* split, as multiple drag targets are accepted */
	filenames = g_strsplit_set ((const gchar *)filename, "\r\n", -1);
	for (i=0; filenames[i]!=NULL; i++) {

		/* blank entry */
		if (filenames[i][0] == '\0')
			continue;

		/* check this is an ICC profile */
		egg_debug ("trying to set %s", filenames[i]);
		file = g_file_new_for_uri (filenames[i]);
		ret = cc_color_panel_virtual_set_from_file (panel, file);
		if (!ret) {
			egg_debug ("%s did not set from file correctly", filenames[i]);
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
	egg_warning ("cannot install: this package was not compiled with --enable-packagekit");
	goto out;
#endif

	/* ask the user to confirm */
	window = GTK_WINDOW(panel->priv->main_window);
	dialog = gtk_message_dialog_new (window, GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
					 /* TRANSLATORS: title, usually we can tell based on the EDID data or output name */
					 _("Install calibration and profiling software?"));

	string = g_string_new ("");
	/* TRANSLATORS: dialog message saying the argyllcms is not installed */
	g_string_append_printf (string, "%s\n", _("Calibration and profiling software is not installed."));
	/* TRANSLATORS: dialog message saying the color targets are not installed */
	g_string_append_printf (string, "%s", _("These tools are required to build color profiles for devices."));

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
 * cc_color_panel_calibrate_cb:
 **/
static void
cc_color_panel_calibrate_cb (GtkWidget *widget, CcColorPanel *panel)
{
	GcmCalibrate *calibrate = NULL;
	GcmDeviceKind kind;
	gboolean ret;
	GError *error = NULL;
	const gchar *filename;
	guint i;
	const gchar *name;
	GcmProfile *profile;
	GPtrArray *profile_array = NULL;
	GFile *file = NULL;
	GFile *dest = NULL;
	gchar *destination = NULL;

	/* ensure argyllcms is installed */
	ret = cc_color_panel_ensure_argyllcms_installed (panel);
	if (!ret)
		goto out;

	/* create new calibration object */
	calibrate = GCM_CALIBRATE(gcm_calibrate_argyll_new ());

	/* choose the correct kind of calibration */
	kind = gcm_device_get_kind (panel->priv->current_device);
	switch (kind) {
	case GCM_DEVICE_KIND_DISPLAY:
		ret = cc_color_panel_calibrate_display (panel, calibrate);
		break;
	case GCM_DEVICE_KIND_SCANNER:
	case GCM_DEVICE_KIND_CAMERA:
		ret = cc_color_panel_calibrate_device (panel, calibrate);
		break;
	case GCM_DEVICE_KIND_PRINTER:
		ret = cc_color_panel_calibrate_printer (panel, calibrate);
		break;
	default:
		egg_warning ("calibration and/or profiling not supported for this device");
		goto out;
	}

	/* we failed to calibrate */
	if (!ret) {
		egg_warning ("failed to calibrate");
		goto out;
	}

	/* failed to get profile */
	filename = gcm_calibrate_get_filename_result (calibrate);
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

	/* find an existing profile of this name */
	profile_array = gcm_device_get_profiles (panel->priv->current_device);
	destination = g_file_get_path (dest);
	for (i=0; i<profile_array->len; i++) {
		profile = g_ptr_array_index (profile_array, i);
		name = gcm_profile_get_filename (profile);
		if (g_strcmp0 (name, destination) == 0) {
			egg_debug ("found existing profile: %s", destination);
			break;
		}
	}

	/* we didn't find an existing profile */
	if (i == profile_array->len) {
		egg_debug ("adding: %s", destination);

		/* set this default */
		gcm_device_set_default_profile_filename (panel->priv->current_device, destination);
		ret = gcm_device_save (panel->priv->current_device, &error);
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
			 CA_PROP_EVENT_DESCRIPTION, _("Profiling completed"), NULL);
out:
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
 * cc_color_panel_device_add_cb:
 **/
static void
cc_color_panel_device_add_cb (GtkWidget *widget, CcColorPanel *panel)
{
	/* show ui */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "dialog_virtual"));
	gtk_widget_show (widget);
	gtk_window_set_transient_for (GTK_WINDOW (widget), GTK_WINDOW (panel->priv->main_window));

	/* clear entries */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "combobox_virtual_type"));
	gtk_combo_box_set_active (GTK_COMBO_BOX(widget), 0);
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "entry_virtual_model"));
	gtk_entry_set_text (GTK_ENTRY (widget), "");
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "entry_virtual_manufacturer"));
	gtk_entry_set_text (GTK_ENTRY (widget), "");
}

/**
 * cc_color_panel_is_profile_suitable_for_device:
 **/
static gboolean
cc_color_panel_is_profile_suitable_for_device (GcmProfile *profile, GcmDevice *device)
{
	GcmProfileKind profile_kind_tmp;
	GcmProfileKind profile_kind;
	GcmColorspace profile_colorspace;
	GcmColorspace device_colorspace;
	gboolean ret = FALSE;
	GcmDeviceKind device_kind;

	/* not the right colorspace */
	device_colorspace = gcm_device_get_colorspace (device);
	profile_colorspace = gcm_profile_get_colorspace (profile);
	if (device_colorspace != profile_colorspace)
		goto out;

	/* not the correct kind */
	device_kind = gcm_device_get_kind (device);
	profile_kind_tmp = gcm_profile_get_kind (profile);
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
cc_color_panel_add_profiles_suitable_for_devices (CcColorPanel *panel, GtkWidget *widget, const gchar *profile_filename)
{
	GtkTreeModel *model;
	guint i;
	gboolean ret;
	GcmProfile *profile;
	GPtrArray *profile_array;
	GtkTreeIter iter;

	/* clear existing entries */
	model = gtk_combo_box_get_model (GTK_COMBO_BOX (widget));
	gtk_list_store_clear (GTK_LIST_STORE (model));

	/* get new list */
	profile_array = gcm_profile_store_get_array (panel->priv->profile_store);

	/* add profiles of the right kind */
	for (i=0; i<profile_array->len; i++) {
		profile = g_ptr_array_index (profile_array, i);

		/* don't add the current profile */
		if (g_strcmp0 (gcm_profile_get_filename (profile), profile_filename) == 0)
			continue;

		/* only add correct types */
		ret = cc_color_panel_is_profile_suitable_for_device (profile, panel->priv->current_device);
		if (!ret)
			continue;

		/* add */
		cc_color_panel_combobox_add_profile (widget, profile, GCM_PREFS_ENTRY_TYPE_PROFILE, &iter);
	}

	/* add a import entry */
	cc_color_panel_combobox_add_profile (widget, NULL, GCM_PREFS_ENTRY_TYPE_IMPORT, NULL);
	gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
	g_ptr_array_unref (profile_array);
}

/**
 * cc_color_panel_assign_save_profiles_for_device:
 **/
static void
cc_color_panel_assign_save_profiles_for_device (CcColorPanel *panel, GcmDevice *device)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	gboolean is_default;
	GcmProfile *profile;
	GPtrArray *array;
	gboolean ret;
	GError *error = NULL;

	/* create empty array */
	array = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);

	/* get first element */
	model = GTK_TREE_MODEL (panel->priv->list_store_assign);
	ret = gtk_tree_model_get_iter_first (model, &iter);
	if (!ret)
		goto set_profiles;

	/* add default device first */
	do {
		gtk_tree_model_get (model, &iter,
				    GCM_ASSIGN_COLUMN_PROFILE, &profile,
				    GCM_ASSIGN_COLUMN_IS_DEFAULT, &is_default,
				    -1);
		if (is_default)
			g_ptr_array_add (array, g_object_ref (profile));
		g_object_unref (profile);
	} while (gtk_tree_model_iter_next (model, &iter));

	/* add non-default devices next */
	gtk_tree_model_get_iter_first (model, &iter);
	do {
		gtk_tree_model_get (model, &iter,
				    GCM_ASSIGN_COLUMN_PROFILE, &profile,
				    GCM_ASSIGN_COLUMN_IS_DEFAULT, &is_default,
				    -1);
		if (!is_default)
			g_ptr_array_add (array, g_object_ref (profile));
		g_object_unref (profile);
	} while (gtk_tree_model_iter_next (model, &iter));

set_profiles:
	/* save new array */
	gcm_device_set_profiles (device, array);

	/* save */
	ret = gcm_device_save (panel->priv->current_device, &error);
	if (!ret) {
		egg_warning ("failed to save config: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* set the profile */
	ret = gcm_device_apply (panel->priv->current_device, &error);
	if (!ret) {
		egg_warning ("failed to apply profile: %s", error->message);
		g_error_free (error);
		goto out;
	}
out:
	g_ptr_array_unref (array);
}

/**
 * cc_color_panel_assign_add_cb:
 **/
static void
cc_color_panel_assign_add_cb (GtkWidget *widget, CcColorPanel *panel)
{
	const gchar *profile_filename;

	/* add profiles of the right kind */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "combobox_profile"));
	profile_filename = gcm_device_get_default_profile_filename (panel->priv->current_device);
	cc_color_panel_add_profiles_suitable_for_devices (panel, widget, profile_filename);

	/* show the dialog */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "dialog_assign"));
	gtk_widget_show (widget);
	gtk_window_set_transient_for (GTK_WINDOW (widget), GTK_WINDOW (panel->priv->main_window));
}

/**
 * cc_color_panel_assign_remove_cb:
 **/
static void
cc_color_panel_assign_remove_cb (GtkWidget *widget, CcColorPanel *panel)
{
	GtkTreeIter iter;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	gboolean is_default;
	gboolean ret;
	const gchar *device_md5;
	GcmProfile *profile = NULL;

	/* get the selected row */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "treeview_assign"));
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
		egg_debug ("no row selected");
		goto out;
	}

	/* if the profile is default, then we'll have to make the first profile default */
	gtk_tree_model_get (model, &iter,
			    GCM_ASSIGN_COLUMN_IS_DEFAULT, &is_default,
			    GCM_ASSIGN_COLUMN_PROFILE, &profile,
			    -1);

	/* if this is an auto-added profile that the user has *manually*
	 * removed, then assume there was something wrong with the profile
	 * and don't do this again on next session start */
	if (gcm_device_get_kind (panel->priv->current_device) == GCM_DEVICE_KIND_DISPLAY) {
		device_md5 = gcm_device_xrandr_get_edid_md5 (GCM_DEVICE_XRANDR (panel->priv->current_device));
		if (g_strstr_len (gcm_profile_get_filename (profile), -1, device_md5) != NULL) {
			egg_debug ("removed an auto-profile, so disabling add for device");
			gcm_device_set_use_edid_profile (panel->priv->current_device, FALSE);
		}
	}

	/* remove this entry */
	gtk_list_store_remove (GTK_LIST_STORE(model), &iter);

	/* /something/ has to be the default profile */
	if (is_default) {
		ret = gtk_tree_model_get_iter_first (model, &iter);
		if (ret) {
			gtk_list_store_set (panel->priv->list_store_assign, &iter,
					    GCM_ASSIGN_COLUMN_IS_DEFAULT, TRUE,
					    GCM_ASSIGN_COLUMN_SORT, "0",
					    -1);
			do {
				gtk_list_store_set (panel->priv->list_store_assign, &iter,
						    GCM_ASSIGN_COLUMN_SORT, "1",
						    -1);
			} while (gtk_tree_model_iter_next (model, &iter));
		}
	}

	/* save device */
	cc_color_panel_assign_save_profiles_for_device (panel, panel->priv->current_device);
out:
	if (profile != NULL)
		g_object_unref (profile);
	return;
}

/**
 * cc_color_panel_assign_make_default_internal:
 **/
static void
cc_color_panel_assign_make_default_internal (CcColorPanel *panel, GtkTreeModel *model, GtkTreeIter *iter_selected)
{
	GtkTreeIter iter;
	GtkWidget *widget;

	/* make none of the devices default */
	gtk_tree_model_get_iter_first (model, &iter);
	do {
		gtk_list_store_set (panel->priv->list_store_assign, &iter,
				    GCM_ASSIGN_COLUMN_SORT, "1",
				    GCM_ASSIGN_COLUMN_IS_DEFAULT, FALSE,
				    -1);
	} while (gtk_tree_model_iter_next (model, &iter));

	/* make the selected device default */
	gtk_list_store_set (panel->priv->list_store_assign, iter_selected,
			    GCM_ASSIGN_COLUMN_IS_DEFAULT, TRUE,
			    GCM_ASSIGN_COLUMN_SORT, "0",
			    -1);

	/* set button insensitive */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "button_assign_make_default"));
	gtk_widget_set_sensitive (widget, FALSE);

	/* save device */
	cc_color_panel_assign_save_profiles_for_device (panel, panel->priv->current_device);
}

/**
 * cc_color_panel_assign_make_default_cb:
 **/
static void
cc_color_panel_assign_make_default_cb (GtkWidget *widget, CcColorPanel *panel)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	GtkTreeSelection *selection;

	/* get the selected row */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "treeview_assign"));
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
		egg_debug ("no row selected");
		return;
	}

	/* make this profile the default */
	cc_color_panel_assign_make_default_internal (panel, model, &iter);
}

/**
 * cc_color_panel_button_virtual_add_cb:
 **/
static void
cc_color_panel_button_virtual_add_cb (GtkWidget *widget, CcColorPanel *panel)
{
	GcmDeviceKind device_kind;
	GcmDevice *device;
	const gchar *model;
	const gchar *manufacturer;
	gboolean ret;
	GError *error = NULL;

	/* get device details */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "combobox_virtual_type"));
	device_kind = gtk_combo_box_get_active (GTK_COMBO_BOX(widget)) + 2;
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "entry_virtual_model"));
	model = gtk_entry_get_text (GTK_ENTRY (widget));
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "entry_virtual_manufacturer"));
	manufacturer = gtk_entry_get_text (GTK_ENTRY (widget));

	/* create device */
	device = gcm_device_virtual_new	();
	ret = gcm_device_virtual_create_from_params (GCM_DEVICE_VIRTUAL (device),
						     device_kind, model, manufacturer,
						     NULL, GCM_COLORSPACE_RGB);
	if (!ret) {
		/* TRANSLATORS: could not add virtual device */
		cc_color_panel_error_dialog (panel, _("Failed to create virtual device"), NULL);
		goto out;
	}

	/* save what we've got */
	ret = gcm_device_save (device, &error);
	if (!ret) {
		/* TRANSLATORS: could not add virtual device */
		cc_color_panel_error_dialog (panel, _("Failed to save virtual device"), error->message);
		g_error_free (error);
		goto out;
	}

	/* add to the device list */
	ret = gcm_client_add_device (panel->priv->gcm_client, device, &error);
	if (!ret) {
		/* TRANSLATORS: could not add virtual device */
		cc_color_panel_error_dialog (panel, _("Failed to add virtual device"), error->message);
		g_error_free (error);
		goto out;
	}

out:
	/* we're done */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "dialog_virtual"));
	gtk_widget_hide (widget);
}

/**
 * cc_color_panel_button_virtual_cancel_cb:
 **/
static void
cc_color_panel_button_virtual_cancel_cb (GtkWidget *widget, CcColorPanel *panel)
{
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "dialog_virtual"));
	gtk_widget_hide (widget);
}

/**
 * cc_color_panel_virtual_delete_event_cb:
 **/
static gboolean
cc_color_panel_virtual_delete_event_cb (GtkWidget *widget, GdkEvent *event, CcColorPanel *panel)
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
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "dialog_assign"));
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
	GcmProfile *profile;
	gboolean is_default = FALSE;
	gboolean ret;

	/* hide window */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "dialog_assign"));
	gtk_widget_hide (widget);

	/* get entry */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "combobox_profile"));
	ret = gtk_combo_box_get_active_iter (GTK_COMBO_BOX(widget), &iter);
	if (!ret)
		return;
	model = gtk_combo_box_get_model (GTK_COMBO_BOX(widget));
	gtk_tree_model_get (model, &iter,
			    GCM_PREFS_COMBO_COLUMN_PROFILE, &profile,
			    -1);

	/* if list is empty, we want this to be the default item */
	model = GTK_TREE_MODEL (panel->priv->list_store_assign);
	is_default = !gtk_tree_model_get_iter_first (model, &iter);

	/* add profile */
	gtk_list_store_append (panel->priv->list_store_assign, &iter);
	gtk_list_store_set (panel->priv->list_store_assign, &iter,
			    GCM_ASSIGN_COLUMN_PROFILE, profile,
			    GCM_ASSIGN_COLUMN_SORT, is_default ? "0" : "1",
			    GCM_ASSIGN_COLUMN_IS_DEFAULT, is_default,
			    -1);

	/* save device */
	cc_color_panel_assign_save_profiles_for_device (panel, panel->priv->current_device);
}

/**
 * cc_color_panel_assign_delete_event_cb:
 **/
static gboolean
cc_color_panel_assign_delete_event_cb (GtkWidget *widget, GdkEvent *event, CcColorPanel *panel)
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
	gboolean ret;
	GError *error = NULL;

	/* try to delete device */
	ret = gcm_client_delete_device (panel->priv->gcm_client, panel->priv->current_device, &error);
	if (!ret) {
		/* TRANSLATORS: could not read file */
		cc_color_panel_error_dialog (panel, _("Failed to delete file"), error->message);
		g_error_free (error);
	}
}


/**
 * cc_color_panel_save_and_apply_current_device_cb:
 **/
static gboolean
cc_color_panel_save_and_apply_current_device_cb (CcColorPanel *panel)
{
	gfloat localgamma;
	gfloat brightness;
	gfloat contrast;
	GtkWidget *widget;
	gboolean ret;
	GError *error = NULL;

	/* get values */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "hscale_gamma"));
	localgamma = gtk_range_get_value (GTK_RANGE (widget));
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "hscale_brightness"));
	brightness = gtk_range_get_value (GTK_RANGE (widget));
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "hscale_contrast"));
	contrast = gtk_range_get_value (GTK_RANGE (widget));

	gcm_device_set_gamma (panel->priv->current_device, localgamma);
	gcm_device_set_brightness (panel->priv->current_device, brightness * 100.0f);
	gcm_device_set_contrast (panel->priv->current_device, contrast * 100.0f);

	/* save new profile */
	ret = gcm_device_save (panel->priv->current_device, &error);
	if (!ret) {
		egg_warning ("failed to save config: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* actually set the new profile */
	ret = gcm_device_apply (panel->priv->current_device, &error);
	if (!ret) {
		egg_warning ("failed to apply profile: %s", error->message);
		g_error_free (error);
		goto out;
	}
out:
	panel->priv->save_and_apply_id = 0;
	return FALSE;
}

/**
 * cc_color_panel_reset_cb:
 **/
static void
cc_color_panel_reset_cb (GtkWidget *widget, CcColorPanel *panel)
{
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "hscale_gamma"));
	gtk_range_set_value (GTK_RANGE (widget), 1.0f);
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "hscale_brightness"));
	gtk_range_set_value (GTK_RANGE (widget), 0.0f);
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "hscale_contrast"));
	gtk_range_set_value (GTK_RANGE (widget), 1.0f);

	/* if we've already queued a save and apply, ignore this */
	if (panel->priv->save_and_apply_id != 0) {
		egg_debug ("ignoring extra save and apply, as one is already pending");
		return;
	}
	panel->priv->save_and_apply_id =
		g_timeout_add (50, (GSourceFunc) cc_color_panel_save_and_apply_current_device_cb, panel);
}

/**
 * cc_color_panel_add_devices_columns:
 **/
static void
cc_color_panel_add_devices_columns (CcColorPanel *panel, GtkTreeView *treeview)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	/* image */
	renderer = gtk_cell_renderer_pixbuf_new ();
	g_object_set (renderer, "stock-size", GTK_ICON_SIZE_DND, NULL);
	column = gtk_tree_view_column_new_with_attributes ("", renderer,
							   "icon-name", GCM_DEVICES_COLUMN_ICON, NULL);
	gtk_tree_view_append_column (treeview, column);

	/* column for text */
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer,
		      "wrap-mode", PANGO_WRAP_WORD,
		      NULL);
	column = gtk_tree_view_column_new_with_attributes ("", renderer,
							   "markup", GCM_DEVICES_COLUMN_TITLE, NULL);
	gtk_tree_view_column_set_sort_column_id (column, GCM_DEVICES_COLUMN_SORT);
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (panel->priv->list_store_devices), GCM_DEVICES_COLUMN_SORT, GTK_SORT_ASCENDING);
	gtk_tree_view_append_column (treeview, column);
	gtk_tree_view_column_set_expand (column, TRUE);
}

/**
 * cc_color_panel_add_assign_columns:
 **/
static void
cc_color_panel_add_assign_columns (CcColorPanel *panel, GtkTreeView *treeview)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	/* column for text */
	renderer = gcm_cell_renderer_profile_new ();
	g_object_set (renderer,
		      "wrap-mode", PANGO_WRAP_WORD,
		      NULL);
	column = gtk_tree_view_column_new_with_attributes ("", renderer,
							   "profile", GCM_ASSIGN_COLUMN_PROFILE,
							   "is-default", GCM_ASSIGN_COLUMN_IS_DEFAULT,
							   NULL);
	gtk_tree_view_column_set_sort_column_id (column, GCM_ASSIGN_COLUMN_SORT);
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (panel->priv->list_store_assign), GCM_ASSIGN_COLUMN_SORT, GTK_SORT_ASCENDING);
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
	GcmDeviceKind kind;
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
	kind = gcm_device_get_kind (panel->priv->current_device);
	if (kind == GCM_DEVICE_KIND_DISPLAY) {

		/* are we disconnected */
		ret = gcm_device_get_connected (panel->priv->current_device);
		if (!ret) {
			/* TRANSLATORS: this is when the button is insensitive */
			tooltip = _("Cannot create profile: The display device is not connected");
			goto out;
		}

		/* are we not XRandR 1.3 compat */
		ret = gcm_device_xrandr_get_xrandr13 (GCM_DEVICE_XRANDR (panel->priv->current_device));
		if (!ret) {
			/* TRANSLATORS: this is when the button is insensitive */
			tooltip = _("Cannot create profile: The display driver does not support XRandR 1.3");
			goto out;
		}

		/* find whether we have hardware installed */
		ret = gcm_sensor_client_get_present (panel->priv->sensor_client);
		if (!ret) {
			/* TRANSLATORS: this is when the button is insensitive */
			tooltip = _("Cannot create profile: The measuring instrument is not plugged in");
			goto out;
		}
	} else if (kind == GCM_DEVICE_KIND_SCANNER ||
		   kind == GCM_DEVICE_KIND_CAMERA) {

		/* TODO: find out if we can scan using gnome-scan */
		ret = TRUE;

	} else if (kind == GCM_DEVICE_KIND_PRINTER) {

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
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "button_calibrate"));
	gtk_widget_set_tooltip_text (widget, tooltip);
	gtk_widget_set_sensitive (widget, ret);
}

/**
 * cc_color_panel_devices_treeview_clicked_cb:
 **/
static void
cc_color_panel_devices_treeview_clicked_cb (GtkTreeSelection *selection, CcColorPanel *panel)
{
	guint i;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreePath *path;
	GtkWidget *widget;
	gfloat localgamma;
	gfloat brightness;
	gfloat contrast;
	gboolean connected;
	gchar *id = NULL;
	gboolean ret;
	GcmDeviceKind kind;
	GPtrArray *profiles = NULL;
	GcmProfile *profile;

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
	if (panel->priv->current_device != NULL) {
		g_object_unref (panel->priv->current_device);
		panel->priv->current_device = NULL;
	}
	panel->priv->current_device = gcm_client_get_device_by_id (panel->priv->gcm_client, id);
	if (panel->priv->current_device == NULL)
		goto out;

	/* not a xrandr device */
	kind = gcm_device_get_kind (panel->priv->current_device);
	if (kind != GCM_DEVICE_KIND_DISPLAY) {
		widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "expander_fine_tuning"));
		gtk_widget_set_sensitive (widget, FALSE);
		widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "button_reset"));
		gtk_widget_set_sensitive (widget, FALSE);
	} else {
		/* show more UI */
		widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "expander_fine_tuning"));
		gtk_widget_set_sensitive (widget, TRUE);
		widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "button_reset"));
		gtk_widget_set_sensitive (widget, TRUE);
	}

	/* show broken devices */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "hbox_problems"));
	gtk_widget_hide (widget);
	if (kind == GCM_DEVICE_KIND_DISPLAY) {
		ret = gcm_device_get_connected (panel->priv->current_device);
		if (ret) {
			ret = gcm_device_xrandr_get_xrandr13 (GCM_DEVICE_XRANDR (panel->priv->current_device));
			if (!ret) {
                                widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "label_problems"));
				/* TRANSLATORS: Some shitty binary drivers do not support per-head gamma controls.
				* Whilst this does not matter if you only have one monitor attached, it means you
				* can't color correct additional monitors or projectors. */
				gtk_label_set_label (GTK_LABEL (widget),
						     _("Per-device settings not supported. Check your display driver."));
                                widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "hbox_problems"));
				gtk_widget_show (widget);
			}
		}
	}

	/* set adjustments */
	panel->priv->setting_up_device = TRUE;
	localgamma = gcm_device_get_gamma (panel->priv->current_device);
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "hscale_gamma"));
	gtk_range_set_value (GTK_RANGE (widget), localgamma);
	brightness = gcm_device_get_brightness (panel->priv->current_device);
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "hscale_brightness"));
	gtk_range_set_value (GTK_RANGE (widget), brightness);
	contrast = gcm_device_get_contrast (panel->priv->current_device);
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "hscale_contrast"));
	gtk_range_set_value (GTK_RANGE (widget), contrast);
	panel->priv->setting_up_device = FALSE;

	/* clear existing list */
	gtk_list_store_clear (panel->priv->list_store_assign);

	/* add profiles for the device */
	profiles = gcm_device_get_profiles (panel->priv->current_device);
	for (i=0; i<profiles->len; i++) {
		profile = g_ptr_array_index (profiles, i);
		gtk_list_store_append (panel->priv->list_store_assign, &iter);
		gtk_list_store_set (panel->priv->list_store_assign, &iter,
				    GCM_ASSIGN_COLUMN_PROFILE, profile,
				    GCM_ASSIGN_COLUMN_SORT, (i == 0) ? "0" : "1",
				    GCM_ASSIGN_COLUMN_IS_DEFAULT, (i == 0),
				    -1);
	}

	/* select the default profile to display */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "treeview_assign"));
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
	path = gtk_tree_path_new_from_string ("0");
	gtk_tree_selection_select_path (selection, path);
	/* nothing selected */
	if (!gtk_tree_selection_path_is_selected (selection, path)) {
		gtk_widget_hide (panel->priv->info_bar_vcgt);
	}
	gtk_tree_path_free (path);

	/* make sure selectable */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "combobox_profile"));
	gtk_widget_set_sensitive (widget, TRUE);
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "button_reset"));
	gtk_widget_set_sensitive (widget, TRUE);

	/* can we delete this device? */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "button_delete"));
	connected = gcm_device_get_connected (panel->priv->current_device);
	gtk_widget_set_sensitive (widget, !connected);

	/* can this device calibrate */
	cc_color_panel_set_calibrate_button_sensitivity (panel);
out:
	if (profiles != NULL)
		g_ptr_array_unref (profiles);
	g_free (id);
}

/**
 * cc_color_panel_assign_treeview_row_activated_cb:
 **/
static void
cc_color_panel_assign_treeview_row_activated_cb (GtkTreeView *tree_view, GtkTreePath *path,
					    GtkTreeViewColumn *column, CcColorPanel *panel)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean ret;

	/* get the iter */
	model = GTK_TREE_MODEL (panel->priv->list_store_assign);
	ret = gtk_tree_model_get_iter (model, &iter, path);
	if (!ret)
		return;

	/* make this profile the default */
	cc_color_panel_assign_make_default_internal (panel, model, &iter);
}

/**
 * cc_color_panel_assign_treeview_clicked_cb:
 **/
static void
cc_color_panel_assign_treeview_clicked_cb (GtkTreeSelection *selection, CcColorPanel *panel)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean is_default;
	GtkWidget *widget;
	GcmProfile *profile;

	/* This will only work in single or browse selection mode! */
	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {

		widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "button_assign_make_default"));
		gtk_widget_set_sensitive (widget, FALSE);
		widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "button_assign_remove"));
		gtk_widget_set_sensitive (widget, FALSE);

		egg_debug ("no row selected");
		return;
	}

	/* get profile */
	gtk_tree_model_get (model, &iter,
			    GCM_ASSIGN_COLUMN_PROFILE, &profile,
			    GCM_ASSIGN_COLUMN_IS_DEFAULT, &is_default,
			    -1);
	egg_debug ("selected profile = %s", gcm_profile_get_filename (profile));

	/* is the element the first in the list */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "button_assign_make_default"));
	gtk_widget_set_sensitive (widget, !is_default);

	/* we can remove it now */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "button_assign_remove"));
	gtk_widget_set_sensitive (widget, TRUE);

	/* show a warning if the profile is crap */
	if (gcm_device_get_kind (panel->priv->current_device) == GCM_DEVICE_KIND_DISPLAY &&
	    !gcm_profile_get_has_vcgt (profile)) {
		gtk_widget_show (panel->priv->info_bar_vcgt);
	} else {
		gtk_widget_hide (panel->priv->info_bar_vcgt);
	}
}

/**
 * gcm_device_kind_to_string:
 **/
static const gchar *
cc_color_panel_device_kind_to_string (GcmDeviceKind kind)
{
	if (kind == GCM_DEVICE_KIND_DISPLAY)
		return "1";
	if (kind == GCM_DEVICE_KIND_SCANNER)
		return "2";
	if (kind == GCM_DEVICE_KIND_CAMERA)
		return "3";
	if (kind == GCM_DEVICE_KIND_PRINTER)
		return "4";
	return "5";
}

/**
 * cc_color_panel_add_device_xrandr:
 **/
static void
cc_color_panel_add_device_xrandr (CcColorPanel *panel, GcmDevice *device)
{
	GtkTreeIter iter;
	const gchar *title_tmp;
	gchar *title = NULL;
	gchar *sort = NULL;
	const gchar *id;
	gboolean ret;
	gboolean connected;
	GError *error = NULL;

	/* sanity check */
	if (!GCM_IS_DEVICE_XRANDR (device)) {
		egg_warning ("not a xrandr device");
		goto out;
	}

	/* italic for non-connected devices */
	connected = gcm_device_get_connected (device);
	title_tmp = gcm_device_get_title (device);
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
		title = g_strdup_printf ("%s\n<i>[%s]</i>", title_tmp, _("disconnected"));
	}

	/* create sort order */
	sort = g_strdup_printf ("%s%s",
				cc_color_panel_device_kind_to_string (GCM_DEVICE_KIND_DISPLAY),
				title);

	/* add to list */
	id = gcm_device_get_id (device);
	egg_debug ("add %s to device list", id);
	gtk_list_store_append (panel->priv->list_store_devices, &iter);
	gtk_list_store_set (panel->priv->list_store_devices, &iter,
			    GCM_DEVICES_COLUMN_ID, id,
			    GCM_DEVICES_COLUMN_SORT, sort,
			    GCM_DEVICES_COLUMN_TITLE, title,
			    GCM_DEVICES_COLUMN_ICON, "video-display", -1);
out:
	g_free (sort);
	g_free (title);
}

/**
 * cc_color_panel_set_combo_simple_text:
 **/
static void
cc_color_panel_set_combo_simple_text (GtkWidget *combo_box)
{
	GtkCellRenderer *renderer;
	GtkListStore *store;

	store = gtk_list_store_new (4, G_TYPE_STRING, GCM_TYPE_PROFILE, G_TYPE_UINT, G_TYPE_STRING);
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store), GCM_PREFS_COMBO_COLUMN_SORTABLE, GTK_SORT_ASCENDING);
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
 * cc_color_panel_profile_combo_changed_cb:
 **/
static void
cc_color_panel_profile_combo_changed_cb (GtkWidget *widget, CcColorPanel *panel)
{
	GFile *file = NULL;
	GFile *dest = NULL;
	gboolean ret;
	GError *error = NULL;
	GcmProfile *profile = NULL;
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
			egg_warning ("failed to get ICC file");
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
			egg_debug ("%s did not import correctly", uri);
			g_free (uri);
			goto out;
		}

		/* get an object of the destination */
		dest = gcm_utils_get_profile_destination (file);
		profile = gcm_profile_new ();
		ret = gcm_profile_parse (profile, dest, &error);
		if (!ret) {
			/* set to first entry */
			gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
			egg_warning ("failed to parse ICC file: %s", error->message);
			g_error_free (error);
			goto out;
		}

		/* check the file is suitable */
		ret = cc_color_panel_is_profile_suitable_for_device (profile, panel->priv->current_device);
		if (!ret) {
			/* set to 'None' */
			gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);

			/* TRANSLATORS: the profile was of the wrong sort for this device */
			cc_color_panel_error_dialog (panel, _("Could not import profile"),
						_("The profile was of the wrong type for this device"));
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
	if (dest != NULL)
		g_object_unref (dest);
	if (profile != NULL)
		g_object_unref (profile);
}

/**
 * cc_color_panel_slider_changed_cb:
 **/
static void
cc_color_panel_slider_changed_cb (GtkRange *range, CcColorPanel *panel)
{
	/* we're just setting up the device, not moving the slider */
	if (panel->priv->setting_up_device) {
		egg_debug ("setting up device, so ignore");
		return;
	}

	/* if we've already queued a save and apply, ignore this */
	if (panel->priv->save_and_apply_id != 0) {
		egg_debug ("ignoring extra save and apply, as one is already pending");
		return;
	}

	/* schedule a save and apply */
	panel->priv->save_and_apply_id =
		g_timeout_add (50, (GSourceFunc) cc_color_panel_save_and_apply_current_device_cb, panel);
}

/**
 * cc_color_panel_sensor_client_changed_cb:
 **/
static void
cc_color_panel_sensor_client_changed_cb (GcmSensorClient *sensor_client, CcColorPanel *panel)
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
cc_color_panel_device_kind_to_icon_name (GcmDeviceKind kind)
{
	if (kind == GCM_DEVICE_KIND_DISPLAY)
		return "video-display";
	if (kind == GCM_DEVICE_KIND_SCANNER)
		return "scanner";
	if (kind == GCM_DEVICE_KIND_PRINTER)
		return "printer";
	if (kind == GCM_DEVICE_KIND_CAMERA)
		return "camera-photo";
	return "image-missing";
}

/**
 * cc_color_panel_add_device_kind:
 **/
static void
cc_color_panel_add_device_kind (CcColorPanel *panel, GcmDevice *device)
{
	GtkTreeIter iter;
	const gchar *title;
	GString *string;
	const gchar *id;
	gchar *sort = NULL;
	GcmDeviceKind kind;
	const gchar *icon_name;
	gboolean connected;
	gboolean virtual;

	/* get icon */
	kind = gcm_device_get_kind (device);
	icon_name = cc_color_panel_device_kind_to_icon_name (kind);

	/* create a title for the device */
	title = gcm_device_get_title (device);
	string = g_string_new (title);

	/* italic for non-connected devices */
	connected = gcm_device_get_connected (device);
	virtual = gcm_device_get_virtual (device);
	if (!connected && !virtual) {
		/* TRANSLATORS: this is where the device has been setup but is not connected */
		g_string_append_printf (string, "\n<i>[%s]</i>", _("disconnected"));
	}

	/* create sort order */
	sort = g_strdup_printf ("%s%s",
				cc_color_panel_device_kind_to_string (kind),
				string->str);

	/* add to list */
	id = gcm_device_get_id (device);
	gtk_list_store_append (panel->priv->list_store_devices, &iter);
	gtk_list_store_set (panel->priv->list_store_devices, &iter,
			    GCM_DEVICES_COLUMN_ID, id,
			    GCM_DEVICES_COLUMN_SORT, sort,
			    GCM_DEVICES_COLUMN_TITLE, string->str,
			    GCM_DEVICES_COLUMN_ICON, icon_name, -1);
	g_free (sort);
	g_string_free (string, TRUE);
}

/**
 * cc_color_panel_remove_device:
 **/
static void
cc_color_panel_remove_device (CcColorPanel *panel, GcmDevice *gcm_device)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	const gchar *id;
	gchar *id_tmp;
	gboolean ret;

	/* remove */
	id = gcm_device_get_id (gcm_device);
	egg_debug ("removing: %s (connected: %i)", id,
		   gcm_device_get_connected (gcm_device));

	/* get first element */
	model = GTK_TREE_MODEL (panel->priv->list_store_devices);
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
 * cc_color_panel_added_cb:
 **/
static void
cc_color_panel_added_cb (GcmClient *client, GcmDevice *device, CcColorPanel *panel)
{
	GcmDeviceKind kind;
	egg_debug ("added: %s (connected: %i, saved: %i)",
		   gcm_device_get_id (device),
		   gcm_device_get_connected (device),
		   gcm_device_get_saved (device));

	/* remove the saved device if it's already there */
	cc_color_panel_remove_device (panel, device);

	/* add the device */
	kind = gcm_device_get_kind (device);
	if (kind == GCM_DEVICE_KIND_DISPLAY)
		cc_color_panel_add_device_xrandr (panel, device);
	else
		cc_color_panel_add_device_kind (panel, device);
}

/**
 * cc_color_panel_changed_cb:
 **/
static void
cc_color_panel_changed_cb (GcmClient *client, GcmDevice *device, CcColorPanel *panel)
{
	GcmDeviceKind kind;

	/* no not re-add to the ui if we just deleted this */
	if (!gcm_device_get_connected (device) &&
	    !gcm_device_get_saved (device)) {
		egg_warning ("ignoring uninteresting device: %s", gcm_device_get_id (device));
		return;
	}

	egg_debug ("changed: %s", gcm_device_get_id (device));

	/* remove the saved device if it's already there */
	cc_color_panel_remove_device (panel, device);

	/* add the device */
	kind = gcm_device_get_kind (device);
	if (kind == GCM_DEVICE_KIND_DISPLAY)
		cc_color_panel_add_device_xrandr (panel, device);
	else
		cc_color_panel_add_device_kind (panel, device);
}

/**
 * cc_color_panel_removed_cb:
 **/
static void
cc_color_panel_removed_cb (GcmClient *client, GcmDevice *device, CcColorPanel *panel)
{
	gboolean connected;
	GtkTreeIter iter;
	GtkTreeSelection *selection;
	GtkWidget *widget;
	gboolean ret;

	/* remove from the UI */
	cc_color_panel_remove_device (panel, device);

	/* ensure this device is re-added if it's been saved */
	connected = gcm_device_get_connected (device);
	if (connected)
		gcm_client_coldplug (panel->priv->gcm_client, GCM_CLIENT_COLDPLUG_SAVED, NULL);

	/* select the first device */
	ret = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (panel->priv->list_store_devices), &iter);
	if (!ret)
		return;

	/* click it */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "treeview_devices"));
	gtk_tree_view_set_model (GTK_TREE_VIEW (widget), GTK_TREE_MODEL (panel->priv->list_store_devices));
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
	gtk_tree_selection_select_iter (selection, &iter);
}

/**
 * cc_color_panel_setup_space_combobox:
 **/
static void
cc_color_panel_setup_space_combobox (CcColorPanel *panel, GtkWidget *widget, GcmColorspace colorspace, const gchar *profile_filename)
{
	GcmProfile *profile;
	guint i;
	const gchar *filename;
	GcmColorspace colorspace_tmp;
	gboolean has_profile = FALSE;
	gboolean has_vcgt;
	gboolean has_colorspace_description;
	gchar *text = NULL;
	GPtrArray *profile_array = NULL;
	GtkTreeIter iter;

	/* get new list */
	profile_array = gcm_profile_store_get_array (panel->priv->profile_store);

	/* update each list */
	for (i=0; i<profile_array->len; i++) {
		profile = g_ptr_array_index (profile_array, i);

		/* only for correct kind */
		has_vcgt = gcm_profile_get_has_vcgt (profile);
		has_colorspace_description = gcm_profile_has_colorspace_description (profile);
		colorspace_tmp = gcm_profile_get_colorspace (profile);
		if (!has_vcgt &&
		    colorspace == colorspace_tmp &&
		    (colorspace != GCM_COLORSPACE_RGB ||
		     has_colorspace_description)) {
			cc_color_panel_combobox_add_profile (widget, profile, GCM_PREFS_ENTRY_TYPE_PROFILE, &iter);

			/* set active option */
			filename = gcm_profile_get_filename (profile);
			if (g_strcmp0 (filename, profile_filename) == 0)
				gtk_combo_box_set_active_iter (GTK_COMBO_BOX (widget), &iter);
			has_profile = TRUE;
		}
	}
	if (!has_profile) {
		/* TRANSLATORS: this is when there are no profiles that can be used; the search term is either "RGB" or "CMYK" */
		text = g_strdup_printf (_("No %s color spaces available"),
					gcm_colorspace_to_localised_string (colorspace));
		gtk_combo_box_append_text (GTK_COMBO_BOX(widget), text);
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
		gtk_widget_set_sensitive (widget, FALSE);
	}
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
	GcmProfile *profile = NULL;
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

	filename = gcm_profile_get_filename (profile);
	egg_debug ("changed working space %s", filename);
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
	egg_debug ("changed rendering intent to %s", gcm_intent_to_string (active+1));
	g_settings_set_enum (panel->priv->settings, key, active+1);
}

/**
 * cc_color_panel_setup_rendering_combobox:
 **/
static void
cc_color_panel_setup_rendering_combobox (GtkWidget *widget, GcmIntent intent)
{
	guint i;
	gboolean ret = FALSE;
	gchar *label;

	for (i=1; i<GCM_INTENT_LAST; i++) {
		label = g_strdup_printf ("%s - %s",
					 gcm_intent_to_localized_text (i),
					 gcm_intent_to_localized_description (i));
		gtk_combo_box_append_text (GTK_COMBO_BOX (widget), label);
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
 * cc_color_panel_startup_idle_cb:
 **/
static gboolean
cc_color_panel_startup_idle_cb (CcColorPanel *panel)
{
	GtkWidget *widget;
	gboolean ret;
	GError *error = NULL;
	gchar *colorspace_rgb;
	gchar *colorspace_cmyk;
	gchar *colorspace_gray;
	gint intent_display = -1;
	gint intent_softproof = -1;
	GcmProfileSearchFlags search_flags = GCM_PROFILE_STORE_SEARCH_ALL;

	/* volume checking is optional */
	ret = g_settings_get_boolean (panel->priv->settings, GCM_SETTINGS_USE_PROFILES_FROM_VOLUMES);
	if (!ret)
		search_flags &= ~GCM_PROFILE_STORE_SEARCH_VOLUMES;

	/* search the disk for profiles */
	gcm_profile_store_search (panel->priv->profile_store, search_flags);
	g_signal_connect (panel->priv->profile_store, "changed", G_CALLBACK(cc_color_panel_profile_store_changed_cb), panel);

	/* setup RGB combobox */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "combobox_space_rgb"));
	colorspace_rgb = g_settings_get_string (panel->priv->settings, GCM_SETTINGS_COLORSPACE_RGB);
	cc_color_panel_set_combo_simple_text (widget);
	cc_color_panel_setup_space_combobox (panel, widget, GCM_COLORSPACE_RGB, colorspace_rgb);
	g_object_set_data (G_OBJECT(widget), "GCM:GSettingsKey", (gpointer) GCM_SETTINGS_COLORSPACE_RGB);
	g_signal_connect (G_OBJECT (widget), "changed",
			  G_CALLBACK (cc_color_panel_space_combo_changed_cb), panel);

	/* setup CMYK combobox */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "combobox_space_cmyk"));
	colorspace_cmyk = g_settings_get_string (panel->priv->settings, GCM_SETTINGS_COLORSPACE_CMYK);
	cc_color_panel_set_combo_simple_text (widget);
	cc_color_panel_setup_space_combobox (panel, widget, GCM_COLORSPACE_CMYK, colorspace_cmyk);
	g_object_set_data (G_OBJECT(widget), "GCM:GSettingsKey", (gpointer) GCM_SETTINGS_COLORSPACE_CMYK);
	g_signal_connect (G_OBJECT (widget), "changed",
			  G_CALLBACK (cc_color_panel_space_combo_changed_cb), panel);

	/* setup gray combobox */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "combobox_space_gray"));
	colorspace_gray = g_settings_get_string (panel->priv->settings, GCM_SETTINGS_COLORSPACE_GRAY);
	cc_color_panel_set_combo_simple_text (widget);
	cc_color_panel_setup_space_combobox (panel, widget, GCM_COLORSPACE_GRAY, colorspace_gray);
	g_object_set_data (G_OBJECT(widget), "GCM:GSettingsKey", (gpointer) GCM_SETTINGS_COLORSPACE_GRAY);
	g_signal_connect (G_OBJECT (widget), "changed",
			  G_CALLBACK (cc_color_panel_space_combo_changed_cb), panel);

	/* setup rendering lists */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "combobox_rendering_display"));
	cc_color_panel_set_combo_simple_text (widget);
	intent_display = g_settings_get_enum (panel->priv->settings, GCM_SETTINGS_RENDERING_INTENT_DISPLAY);
	cc_color_panel_setup_rendering_combobox (widget, intent_display);
	g_object_set_data (G_OBJECT(widget), "GCM:GSettingsKey", (gpointer) GCM_SETTINGS_RENDERING_INTENT_DISPLAY);
	g_signal_connect (G_OBJECT (widget), "changed",
			  G_CALLBACK (cc_color_panel_renderer_combo_changed_cb), panel);

	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "combobox_rendering_softproof"));
	cc_color_panel_set_combo_simple_text (widget);
	intent_softproof = g_settings_get_enum (panel->priv->settings, GCM_SETTINGS_RENDERING_INTENT_SOFTPROOF);
	cc_color_panel_setup_rendering_combobox (widget, intent_softproof);
	g_object_set_data (G_OBJECT(widget), "GCM:GSettingsKey", (gpointer) GCM_SETTINGS_RENDERING_INTENT_SOFTPROOF);
	g_signal_connect (G_OBJECT (widget), "changed",
			  G_CALLBACK (cc_color_panel_renderer_combo_changed_cb), panel);

	/* coldplug plugged in devices */
	ret = gcm_client_coldplug (panel->priv->gcm_client, GCM_CLIENT_COLDPLUG_ALL, &error);
	if (!ret) {
		egg_warning ("failed to add connected devices: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* set calibrate button sensitivity */
	cc_color_panel_set_calibrate_button_sensitivity (panel);

	/* we're probably showing now */
	panel->priv->main_window = gtk_widget_get_toplevel (panel->priv->info_bar_loading);

	/* do we show the shared-color-profiles-extra installer? */
	egg_debug ("getting installed");
	ret = gcm_utils_is_package_installed (GCM_PREFS_PACKAGE_NAME_COLOR_PROFILES_EXTRA);
	gtk_widget_set_visible (panel->priv->info_bar_profiles, !ret);
out:
	g_free (colorspace_rgb);
	g_free (colorspace_cmyk);
	return FALSE;
}

/**
 * cc_color_panel_apply_all_devices_idle_cb:
 **/
static gboolean
cc_color_panel_apply_all_devices_idle_cb (CcColorPanel *panel)
{
	GPtrArray *array = NULL;
	GcmDevice *device;
	GError *error = NULL;
	gboolean ret;
	guint i;

	/* set for each output */
	array = gcm_client_get_devices (panel->priv->gcm_client);
	for (i=0; i<array->len; i++) {
		device = g_ptr_array_index (array, i);

		/* we don't care */
		ret = gcm_device_get_connected (device);
		if (!ret)
			continue;

		/* set gamma for device */
		ret = gcm_device_apply (device, &error);
		if (!ret) {
			egg_warning ("failed to set profile: %s", error->message);
			g_error_free (error);
			break;
		}
	}
	g_ptr_array_unref (array);
	panel->priv->apply_all_devices_id = 0;
	return FALSE;
}

/**
 * cc_color_panel_checkbutton_changed_cb:
 **/
static void
cc_color_panel_checkbutton_changed_cb (GtkWidget *widget, CcColorPanel *panel)
{
	/* set the new setting */
	panel->priv->apply_all_devices_id =
		g_idle_add ((GSourceFunc) cc_color_panel_apply_all_devices_idle_cb, panel);
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

	gtk_drag_dest_set (widget, GTK_DEST_DEFAULT_ALL, &entry, 1, GDK_ACTION_MOVE | GDK_ACTION_COPY);
	g_free (entry.target);
}

/**
 * cc_color_panel_profile_store_changed_cb:
 **/
static void
cc_color_panel_profile_store_changed_cb (GcmProfileStore *profile_store, CcColorPanel *panel)
{
	GtkTreeSelection *selection;
	GtkWidget *widget;

	/* re-get all the profiles for this device */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "treeview_devices"));
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
	if (selection == NULL)
		return;
	g_signal_emit_by_name (selection, "changed", panel);
}

/**
 * cc_color_panel_select_first_device_idle_cb:
 **/
static gboolean
cc_color_panel_select_first_device_idle_cb (CcColorPanel *panel)
{
	GtkTreePath *path;
	GtkWidget *widget;

	/* set the cursor on the first device */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "treeview_devices"));
	path = gtk_tree_path_new_from_string ("0");
	gtk_tree_view_set_cursor (GTK_TREE_VIEW (widget), path, NULL, FALSE);
	gtk_tree_path_free (path);

	return FALSE;
}

/**
 * cc_color_panel_client_notify_loading_cb:
 **/
static void
cc_color_panel_client_notify_loading_cb (GcmClient *client, GParamSpec *pspec, CcColorPanel *panel)
{
	gboolean loading;

	/*if loading show the bar */
	loading = gcm_client_get_loading (client);
	if (loading) {
		gtk_widget_show (panel->priv->info_bar_loading);
		return;
	}

	/* otherwise clear the loading widget */
	gtk_widget_hide (panel->priv->info_bar_loading);

	/* idle callback */
	g_idle_add ((GSourceFunc) cc_color_panel_select_first_device_idle_cb, panel);
}

/**
 * cc_color_panel_info_bar_response_cb:
 **/
static void
cc_color_panel_info_bar_response_cb (GtkDialog *dialog, GtkResponseType response, CcColorPanel *panel)
{
	GtkWindow *window;
	gboolean ret;

	if (response == GTK_RESPONSE_HELP) {
		/* open the help file in the right place */
		gcm_gnome_help ("faq-missing-vcgt");

	} else if (response == GTK_RESPONSE_APPLY) {
		/* install the extra profiles */
		window = GTK_WINDOW(panel->priv->main_window);
		ret = gcm_utils_install_package (GCM_PREFS_PACKAGE_NAME_COLOR_PROFILES_EXTRA, window);
		if (ret)
			gtk_widget_hide (panel->priv->info_bar_profiles);
	}
}

/**
 * gcm_device_kind_to_localised_string:
 **/
static const gchar *
gcm_device_kind_to_localised_string (GcmDeviceKind device_kind)
{
	if (device_kind == GCM_DEVICE_KIND_DISPLAY) {
		/* TRANSLATORS: device type */
		return _("Display");
	}
	if (device_kind == GCM_DEVICE_KIND_SCANNER) {
		/* TRANSLATORS: device type */
		return _("Scanner");
	}
	if (device_kind == GCM_DEVICE_KIND_PRINTER) {
		/* TRANSLATORS: device type */
		return _("Printer");
	}
	if (device_kind == GCM_DEVICE_KIND_CAMERA) {
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

	for (i=GCM_DEVICE_KIND_SCANNER; i<GCM_DEVICE_KIND_LAST; i++) {
		text = gcm_device_kind_to_localised_string (i);
		gtk_combo_box_append_text (GTK_COMBO_BOX(widget), text);
	}
	gtk_combo_box_set_active (GTK_COMBO_BOX (widget), GCM_DEVICE_KIND_PRINTER - 2);
}

/**
 * gpk_update_viewer_notify_network_state_cb:
 **/
static void
cc_color_panel_button_virtual_entry_changed_cb (GtkEntry *entry, GParamSpec *pspec, CcColorPanel *panel)
{
	const gchar *model;
	const gchar *manufacturer;
	GtkWidget *widget;

	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "entry_virtual_model"));
	model = gtk_entry_get_text (GTK_ENTRY (widget));
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "entry_virtual_manufacturer"));
	manufacturer = gtk_entry_get_text (GTK_ENTRY (widget));

	/* only set the add button sensitive if both sections have text */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "button_virtual_add"));
	gtk_widget_set_sensitive (widget, (model != NULL && model[0] != '\0' && manufacturer != NULL && manufacturer[0] != '\0'));
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

static void
cc_color_panel_finalize (GObject *object)
{
	CcColorPanel *panel = CC_COLOR_PANEL (object);

	if (panel->priv->current_device != NULL)
		g_object_unref (panel->priv->current_device);
	if (panel->priv->sensor_client != NULL)
		g_object_unref (panel->priv->sensor_client);
	if (panel->priv->settings != NULL)
		g_object_unref (panel->priv->settings);
	if (panel->priv->builder != NULL)
		g_object_unref (panel->priv->builder);
	if (panel->priv->profile_store != NULL)
		g_object_unref (panel->priv->profile_store);
	if (panel->priv->gcm_client != NULL)
		g_object_unref (panel->priv->gcm_client);
	if (panel->priv->save_and_apply_id != 0)
		g_source_remove (panel->priv->save_and_apply_id);
	if (panel->priv->apply_all_devices_id != 0)
		g_source_remove (panel->priv->apply_all_devices_id);

	G_OBJECT_CLASS (cc_color_panel_parent_class)->finalize (object);
}

static void
cc_color_panel_init (CcColorPanel *panel)
{
	GtkWidget *widget;
	GtkWidget *main_window;
	GError *error = NULL;
	gint retval;
	GtkTreeSelection *selection;
	GtkWidget *info_bar_loading_label;
	GtkWidget *info_bar_vcgt_label;
	GtkWidget *info_bar_profiles_label;

	panel->priv = CC_COLOR_PREFS_GET_PRIVATE (panel);

	/* setup defaults */
	panel->priv->settings = g_settings_new (GCM_SETTINGS_SCHEMA);

	/* get UI */
	panel->priv->builder = gtk_builder_new ();
	retval = gtk_builder_add_from_file (panel->priv->builder, GCM_DATA "/gcm-prefs.ui", &error);
	if (retval == 0) {
		egg_error ("failed to load ui: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* reparent */
	main_window = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "dialog_prefs"));
	widget = gtk_dialog_get_content_area (GTK_DIALOG (main_window));
	gtk_widget_reparent (widget, GTK_WIDGET (panel));

	/* add application specific icons to search path */
	gtk_icon_theme_append_search_path (gtk_icon_theme_get_default (),
	                                   GCM_DATA G_DIR_SEPARATOR_S "icons");

	/* maintain a list of profiles */
	panel->priv->profile_store = gcm_profile_store_new ();

	/* create list stores */
	panel->priv->list_store_devices = gtk_list_store_new (GCM_DEVICES_COLUMN_LAST, G_TYPE_STRING, G_TYPE_STRING,
						 G_TYPE_STRING, G_TYPE_STRING);
	panel->priv->list_store_assign = gtk_list_store_new (GCM_ASSIGN_COLUMN_LAST, G_TYPE_STRING, GCM_TYPE_PROFILE, G_TYPE_BOOLEAN);

	/* assign buttons */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "button_assign_add"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (cc_color_panel_assign_add_cb), panel);
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "button_assign_remove"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (cc_color_panel_assign_remove_cb), panel);
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "button_assign_make_default"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (cc_color_panel_assign_make_default_cb), panel);

	/* create device tree view */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "treeview_devices"));
	gtk_tree_view_set_model (GTK_TREE_VIEW (widget),
				 GTK_TREE_MODEL (panel->priv->list_store_devices));
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
	g_signal_connect (selection, "changed",
			  G_CALLBACK (cc_color_panel_devices_treeview_clicked_cb), panel);

	/* add columns to the tree view */
	cc_color_panel_add_devices_columns (panel, GTK_TREE_VIEW (widget));
	gtk_tree_view_columns_autosize (GTK_TREE_VIEW (widget));

	/* create assign tree view */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "treeview_assign"));
	gtk_tree_view_set_model (GTK_TREE_VIEW (widget),
				 GTK_TREE_MODEL (panel->priv->list_store_assign));
	g_signal_connect (GTK_TREE_VIEW (widget), "row-activated",
			  G_CALLBACK (cc_color_panel_assign_treeview_row_activated_cb), panel);
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
	g_signal_connect (selection, "changed",
			  G_CALLBACK (cc_color_panel_assign_treeview_clicked_cb), panel);

	/* add columns to the tree view */
	cc_color_panel_add_assign_columns (panel, GTK_TREE_VIEW (widget));
	gtk_tree_view_columns_autosize (GTK_TREE_VIEW (widget));
	gtk_tree_view_set_reorderable (GTK_TREE_VIEW (widget), TRUE);

	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "button_default"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (cc_color_panel_default_cb), panel);
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "button_help"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (cc_color_panel_help_cb), panel);
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "button_viewer"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (cc_color_panel_viewer_cb), panel);
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "button_reset"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (cc_color_panel_reset_cb), panel);
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "button_delete"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (cc_color_panel_delete_cb), panel);
	gtk_widget_set_sensitive (widget, FALSE);
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "button_device_add"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (cc_color_panel_device_add_cb), panel);
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "button_calibrate"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (cc_color_panel_calibrate_cb), panel);
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "expander_fine_tuning"));
	gtk_widget_set_sensitive (widget, FALSE);

	/* set up virtual dialog */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "dialog_virtual"));
	g_signal_connect (widget, "delete-event",
			  G_CALLBACK (cc_color_panel_virtual_delete_event_cb), panel);
	g_signal_connect (widget, "drag-data-received",
			  G_CALLBACK (cc_color_panel_virtual_drag_data_received_cb), panel);
	cc_color_panel_setup_drag_and_drop (widget);

	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "button_virtual_add"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (cc_color_panel_button_virtual_add_cb), panel);
	gtk_widget_set_sensitive (widget, FALSE);

	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "button_virtual_cancel"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (cc_color_panel_button_virtual_cancel_cb), panel);

	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "combobox_virtual_type"));
	cc_color_panel_set_combo_simple_text (widget);
	cc_color_panel_setup_virtual_combobox (widget);

	/* set up assign dialog */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "dialog_assign"));
	g_signal_connect (widget, "delete-event",
			  G_CALLBACK (cc_color_panel_assign_delete_event_cb), panel);
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "button_assign_cancel"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (cc_color_panel_button_assign_cancel_cb), panel);
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "button_assign_ok"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (cc_color_panel_button_assign_ok_cb), panel);

	/* disable the add button if nothing in either box */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "entry_virtual_model"));
	g_signal_connect (widget, "notify::text",
			  G_CALLBACK (cc_color_panel_button_virtual_entry_changed_cb), panel);
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "entry_virtual_manufacturer"));
	g_signal_connect (widget, "notify::text",
			  G_CALLBACK (cc_color_panel_button_virtual_entry_changed_cb), panel);

	/* setup icc profiles list */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "combobox_profile"));
	cc_color_panel_set_combo_simple_text (widget);
	gtk_widget_set_sensitive (widget, FALSE);
	g_signal_connect (G_OBJECT (widget), "changed",
			  G_CALLBACK (cc_color_panel_profile_combo_changed_cb), panel);

	/* set ranges */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "hscale_gamma"));
	gtk_range_set_range (GTK_RANGE (widget), 0.1f, 5.0f);
	gtk_scale_add_mark (GTK_SCALE (widget), 1.0f, GTK_POS_TOP, "");
	gtk_scale_add_mark (GTK_SCALE (widget), 1.8f, GTK_POS_TOP, "");
	gtk_scale_add_mark (GTK_SCALE (widget), 2.2f, GTK_POS_TOP, "");

	/* set ranges */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "hscale_brightness"));
	gtk_range_set_range (GTK_RANGE (widget), 0.0f, 0.9f);
//	gtk_scale_add_mark (GTK_SCALE (widget), 0.0f, GTK_POS_TOP, "");

	/* set ranges */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "hscale_contrast"));
	gtk_range_set_range (GTK_RANGE (widget), 0.1f, 1.0f);
//	gtk_scale_add_mark (GTK_SCALE (widget), 1.0f, GTK_POS_TOP, "");

	/* use a device client array */
	panel->priv->gcm_client = gcm_client_new ();
	gcm_client_set_use_threads (panel->priv->gcm_client, TRUE);
	g_signal_connect (panel->priv->gcm_client, "added", G_CALLBACK (cc_color_panel_added_cb), panel);
	g_signal_connect (panel->priv->gcm_client, "removed", G_CALLBACK (cc_color_panel_removed_cb), panel);
	g_signal_connect (panel->priv->gcm_client, "changed", G_CALLBACK (cc_color_panel_changed_cb), panel);
	g_signal_connect (panel->priv->gcm_client, "notify::loading",
			  G_CALLBACK (cc_color_panel_client_notify_loading_cb), panel);

	/* use the color device */
	panel->priv->sensor_client = gcm_sensor_client_new ();
	g_signal_connect (panel->priv->sensor_client, "changed", G_CALLBACK (cc_color_panel_sensor_client_changed_cb), panel);

	/* use infobar */
	panel->priv->info_bar_loading = gtk_info_bar_new ();
	panel->priv->info_bar_vcgt = gtk_info_bar_new ();
	g_signal_connect (panel->priv->info_bar_vcgt, "response",
			  G_CALLBACK (cc_color_panel_info_bar_response_cb), panel);
	panel->priv->info_bar_profiles = gtk_info_bar_new ();
	g_signal_connect (panel->priv->info_bar_profiles, "response",
			  G_CALLBACK (cc_color_panel_info_bar_response_cb), panel);

	/* TRANSLATORS: button for more details about the vcgt failure */
	gtk_info_bar_add_button (GTK_INFO_BAR(panel->priv->info_bar_vcgt), _("More Information"), GTK_RESPONSE_HELP);

	/* TRANSLATORS: button to install extra profiles */
	gtk_info_bar_add_button (GTK_INFO_BAR(panel->priv->info_bar_profiles), _("Install now"), GTK_RESPONSE_APPLY);

	/* TRANSLATORS: this is displayed while the devices are being probed */
	info_bar_loading_label = gtk_label_new (_("Loading list of devices…"));
	gtk_info_bar_set_message_type (GTK_INFO_BAR(panel->priv->info_bar_loading), GTK_MESSAGE_INFO);
	widget = gtk_info_bar_get_content_area (GTK_INFO_BAR(panel->priv->info_bar_loading));
	gtk_container_add (GTK_CONTAINER(widget), info_bar_loading_label);
	gtk_widget_show (info_bar_loading_label);

	/* TRANSLATORS: this is displayed when the profile is crap */
	info_bar_vcgt_label = gtk_label_new (_("This profile does not have the information required for whole-screen color correction."));
	gtk_label_set_line_wrap (GTK_LABEL (info_bar_vcgt_label), TRUE);
	gtk_info_bar_set_message_type (GTK_INFO_BAR(panel->priv->info_bar_vcgt), GTK_MESSAGE_INFO);
	widget = gtk_info_bar_get_content_area (GTK_INFO_BAR(panel->priv->info_bar_vcgt));
	gtk_container_add (GTK_CONTAINER(widget), info_bar_vcgt_label);
	gtk_widget_show (info_bar_vcgt_label);

	/* TRANSLATORS: this is displayed when the profile is crap */
	info_bar_profiles_label = gtk_label_new (_("More color profiles could be automatically installed."));
	gtk_label_set_line_wrap (GTK_LABEL (info_bar_profiles_label), TRUE);
	gtk_info_bar_set_message_type (GTK_INFO_BAR(panel->priv->info_bar_profiles), GTK_MESSAGE_INFO);
	widget = gtk_info_bar_get_content_area (GTK_INFO_BAR(panel->priv->info_bar_profiles));
	gtk_container_add (GTK_CONTAINER(widget), info_bar_profiles_label);
	gtk_widget_show (info_bar_profiles_label);

	/* add infobar to devices pane */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "vbox_devices"));
	gtk_box_pack_start (GTK_BOX(widget), panel->priv->info_bar_loading, FALSE, FALSE, 0);

	/* add infobar to devices pane */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "vbox_devices"));
	gtk_box_pack_start (GTK_BOX(widget), panel->priv->info_bar_vcgt, FALSE, FALSE, 0);

	/* add infobar to defaults pane */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "vbox_working_spaces"));
	gtk_box_pack_start (GTK_BOX(widget), panel->priv->info_bar_profiles, TRUE, FALSE, 0);

	/* connect up sliders */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "hscale_contrast"));
	g_signal_connect (widget, "value-changed",
			  G_CALLBACK (cc_color_panel_slider_changed_cb), panel);
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "hscale_brightness"));
	g_signal_connect (widget, "value-changed",
			  G_CALLBACK (cc_color_panel_slider_changed_cb), panel);
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "hscale_gamma"));
	g_signal_connect (widget, "value-changed",
			  G_CALLBACK (cc_color_panel_slider_changed_cb), panel);

	/* connect up global widget */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "checkbutton_display"));
	g_settings_bind (panel->priv->settings,
			 GCM_SETTINGS_GLOBAL_DISPLAY_CORRECTION,
			 widget, "active",
			 G_SETTINGS_BIND_DEFAULT);
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (cc_color_panel_checkbutton_changed_cb), panel);

	/* connect up atom widget */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "checkbutton_profile"));
	g_settings_bind (panel->priv->settings,
			 GCM_SETTINGS_SET_ICC_PROFILE_ATOM,
			 widget, "active",
			 G_SETTINGS_BIND_DEFAULT);
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (cc_color_panel_checkbutton_changed_cb), panel);

	/* do we show the fine tuning box */
	widget = GTK_WIDGET (gtk_builder_get_object (panel->priv->builder, "expander_fine_tuning"));
	g_settings_bind (panel->priv->settings,
			 GCM_SETTINGS_SHOW_FINE_TUNING,
			 widget, "visible",
			 G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_NO_SENSITIVITY);

	/* get main window */
	panel->priv->main_window = gtk_widget_get_toplevel (panel->priv->info_bar_loading);

	/* connect up drags */
	g_signal_connect (panel->priv->main_window, "drag-data-received",
			  G_CALLBACK (cc_color_panel_drag_data_received_cb), panel);
if(0)	cc_color_panel_setup_drag_and_drop (GTK_WIDGET (panel->priv->main_window));

	/* do all this after the window has been set up */
	g_idle_add ((GSourceFunc) cc_color_panel_startup_idle_cb, panel);
out:
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

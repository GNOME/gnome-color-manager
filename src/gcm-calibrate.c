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

/**
 * SECTION:gcm-calibrate
 * @short_description: Calibration object
 *
 * This object allows calibration functionality using CMS.
 */

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <tiff.h>
#include <tiffio.h>
#include <gconf/gconf-client.h>

#include "gcm-calibrate.h"
#include "gcm-utils.h"
#include "gcm-brightness.h"
#include "gcm-colorimeter.h"
#include "gcm-calibrate-dialog.h"

#include "egg-debug.h"

static void     gcm_calibrate_finalize	(GObject     *object);

#define GCM_CALIBRATE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GCM_TYPE_CALIBRATE, GcmCalibratePrivate))

/**
 * GcmCalibratePrivate:
 *
 * Private #GcmCalibrate data
 **/
struct _GcmCalibratePrivate
{
	GcmColorimeter			*colorimeter;
	GcmCalibrateReferenceKind	 reference_kind;
	GcmCalibrateDeviceKind		 device_kind;
	GcmCalibratePrintKind		 print_kind;
	GcmCalibratePrecision		 precision;
	GcmColorimeterKind		 colorimeter_kind;
	GcmCalibrateDialog		*calibrate_dialog;
	GcmDeviceTypeEnum		 device_type;
	gchar				*output_name;
	gchar				*filename_source;
	gchar				*filename_reference;
	gchar				*filename_result;
	gchar				*basename;
	gchar				*manufacturer;
	gchar				*model;
	gchar				*description;
	gchar				*serial;
	gchar				*device;
	gchar				*working_path;
	GConfClient			*gconf_client;
};

enum {
	PROP_0,
	PROP_BASENAME,
	PROP_MODEL,
	PROP_DESCRIPTION,
	PROP_SERIAL,
	PROP_DEVICE,
	PROP_MANUFACTURER,
	PROP_REFERENCE_KIND,
	PROP_DEVICE_KIND,
	PROP_PRINT_KIND,
	PROP_DEVICE_TYPE,
	PROP_COLORIMETER_KIND,
	PROP_OUTPUT_NAME,
	PROP_FILENAME_SOURCE,
	PROP_FILENAME_REFERENCE,
	PROP_FILENAME_RESULT,
	PROP_WORKING_PATH,
	PROP_PRECISION,
	PROP_LAST
};

G_DEFINE_TYPE (GcmCalibrate, gcm_calibrate, G_TYPE_OBJECT)

/**
 * gcm_calibrate_get_model_fallback:
 **/
const gchar *
gcm_calibrate_get_model_fallback (GcmCalibrate *calibrate)
{
	GcmCalibratePrivate *priv = calibrate->priv;
	if (priv->model != NULL)
		return priv->model;

	/* TRANSLATORS: this is saved in the profile */
	return _("Unknown model");
}

/**
 * gcm_calibrate_get_description_fallback:
 **/
const gchar *
gcm_calibrate_get_description_fallback (GcmCalibrate *calibrate)
{
	GcmCalibratePrivate *priv = calibrate->priv;
	if (priv->description != NULL)
		return priv->description;

	/* TRANSLATORS: this is saved in the profile */
	return _("Unknown description");
}

/**
 * gcm_calibrate_get_manufacturer_fallback:
 **/
const gchar *
gcm_calibrate_get_manufacturer_fallback (GcmCalibrate *calibrate)
{
	GcmCalibratePrivate *priv = calibrate->priv;
	if (priv->manufacturer != NULL)
		return priv->manufacturer;

	/* TRANSLATORS: this is saved in the profile */
	return _("Unknown manufacturer");
}

/**
 * gcm_calibrate_get_device_fallback:
 **/
const gchar *
gcm_calibrate_get_device_fallback (GcmCalibrate *calibrate)
{
	GcmCalibratePrivate *priv = calibrate->priv;
	if (priv->device != NULL)
		return priv->device;

	/* TRANSLATORS: this is saved in the profile */
	return _("Unknown device");
}

/**
 * gcm_calibrate_get_time:
 **/
static gchar *
gcm_calibrate_get_time (void)
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
 * gcm_calibrate_set_basename:
 **/
static void
gcm_calibrate_set_basename (GcmCalibrate *calibrate)
{
	gchar *serial = NULL;
	gchar *manufacturer = NULL;
	gchar *model = NULL;
	gchar *timespec = NULL;
	GDate *date = NULL;
	GString *basename;

	/* get device properties */
	g_object_get (calibrate,
		      "serial", &serial,
		      "manufacturer", &manufacturer,
		      "model", &model,
		      NULL);

	/* create date and set it to now */
	date = g_date_new ();
	g_date_set_time_t (date, time (NULL));
	timespec = gcm_calibrate_get_time ();

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

	/* save this */
	g_object_set (calibrate, "basename", basename->str, NULL);

	g_date_free (date);
	g_free (serial);
	g_free (manufacturer);
	g_free (model);
	g_free (timespec);
	g_string_free (basename, TRUE);
}

/**
 * gcm_calibrate_set_from_device:
 **/
gboolean
gcm_calibrate_set_from_device (GcmCalibrate *calibrate, GcmDevice *device, GError **error)
{
	gboolean ret = TRUE;
	gchar *native_device = NULL;
	gchar *manufacturer = NULL;
	gchar *model = NULL;
	gchar *description = NULL;
	gchar *serial = NULL;
	GcmDeviceTypeEnum type;

	/* get the device */
	g_object_get (device,
		      "type", &type,
		      "serial", &serial,
		      "model", &model,
		      "title", &description,
		      "manufacturer", &manufacturer,
		      NULL);

	/* set the proper values */
	g_object_set (calibrate,
		      "device-type", type,
		      "model", model,
		      "description", description,
		      "manufacturer", manufacturer,
		      "serial", serial,
		      NULL);

	/* get a filename based on calibration attributes we've just set */
	gcm_calibrate_set_basename (calibrate);

	/* display specific properties */
	if (type == GCM_DEVICE_TYPE_ENUM_DISPLAY) {
		g_object_get (device,
			      "native-device", &native_device,
			      NULL);
		if (native_device == NULL) {
			g_set_error (error,
				     GCM_CALIBRATE_ERROR,
				     GCM_CALIBRATE_ERROR_INTERNAL,
				     "failed to get output");
			ret = FALSE;
			goto out;
		}
		g_object_set (calibrate,
			      "output-name", native_device,
			      NULL);
	}

out:
	g_free (native_device);
	g_free (manufacturer);
	g_free (model);
	g_free (description);
	g_free (serial);
	return ret;
}

/**
 * gcm_calibrate_set_from_exif:
 **/
gboolean
gcm_calibrate_set_from_exif (GcmCalibrate *calibrate, const gchar *filename, GError **error)
{
	const gchar *manufacturer = NULL;
	const gchar *model = NULL;
	gchar *description = NULL;
	const gchar *serial = NULL;
	TIFF *tiff;
	gboolean ret = TRUE;

	/* open file */
	tiff = TIFFOpen (filename, "r");
	TIFFGetField (tiff,TIFFTAG_MAKE, &manufacturer);
	TIFFGetField (tiff,TIFFTAG_MODEL, &model);
	TIFFGetField (tiff,TIFFTAG_CAMERASERIALNUMBER, &serial);

	/* we failed to get data */
	if (manufacturer == NULL || model == NULL) {
		g_set_error (error,
			     GCM_CALIBRATE_ERROR,
			     GCM_CALIBRATE_ERROR_NO_DATA,
			     "failed to get EXIF data from TIFF");
		ret = FALSE;
		goto out;
	}

	/* do the best we can */
	description = g_strdup_printf ("%s - %s", manufacturer, model);

	/* only set what we've got, don't nuke perfectly good device data */
	if (model != NULL)
		g_object_set (calibrate, "model", model, NULL);
	if (description != NULL)
		g_object_set (calibrate, "description", description, NULL);
	if (manufacturer != NULL)
		g_object_set (calibrate, "manufacturer", manufacturer, NULL);
	if (serial != NULL)
		g_object_set (calibrate, "serial", serial, NULL);

out:
	g_free (description);
	TIFFClose (tiff);
	return ret;
}

/**
 * gcm_calibrate_get_display_type:
 **/
static gboolean
gcm_calibrate_get_display_type (GcmCalibrate *calibrate, GtkWindow *window, GError **error)
{
	gboolean ret = TRUE;
	const gchar *title;
	const gchar *message;
	GtkResponseType response;
	GcmCalibratePrivate *priv = calibrate->priv;

	/* TRANSLATORS: title, usually we can tell based on the EDID data or output name */
	title = _("Could not detect screen type");

	/* TRANSLATORS: dialog message */
	message = _("Please indicate if the screen you are trying to profile is an LCD, CRT or a projector.");

	/* show the ui */
	gcm_calibrate_dialog_set_window (priv->calibrate_dialog, window);
	gcm_calibrate_dialog_show (priv->calibrate_dialog, GCM_CALIBRATE_DIALOG_TAB_DISPLAY_TYPE, title, message);
	gcm_calibrate_dialog_set_show_button_ok (priv->calibrate_dialog, FALSE);
	gcm_calibrate_dialog_set_show_expander (priv->calibrate_dialog, FALSE);
	response = gcm_calibrate_dialog_run (priv->calibrate_dialog);
	if (response != GTK_RESPONSE_OK) {
		gcm_calibrate_dialog_hide (priv->calibrate_dialog);
		g_set_error_literal (error,
				     GCM_CALIBRATE_ERROR,
				     GCM_CALIBRATE_ERROR_USER_ABORT,
				     "user did not choose crt or lcd");
		ret = FALSE;
		goto out;
	}

	/* copy */
	g_object_get (priv->calibrate_dialog, "device-kind", &priv->device_kind, NULL);

	/* can this device support projectors? */
	if (priv->device_kind == GCM_CALIBRATE_DEVICE_KIND_PROJECTOR &&
	    !gcm_colorimeter_supports_projector (priv->colorimeter)) {
		/* TRANSLATORS: title, the hardware calibration device does not support projectors */
		title = _("Could not calibrate using this colorimeter device");

		/* TRANSLATORS: dialog message */
		message = _("This colorimeter device is not designed to support profiling projectors.");

		/* ask the user again */
		gcm_calibrate_dialog_show (priv->calibrate_dialog, GCM_CALIBRATE_DIALOG_TAB_GENERIC, title, message);
		gcm_calibrate_dialog_set_show_button_ok (priv->calibrate_dialog, FALSE);
		gcm_calibrate_dialog_set_show_expander (priv->calibrate_dialog, FALSE);
		response = gcm_calibrate_dialog_run (priv->calibrate_dialog);
		if (response != GTK_RESPONSE_OK) {
			gcm_calibrate_dialog_hide (priv->calibrate_dialog);
			g_set_error_literal (error,
					     GCM_CALIBRATE_ERROR,
					     GCM_CALIBRATE_ERROR_NO_SUPPORT,
					     "hardware not capable of profiling a projector");
			ret = FALSE;
			goto out;
		}
	}
out:
	return ret;
}

/**
 * gcm_calibrate_set_working_path:
 **/
static gboolean
gcm_calibrate_set_working_path (GcmCalibrate *calibrate, GError **error)
{
	gboolean ret = FALSE;
	gchar *timespec = NULL;
	gchar *folder = NULL;
	GcmCalibratePrivate *priv = calibrate->priv;

	/* remove old value */
	g_free (priv->working_path);

	/* use the basename and the timespec */
	timespec = gcm_calibrate_get_time ();
	folder = g_strjoin (" - ", priv->basename, timespec, NULL);
	priv->working_path = g_build_filename (g_get_user_config_dir (), "gnome-color-manager", "calibration", folder, NULL);
	ret = gcm_utils_mkdir_with_parents (priv->working_path, error);
	g_free (timespec);
	g_free (folder);
	return ret;
}

/**
 * gcm_calibrate_display:
 **/
gboolean
gcm_calibrate_display (GcmCalibrate *calibrate, GtkWindow *window, GError **error)
{
	GcmCalibrateClass *klass = GCM_CALIBRATE_GET_CLASS (calibrate);
	gboolean ret = TRUE;
	const gchar *hardware_device;
	gboolean ret_tmp;
	GString *string = NULL;
	GcmBrightness *brightness = NULL;
	guint percentage = G_MAXUINT;
	GtkResponseType response;
	GError *error_tmp = NULL;
	GcmCalibratePrivate *priv = calibrate->priv;

	/* coldplug source */
	if (priv->output_name == NULL) {
		ret = FALSE;
		g_set_error_literal (error,
				     GCM_CALIBRATE_ERROR,
				     GCM_CALIBRATE_ERROR_INTERNAL,
				     "no output name set");
		goto out;
	}

	/* set the per-profile filename */
	ret = gcm_calibrate_set_working_path (calibrate, error);
	if (!ret)
		goto out;

	/* coldplug source */
	if (klass->calibrate_display == NULL) {
		ret = FALSE;
		g_set_error_literal (error,
				     GCM_CALIBRATE_ERROR,
				     GCM_CALIBRATE_ERROR_INTERNAL,
				     "no klass support");
		goto out;
	}

	/* get calibration device model */
	hardware_device = gcm_colorimeter_get_model (priv->colorimeter);

	/* get device, harder */
	if (hardware_device == NULL) {
		/* TRANSLATORS: this is the formattted custom profile description. "Custom" refers to the fact that it's user generated */
		hardware_device = g_strdup (_("Custom"));
	}

	/* set display specific properties */
	g_object_set (calibrate,
		      "device", hardware_device,
		      NULL);

	/* this wasn't previously set */
	if (priv->device_kind == GCM_CALIBRATE_DEVICE_KIND_UNKNOWN) {
		ret = gcm_calibrate_get_display_type (calibrate, window, error);
		if (!ret)
			goto out;
	}

	/* show a warning for external monitors */
	ret = gcm_utils_output_is_lcd_internal (priv->output_name);
	if (!ret) {
		string = g_string_new ("");

		/* TRANSLATORS: dialog message, preface */
		g_string_append_printf (string, "%s\n", _("Before calibrating the display, it is recommended to configure your display with the following settings to get optimal results."));

		/* TRANSLATORS: dialog message, preface */
		g_string_append_printf (string, "%s\n\n", _("You may want to consult the owner's manual for your display on how to achieve these settings."));

		/* TRANSLATORS: dialog message, bullet item */
		g_string_append_printf (string, "• %s\n", _("Reset your display to the factory defaults."));

		/* TRANSLATORS: dialog message, bullet item */
		g_string_append_printf (string, "• %s\n", _("Disable dynamic contrast if your display has this feature."));

		/* TRANSLATORS: dialog message, bullet item */
		g_string_append_printf (string, "• %s", _("Configure your display with custom color settings and ensure the RGB channels are set to the same values."));

		/* TRANSLATORS: dialog message, addition to bullet item */
		g_string_append_printf (string, " %s\n", _("If custom color is not available then use a 6500K color temperature."));

		/* TRANSLATORS: dialog message, bullet item */
		g_string_append_printf (string, "• %s\n", _("Adjust the display brightness to a comfortable level for prolonged viewing."));

		/* TRANSLATORS: dialog message, suffix */
		g_string_append_printf (string, "\n%s\n", _("For best results, the display should have been powered for at least 15 minutes before starting the calibration."));

		/* TRANSLATORS: window title */
		gcm_calibrate_dialog_set_window (priv->calibrate_dialog, window);
		gcm_calibrate_dialog_show (priv->calibrate_dialog, GCM_CALIBRATE_DIALOG_TAB_GENERIC, _("Display setup"), string->str);
		gcm_calibrate_dialog_set_show_button_ok (priv->calibrate_dialog, TRUE);
		gcm_calibrate_dialog_set_show_expander (priv->calibrate_dialog, FALSE);
		response = gcm_calibrate_dialog_run (priv->calibrate_dialog);
		if (response != GTK_RESPONSE_OK) {
			gcm_calibrate_dialog_hide (priv->calibrate_dialog);
			g_set_error_literal (error,
					     GCM_CALIBRATE_ERROR,
					     GCM_CALIBRATE_ERROR_USER_ABORT,
					     "user did not follow calibration steps");
			ret = FALSE;
			goto out;
		}
	}

	/* create new helper objects */
	brightness = gcm_brightness_new ();

	/* if we are an internal LCD, we need to set the brightness to maximum */
	ret = gcm_utils_output_is_lcd_internal (priv->output_name);
	if (ret) {
		/* get the old brightness so we can restore state */
		ret = gcm_brightness_get_percentage (brightness, &percentage, &error_tmp);
		if (!ret) {
			egg_warning ("failed to get brightness: %s", error_tmp->message);
			g_error_free (error_tmp);
			/* not fatal */
			error_tmp = NULL;
		}

		/* set the new brightness */
		ret = gcm_brightness_set_percentage (brightness, 100, &error_tmp);
		if (!ret) {
			egg_warning ("failed to set brightness: %s", error_tmp->message);
			g_error_free (error_tmp);
			/* not fatal */
			error_tmp = NULL;
		}
	}

	/* proxy */
	ret = klass->calibrate_display (calibrate, window, error);
out:
	/* restore brightness */
	if (percentage != G_MAXUINT) {
		/* set the new brightness */
		ret_tmp = gcm_brightness_set_percentage (brightness, percentage, &error_tmp);
		if (!ret_tmp) {
			egg_warning ("failed to restore brightness: %s", error_tmp->message);
			g_error_free (error_tmp);
			/* not fatal */
			error = NULL;
		}
	}

	if (brightness != NULL)
		g_object_unref (brightness);
	if (string != NULL)
		g_string_free (string, TRUE);
	return ret;
}


/**
 * gcm_calibrate_device_get_reference_image:
 **/
static gchar *
gcm_calibrate_device_get_reference_image (const gchar *directory, GtkWindow *window)
{
	gchar *filename = NULL;
	GtkWidget *dialog;
	GtkFileFilter *filter;

	/* TRANSLATORS: dialog for file->open dialog */
	dialog = gtk_file_chooser_dialog_new (_("Select reference image"), window,
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

	/* setup the all files filter */
	filter = gtk_file_filter_new ();
	gtk_file_filter_add_pattern (filter, "*");
	/* TRANSLATORS: filter name on the file->open dialog */
	gtk_file_filter_set_name (filter, _("All files"));
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER(dialog), filter);

	/* did user choose file */
	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER(dialog));

	/* we're done */
	gtk_widget_destroy (dialog);

	/* or NULL for missing */
	return filename;
}

/**
 * gcm_calibrate_device_get_reference_data:
 **/
static gchar *
gcm_calibrate_device_get_reference_data (const gchar *directory, GtkWindow *window)
{
	gchar *filename = NULL;
	GtkWidget *dialog;
	GtkFileFilter *filter;

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

	/* setup the all files filter */
	filter = gtk_file_filter_new ();
	gtk_file_filter_add_pattern (filter, "*");
	/* TRANSLATORS: filter name on the file->open dialog */
	gtk_file_filter_set_name (filter, _("All files"));
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER(dialog), filter);

	/* did user choose file */
	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER(dialog));

	/* we're done */
	gtk_widget_destroy (dialog);

	/* or NULL for missing */
	return filename;
}

/**
 * gcm_calibrate_get_device_for_it8_file:
 **/
static gchar *
gcm_calibrate_get_device_for_it8_file (const gchar *filename)
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
 * gcm_calibrate_file_chooser_get_working_path:
 **/
static gchar *
gcm_calibrate_file_chooser_get_working_path (GcmCalibrate *calibrate, GtkWindow *window)
{
	GtkWidget *dialog;
	gchar *current_folder;
	gchar *working_path = NULL;

	/* start in the correct place */
	current_folder = g_build_filename (g_get_user_config_dir (), "gnome-color-manager", "calibration", NULL);

	/* TRANSLATORS: dialog for file->open dialog */
	dialog = gtk_file_chooser_dialog_new (_("Select ICC Profile File"), window,
					       GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
					       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					       _("Open"), GTK_RESPONSE_ACCEPT,
					      NULL);
	gtk_window_set_icon_name (GTK_WINDOW (dialog), GCM_STOCK_ICON);
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER(dialog), current_folder);
	gtk_file_chooser_set_create_folders (GTK_FILE_CHOOSER(dialog), FALSE);
	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER(dialog), FALSE);

	/* did user choose file */
	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
		working_path = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER(dialog));

	/* we're done */
	gtk_widget_destroy (dialog);

	/* or NULL for missing */
	g_free (current_folder);
	return working_path;
}

/**
 * gcm_calibrate_printer:
 **/
gboolean
gcm_calibrate_printer (GcmCalibrate *calibrate, GtkWindow *window, GError **error)
{
	gboolean ret = FALSE;
	const gchar *title;
	const gchar *message;
	gchar *ptr;
	GtkWindow *window_tmp;
	GtkResponseType response;
	GcmCalibrateClass *klass = GCM_CALIBRATE_GET_CLASS (calibrate);
	GcmCalibratePrivate *priv = calibrate->priv;

	/* TRANSLATORS: title, you can profile all at once, or in steps */
	title = _("Please choose a profiling mode");

	/* TRANSLATORS: dialog message */
	message = _("Please indicate if you want to profile a local printer, generate some reference images, or process some reference images.");

	/* push new messages into the UI */
	gcm_calibrate_dialog_show (priv->calibrate_dialog, GCM_CALIBRATE_DIALOG_TAB_PRINT_MODE, title, message);
	gcm_calibrate_dialog_set_show_button_ok (priv->calibrate_dialog, FALSE);
	gcm_calibrate_dialog_set_show_expander (priv->calibrate_dialog, FALSE);
	response = gcm_calibrate_dialog_run (priv->calibrate_dialog);
	if (response != GTK_RESPONSE_OK) {
		gcm_calibrate_dialog_hide (priv->calibrate_dialog);
		g_set_error_literal (error,
				     GCM_CALIBRATE_ERROR,
				     GCM_CALIBRATE_ERROR_USER_ABORT,
				     "user did not choose print mode");
		ret = FALSE;
		goto out;
	}

	/* copy */
	g_object_get (priv->calibrate_dialog, "print-kind", &priv->print_kind, NULL);

	if (priv->print_kind != GCM_CALIBRATE_PRINT_KIND_ANALYZE) {
		/* set the per-profile filename */
		ret = gcm_calibrate_set_working_path (calibrate, error);
		if (!ret)
			goto out;
	} else {

		/* remove previously set value (if any) */
		g_free (priv->working_path);
		priv->working_path = NULL;

		/* get from the user */
		window_tmp = gcm_calibrate_dialog_get_window (priv->calibrate_dialog);
		priv->working_path = gcm_calibrate_file_chooser_get_working_path (calibrate, window_tmp);
		if (priv->working_path == NULL) {
			g_set_error_literal (error,
					     GCM_CALIBRATE_ERROR,
					     GCM_CALIBRATE_ERROR_USER_ABORT,
					     "user did not choose folder");
			ret = FALSE;
			goto out;
		}

		/* reprogram the basename */
		g_free (priv->basename);
		priv->basename = g_path_get_basename (priv->working_path);

		/* remove the timespec */
		ptr = g_strrstr (priv->basename, " - ");
		if (ptr != NULL)
			ptr[0] = '\0';
	}

	/* coldplug source */
	if (klass->calibrate_printer == NULL) {
		g_set_error_literal (error,
				     GCM_CALIBRATE_ERROR,
				     GCM_CALIBRATE_ERROR_INTERNAL,
				     "no klass support");
		goto out;
	}

	/* proxy */
	ret = klass->calibrate_printer (calibrate, window, error);
out:
	return ret;
}

/**
 * gcm_calibrate_get_precision:
 **/
static GcmCalibratePrecision
gcm_calibrate_get_precision (GcmCalibrate *calibrate, GError **error)
{
	GcmCalibratePrecision precision = GCM_CALIBRATE_PRECISION_UNKNOWN;
	const gchar *title;
	GString *string;
	GtkResponseType response;
	GcmCalibratePrivate *priv = calibrate->priv;

	string = g_string_new ("");

	/* TRANSLATORS: dialog title */
	title = _("Choose the precision of the profile");

	/* TRANSLATORS: dialog message, suffix */
	g_string_append_printf (string, "%s\n", _("Please choose the profile precision."));

	/* TRANSLATORS: this is the message body for the chart selection */
	g_string_append_printf (string, "\n\n%s", _("For a typical workflow, a normal precision profile is sufficient."));

	/* TRANSLATORS: this is the message body for the chart selection */
	g_string_append_printf (string, "\n%s", _("High precision profiles provide higher accuracy in color matching. Correspondingly, low precision profiles result in lower quality."));

	/* printer specific options */
	if (priv->device_type == GCM_DEVICE_TYPE_ENUM_PRINTER) {
		/* TRANSLATORS: dialog message, preface */
		g_string_append_printf (string, "\n%s", _("The high precision profiles also require more paper and time for reading the color swatches."));
	}

	/* push new messages into the UI */
	gcm_calibrate_dialog_show (priv->calibrate_dialog, GCM_CALIBRATE_DIALOG_TAB_PRECISION, title, string->str);
	gcm_calibrate_dialog_set_show_button_ok (priv->calibrate_dialog, FALSE);
	gcm_calibrate_dialog_set_show_expander (priv->calibrate_dialog, FALSE);
	response = gcm_calibrate_dialog_run (priv->calibrate_dialog);
	if (response != GTK_RESPONSE_OK) {
		gcm_calibrate_dialog_hide (priv->calibrate_dialog);
		g_set_error_literal (error,
				     GCM_CALIBRATE_ERROR,
				     GCM_CALIBRATE_ERROR_USER_ABORT,
				     "user did not choose precision type and ask is specified in GConf");
		goto out;
	}

	/* copy */
	g_object_get (priv->calibrate_dialog, "precision", &precision, NULL);
out:
	if (string != NULL)
		g_string_free (string, TRUE);
	return precision;
}

/**
 * gcm_calibrate_precision_from_string:
 **/
static GcmCalibratePrecision
gcm_calibrate_precision_from_string (const gchar *string)
{
	if (g_strcmp0 (string, "short") == 0)
		return GCM_CALIBRATE_PRECISION_SHORT;
	if (g_strcmp0 (string, "normal") == 0)
		return GCM_CALIBRATE_PRECISION_NORMAL;
	if (g_strcmp0 (string, "long") == 0)
		return GCM_CALIBRATE_PRECISION_LONG;
	if (g_strcmp0 (string, "ask") == 0)
		return GCM_CALIBRATE_PRECISION_UNKNOWN;
	egg_warning ("failed to convert to precision: %s", string);
	return GCM_CALIBRATE_PRECISION_UNKNOWN;
}

/**
 * gcm_calibrate_device:
 **/
gboolean
gcm_calibrate_device (GcmCalibrate *calibrate, GtkWindow *window, GError **error)
{
	gboolean ret = FALSE;
	gboolean has_shared_targets;
	gchar *reference_image = NULL;
	gchar *reference_data = NULL;
	gchar *device = NULL;
	const gchar *directory;
	GString *string;
	GtkResponseType response;
	GtkWindow *window_tmp;
	gchar *precision = NULL;
#ifdef GCM_USE_PACKAGEKIT
	GtkWidget *dialog;
#endif
	const gchar *title;
	GcmCalibratePrivate *priv = calibrate->priv;
	GcmCalibrateClass *klass = GCM_CALIBRATE_GET_CLASS (calibrate);

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
		g_string_append_printf (string, "%s\n\n", _("Do you want them to be installed?"));
		/* TRANSLATORS: dialog message, if the user has the target file on a CDROM then there's no need for this package */
		g_string_append_printf (string, "%s", _("If you already have the correct file, you can skip this step."));

		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", string->str);
		gtk_window_set_icon_name (GTK_WINDOW (dialog), GCM_STOCK_ICON);
		/* TRANSLATORS: button, skip installing a package */
		gtk_dialog_add_button (GTK_DIALOG (dialog), _("Do not install"), GTK_RESPONSE_CANCEL);
		/* TRANSLATORS: button, install a package */
		gtk_dialog_add_button (GTK_DIALOG (dialog), _("Install"), GTK_RESPONSE_YES);
		response = gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		/* only install if the user wanted to */
		if (response == GTK_RESPONSE_YES)
			has_shared_targets = gcm_utils_install_package (GCM_PREFS_PACKAGE_NAME_SHARED_COLOR_TARGETS, window);
#else
		egg_warning ("cannot install: this package was not compiled with --enable-packagekit");
#endif
	}

	/* TRANSLATORS: this is the window title for when the user selects the chart type.
	                A chart is a type of reference image the user has purchased. */
	title = _("Please select chart type");
	g_string_set_size (string, 0);

	/* TRANSLATORS: dialog message, preface */
	g_string_append_printf (string, "%s\n", _("Before calibrating the device, you have to manually capture an image of a calibrated target and save it as a TIFF image file."));

	/* scanner specific options */
	if (priv->device_type == GCM_DEVICE_TYPE_ENUM_SCANNER) {
		/* TRANSLATORS: dialog message, preface */
		g_string_append_printf (string, "%s\n", _("Ensure that the contrast and brightness are not changed and color correction profiles are not applied."));

		/* TRANSLATORS: dialog message, suffix */
		g_string_append_printf (string, "%s\n", _("The device sensor should have been cleaned prior to scanning and the output file resolution should be at least 200dpi."));
	}

	/* camera specific options */
	if (priv->device_type == GCM_DEVICE_TYPE_ENUM_CAMERA) {
		/* TRANSLATORS: dialog message, preface */
		g_string_append_printf (string, "%s\n", _("Ensure that the white-balance has not been modified by the camera and that the lens is clean."));
	}

	/* TRANSLATORS: dialog message, suffix */
	g_string_append_printf (string, "\n%s\n", _("For best results, the reference target should also be less than two years old."));

	/* TRANSLATORS: this is the message body for the chart selection */
	g_string_append_printf (string, "\n%s\n", _("Please select the chart type which corresponds to your reference file."));

	/* push new messages into the UI */
	gcm_calibrate_dialog_set_window (priv->calibrate_dialog, window);
	gcm_calibrate_dialog_show (priv->calibrate_dialog, GCM_CALIBRATE_DIALOG_TAB_TARGET_TYPE, title, string->str);
	gcm_calibrate_dialog_set_show_button_ok (priv->calibrate_dialog, TRUE);
	gcm_calibrate_dialog_set_show_expander (priv->calibrate_dialog, FALSE);
	response = gcm_calibrate_dialog_run (priv->calibrate_dialog);
	if (response != GTK_RESPONSE_OK) {
		gcm_calibrate_dialog_hide (priv->calibrate_dialog);
		g_set_error_literal (error,
				     GCM_CALIBRATE_ERROR,
				     GCM_CALIBRATE_ERROR_USER_ABORT,
				     "user did not choose chart type");
		ret = FALSE;
		goto out;
	}

	/* copy */
	g_object_get (priv->calibrate_dialog, "reference-kind", &priv->reference_kind, NULL);

	/* get default precision */
	precision = gconf_client_get_string (priv->gconf_client, GCM_SETTINGS_CALIBRATION_LENGTH, NULL);
	priv->precision = gcm_calibrate_precision_from_string (precision);
	if (priv->precision == GCM_CALIBRATE_PRECISION_UNKNOWN) {
		priv->precision = gcm_calibrate_get_precision (calibrate, error);
		if (priv->precision == GCM_CALIBRATE_PRECISION_UNKNOWN) {
			ret = FALSE;
			goto out;
		}
	}

	/* get scanned image */
	directory = g_get_home_dir ();
	window_tmp = gcm_calibrate_dialog_get_window (priv->calibrate_dialog);
	reference_image = gcm_calibrate_device_get_reference_image (directory, window_tmp);
	if (reference_image == NULL) {
		g_set_error_literal (error,
				     GCM_CALIBRATE_ERROR,
				     GCM_CALIBRATE_ERROR_USER_ABORT,
				     "could not get reference image");
		ret = FALSE;
		goto out;
	}

	/* use the exif data if there is any present */
	ret = gcm_calibrate_set_from_exif (calibrate, reference_image, NULL);
	if (!ret)
		egg_debug ("no EXIF data, so using device attributes");

	/* get reference data */
	directory = has_shared_targets ? "/usr/share/color/targets" : "/media";
	reference_data = gcm_calibrate_device_get_reference_data (directory, window_tmp);
	if (reference_data == NULL) {
		g_set_error_literal (error,
				     GCM_CALIBRATE_ERROR,
				     GCM_CALIBRATE_ERROR_USER_ABORT,
				     "could not get reference target");
		ret = FALSE;
		goto out;
	}

	/* use the ORIGINATOR in the it8 file */
	device = gcm_calibrate_get_device_for_it8_file (reference_data);
	if (device == NULL)
		device = g_strdup ("IT8.7");

	/* set the calibration parameters */
	g_object_set (calibrate,
		      "filename-source", reference_image,
		      "filename-reference", reference_data,
		      "device", device,
		      NULL);

	/* set the per-profile filename */
	ret = gcm_calibrate_set_working_path (calibrate, error);
	if (!ret)
		goto out;

	/* coldplug source */
	if (klass->calibrate_device == NULL) {
		g_set_error_literal (error,
				     GCM_CALIBRATE_ERROR,
				     GCM_CALIBRATE_ERROR_INTERNAL,
				     "no klass support");
		goto out;
	}

	/* proxy */
	ret = klass->calibrate_device (calibrate, window, error);
out:
	if (string != NULL)
		g_string_free (string, TRUE);
	g_free (precision);
	g_free (device);
	g_free (reference_image);
	g_free (reference_data);
	return ret;
}

/**
 * gcm_calibrate_get_property:
 **/
static void
gcm_calibrate_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GcmCalibrate *calibrate = GCM_CALIBRATE (object);
	GcmCalibratePrivate *priv = calibrate->priv;

	switch (prop_id) {
	case PROP_REFERENCE_KIND:
		g_value_set_uint (value, priv->reference_kind);
		break;
	case PROP_DEVICE_KIND:
		g_value_set_uint (value, priv->device_kind);
		break;
	case PROP_PRINT_KIND:
		g_value_set_uint (value, priv->print_kind);
		break;
	case PROP_DEVICE_TYPE:
		g_value_set_uint (value, priv->device_type);
		break;
	case PROP_COLORIMETER_KIND:
		g_value_set_uint (value, priv->colorimeter_kind);
		break;
	case PROP_OUTPUT_NAME:
		g_value_set_string (value, priv->output_name);
		break;
	case PROP_FILENAME_SOURCE:
		g_value_set_string (value, priv->filename_source);
		break;
	case PROP_FILENAME_REFERENCE:
		g_value_set_string (value, priv->filename_reference);
		break;
	case PROP_FILENAME_RESULT:
		g_value_set_string (value, priv->filename_result);
		break;
	case PROP_BASENAME:
		g_value_set_string (value, priv->basename);
		break;
	case PROP_MODEL:
		g_value_set_string (value, priv->model);
		break;
	case PROP_DESCRIPTION:
		g_value_set_string (value, priv->description);
		break;
	case PROP_SERIAL:
		g_value_set_string (value, priv->serial);
		break;
	case PROP_DEVICE:
		g_value_set_string (value, priv->device);
		break;
	case PROP_MANUFACTURER:
		g_value_set_string (value, priv->manufacturer);
		break;
	case PROP_WORKING_PATH:
		g_value_set_string (value, priv->working_path);
		break;
	case PROP_PRECISION:
		g_value_set_uint (value, priv->precision);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gcm_calibrate_guess_type:
 **/
static void
gcm_calibrate_guess_type (GcmCalibrate *calibrate)
{
	gboolean ret;
	GcmCalibratePrivate *priv = calibrate->priv;

	/* guess based on the output name */
	ret = gcm_utils_output_is_lcd_internal (priv->output_name);
	if (ret)
		priv->device_kind = GCM_CALIBRATE_DEVICE_KIND_LCD;
}

/**
 * gcm_prefs_colorimeter_changed_cb:
 **/
static void
gcm_prefs_colorimeter_changed_cb (GcmColorimeter *_colorimeter, GcmCalibrate *calibrate)
{
	calibrate->priv->colorimeter_kind = gcm_colorimeter_get_kind (_colorimeter);
	g_object_notify (G_OBJECT (calibrate), "colorimeter-kind");
}

/**
 * gcm_calibrate_set_property:
 **/
static void
gcm_calibrate_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	GcmCalibrate *calibrate = GCM_CALIBRATE (object);
	GcmCalibratePrivate *priv = calibrate->priv;

	switch (prop_id) {
	case PROP_OUTPUT_NAME:
		g_free (priv->output_name);
		priv->output_name = g_strdup (g_value_get_string (value));
		gcm_calibrate_guess_type (calibrate);
		break;
	case PROP_FILENAME_SOURCE:
		g_free (priv->filename_source);
		priv->filename_source = g_strdup (g_value_get_string (value));
		break;
	case PROP_FILENAME_REFERENCE:
		g_free (priv->filename_reference);
		priv->filename_reference = g_strdup (g_value_get_string (value));
		break;
	case PROP_FILENAME_RESULT:
		g_free (priv->filename_result);
		priv->filename_result = g_strdup (g_value_get_string (value));
		break;
	case PROP_BASENAME:
		g_free (priv->basename);
		priv->basename = g_strdup (g_value_get_string (value));
		gcm_utils_ensure_sensible_filename (priv->basename);
		break;
	case PROP_MODEL:
		g_free (priv->model);
		priv->model = g_strdup (g_value_get_string (value));
		break;
	case PROP_DESCRIPTION:
		g_free (priv->description);
		priv->description = g_strdup (g_value_get_string (value));
		break;
	case PROP_SERIAL:
		g_free (priv->serial);
		priv->serial = g_strdup (g_value_get_string (value));
		break;
	case PROP_DEVICE:
		g_free (priv->device);
		priv->device = g_strdup (g_value_get_string (value));
		break;
	case PROP_MANUFACTURER:
		g_free (priv->manufacturer);
		priv->manufacturer = g_strdup (g_value_get_string (value));
		break;
	case PROP_DEVICE_TYPE:
		priv->device_type = g_value_get_uint (value);
		break;
	case PROP_WORKING_PATH:
		g_free (priv->working_path);
		priv->working_path = g_strdup (g_value_get_string (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gcm_calibrate_class_init:
 **/
static void
gcm_calibrate_class_init (GcmCalibrateClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gcm_calibrate_finalize;
	object_class->get_property = gcm_calibrate_get_property;
	object_class->set_property = gcm_calibrate_set_property;

	/**
	 * GcmCalibrate:reference-kind:
	 */
	pspec = g_param_spec_uint ("reference-kind", NULL, NULL,
				   0, G_MAXUINT, 0,
				   G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_REFERENCE_KIND, pspec);

	/**
	 * GcmCalibrate:device-kind:
	 */
	pspec = g_param_spec_uint ("device-kind", NULL, NULL,
				   0, G_MAXUINT, 0,
				   G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_DEVICE_KIND, pspec);

	/**
	 * GcmCalibrate:print-kind:
	 */
	pspec = g_param_spec_uint ("print-kind", NULL, NULL,
				   0, G_MAXUINT, 0,
				   G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_PRINT_KIND, pspec);

	/**
	 * GcmCalibrate:device-type:
	 */
	pspec = g_param_spec_uint ("device-type", NULL, NULL,
				   0, G_MAXUINT, 0,
				   G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_DEVICE_TYPE, pspec);

	/**
	 * GcmCalibrate:colorimeter-kind:
	 */
	pspec = g_param_spec_uint ("colorimeter-kind", NULL, NULL,
				   0, G_MAXUINT, 0,
				   G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_COLORIMETER_KIND, pspec);

	/**
	 * GcmCalibrate:output-name:
	 */
	pspec = g_param_spec_string ("output-name", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_OUTPUT_NAME, pspec);

	/**
	 * GcmCalibrate:filename-source:
	 */
	pspec = g_param_spec_string ("filename-source", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_FILENAME_SOURCE, pspec);

	/**
	 * GcmCalibrate:filename-reference:
	 */
	pspec = g_param_spec_string ("filename-reference", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_FILENAME_REFERENCE, pspec);

	/**
	 * GcmCalibrate:filename-result:
	 */
	pspec = g_param_spec_string ("filename-result", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_FILENAME_RESULT, pspec);

	/**
	 * GcmCalibrate:basename:
	 */
	pspec = g_param_spec_string ("basename", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_BASENAME, pspec);

	/**
	 * GcmCalibrate:model:
	 */
	pspec = g_param_spec_string ("model", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_MODEL, pspec);

	/**
	 * GcmCalibrate:description:
	 */
	pspec = g_param_spec_string ("description", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_DESCRIPTION, pspec);

	/**
	 * GcmCalibrate:serial:
	 */
	pspec = g_param_spec_string ("serial", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_SERIAL, pspec);

	/**
	 * GcmCalibrate:device:
	 */
	pspec = g_param_spec_string ("device", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_DEVICE, pspec);

	/**
	 * GcmCalibrate:manufacturer:
	 */
	pspec = g_param_spec_string ("manufacturer", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_MANUFACTURER, pspec);

	/**
	 * GcmCalibrate:working-path:
	 */
	pspec = g_param_spec_string ("working-path", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_WORKING_PATH, pspec);

	/**
	 * GcmCalibrate:precision:
	 */
	pspec = g_param_spec_uint ("precision", NULL, NULL,
				   0, G_MAXUINT, 0,
				   G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_PRECISION, pspec);

	g_type_class_add_private (klass, sizeof (GcmCalibratePrivate));
}

/**
 * gcm_calibrate_init:
 **/
static void
gcm_calibrate_init (GcmCalibrate *calibrate)
{
	calibrate->priv = GCM_CALIBRATE_GET_PRIVATE (calibrate);
	calibrate->priv->output_name = NULL;
	calibrate->priv->filename_source = NULL;
	calibrate->priv->filename_reference = NULL;
	calibrate->priv->filename_result = NULL;
	calibrate->priv->basename = NULL;
	calibrate->priv->manufacturer = NULL;
	calibrate->priv->model = NULL;
	calibrate->priv->description = NULL;
	calibrate->priv->device = NULL;
	calibrate->priv->serial = NULL;
	calibrate->priv->working_path = NULL;
	calibrate->priv->device_kind = GCM_CALIBRATE_DEVICE_KIND_UNKNOWN;
	calibrate->priv->print_kind = GCM_CALIBRATE_PRINT_KIND_UNKNOWN;
	calibrate->priv->reference_kind = GCM_CALIBRATE_REFERENCE_KIND_UNKNOWN;
	calibrate->priv->precision = GCM_CALIBRATE_PRECISION_UNKNOWN;
	calibrate->priv->colorimeter = gcm_colorimeter_new ();
	calibrate->priv->calibrate_dialog = gcm_calibrate_dialog_new ();

	// FIXME: this has to be per-run specific
	calibrate->priv->working_path = g_strdup ("/tmp");

	/* use GConf to get defaults */
	calibrate->priv->gconf_client = gconf_client_get_default ();

	/* coldplug, and watch for changes */
	calibrate->priv->colorimeter_kind = gcm_colorimeter_get_kind (calibrate->priv->colorimeter);
	g_signal_connect (calibrate->priv->colorimeter, "changed", G_CALLBACK (gcm_prefs_colorimeter_changed_cb), calibrate);
}

/**
 * gcm_calibrate_finalize:
 **/
static void
gcm_calibrate_finalize (GObject *object)
{
	GcmCalibrate *calibrate = GCM_CALIBRATE (object);
	GcmCalibratePrivate *priv = calibrate->priv;

	g_free (priv->filename_source);
	g_free (priv->filename_reference);
	g_free (priv->filename_result);
	g_free (priv->output_name);
	g_free (priv->basename);
	g_free (priv->manufacturer);
	g_free (priv->model);
	g_free (priv->description);
	g_free (priv->device);
	g_free (priv->serial);
	g_free (priv->working_path);
	g_signal_handlers_disconnect_by_func (calibrate->priv->colorimeter, G_CALLBACK (gcm_prefs_colorimeter_changed_cb), calibrate);
	g_object_unref (priv->colorimeter);
	g_object_unref (priv->calibrate_dialog);
	g_object_unref (priv->gconf_client);

	G_OBJECT_CLASS (gcm_calibrate_parent_class)->finalize (object);
}

/**
 * gcm_calibrate_new:
 *
 * Return value: a new GcmCalibrate object.
 **/
GcmCalibrate *
gcm_calibrate_new (void)
{
	GcmCalibrate *calibrate;
	calibrate = g_object_new (GCM_TYPE_CALIBRATE, NULL);
	return GCM_CALIBRATE (calibrate);
}

/***************************************************************************
 ***                          MAKE CHECK TESTS                           ***
 ***************************************************************************/
#ifdef EGG_TEST
#include "egg-test.h"

void
gcm_calibrate_test (EggTest *test)
{
	GcmCalibrate *calibrate;
	gboolean ret;
	GError *error = NULL;
	gchar *filename;

	if (!egg_test_start (test, "GcmCalibrate"))
		return;

	/************************************************************/
	egg_test_title (test, "get a calibrate object");
	calibrate = gcm_calibrate_new ();
	egg_test_assert (test, calibrate != NULL);

	/************************************************************/
	egg_test_title (test, "calibrate display manually");
	filename = egg_test_get_data_file ("test.tif");
	ret = gcm_calibrate_set_from_exif (GCM_CALIBRATE(calibrate), filename, &error);
	if (!ret)
		egg_test_failed (test, "error: %s", error->message);
	else if (g_strcmp0 (gcm_calibrate_get_model_fallback (calibrate), "NIKON D60") != 0)
		egg_test_failed (test, "got model: %s", gcm_calibrate_get_model_fallback (calibrate));
	else if (g_strcmp0 (gcm_calibrate_get_manufacturer_fallback (calibrate), "NIKON CORPORATION") != 0)
		egg_test_failed (test, "got manufacturer: %s", gcm_calibrate_get_manufacturer_fallback (calibrate));
	else
		egg_test_success (test, NULL);

	g_object_unref (calibrate);
	g_free (filename);

	egg_test_end (test);
}
#endif


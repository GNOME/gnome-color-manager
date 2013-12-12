/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009-2012 Richard Hughes <richard@hughsie.com>
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
#include <colord-gtk.h>
#include <canberra-gtk.h>

#include "gcm-calibrate.h"
#include "gcm-utils.h"
#include "gcm-brightness.h"
#include "gcm-exif.h"

static void     gcm_calibrate_finalize	(GObject     *object);

#define GCM_CALIBRATE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GCM_TYPE_CALIBRATE, GcmCalibratePrivate))

/**
 * GcmCalibratePrivate:
 *
 * Private #GcmCalibrate data
 **/
struct _GcmCalibratePrivate
{
	GcmBrightness			*brightness;
	GcmCalibrateDisplayKind		 display_kind;
	GcmCalibratePrintKind		 print_kind;
	GcmCalibratePrecision		 precision;
	GcmCalibrateReferenceKind	 reference_kind;
	CdSensor			*sensor;
	CdDeviceKind			 device_kind;
	gchar				*output_name;
	gchar				*filename_source;
	gchar				*filename_reference;
	gchar				*filename_result;
	gchar				*basename;
	gchar				*copyright;
	gchar				*manufacturer;
	gchar				*model;
	gchar				*description;
	gchar				*serial;
	gchar				*device;
	gchar				*working_path;
	guint				 old_brightness;
	guint				 new_brightness;
	guint				 target_whitepoint;
	GtkWidget			*content_widget;
	GtkWindow			*sample_window;
	gchar				*old_message;
	gchar				*old_title;
	gboolean			 sensor_on_screen;
};

enum {
	PROP_0,
	PROP_BASENAME,
	PROP_COPYRIGHT,
	PROP_MODEL,
	PROP_DESCRIPTION,
	PROP_SERIAL,
	PROP_DEVICE,
	PROP_MANUFACTURER,
	PROP_REFERENCE_KIND,
	PROP_DISPLAY_KIND,
	PROP_PRINT_KIND,
	PROP_DEVICE_KIND,
	PROP_SENSOR_KIND,
	PROP_OUTPUT_NAME,
	PROP_FILENAME_SOURCE,
	PROP_FILENAME_REFERENCE,
	PROP_FILENAME_RESULT,
	PROP_WORKING_PATH,
	PROP_PRECISION,
	PROP_TARGET_WHITEPOINT,
	PROP_LAST
};

enum {
	SIGNAL_TITLE_CHANGED,
	SIGNAL_MESSAGE_CHANGED,
	SIGNAL_IMAGE_CHANGED,
	SIGNAL_PROGRESS_CHANGED,
	SIGNAL_INTERACTION_REQUIRED,
	SIGNAL_LAST
};

static guint signals[SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE (GcmCalibrate, gcm_calibrate, G_TYPE_OBJECT)

void
gcm_calibrate_set_content_widget (GcmCalibrate *calibrate, GtkWidget *widget)
{
	calibrate->priv->content_widget = widget;
}

GtkWidget *
gcm_calibrate_get_content_widget (GcmCalibrate *calibrate)
{
	return calibrate->priv->content_widget;
}

/**
 * gcm_calibrate_mkdir_with_parents:
 **/
static gboolean
gcm_calibrate_mkdir_with_parents (const gchar *filename, GError **error)
{
	gboolean ret;
	GFile *file = NULL;

	/* ensure desination exists */
	ret = g_file_test (filename, G_FILE_TEST_EXISTS);
	if (!ret) {
		file = g_file_new_for_path (filename);
		ret = g_file_make_directory_with_parents (file, NULL, error);
		if (!ret)
			goto out;
	}
out:
	if (file != NULL)
		g_object_unref (file);
	return ret;
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
 * gcm_calibrate_get_filename_result:
 **/
const gchar *
gcm_calibrate_get_filename_result (GcmCalibrate *calibrate)
{
	return calibrate->priv->filename_result;
}

/**
 * gcm_calibrate_get_working_path:
 **/
const gchar *
gcm_calibrate_get_working_path (GcmCalibrate *calibrate)
{
	return calibrate->priv->working_path;
}

/**
 * gcm_calibrate_get_basename:
 **/
const gchar *
gcm_calibrate_get_basename (GcmCalibrate *calibrate)
{
	return calibrate->priv->basename;
}

/**
 * gcm_calibrate_get_screen_brightness:
 **/
guint
gcm_calibrate_get_screen_brightness (GcmCalibrate *calibrate)
{
	return calibrate->priv->new_brightness;
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

	/* Use time as we can calibrate more than once per day */
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
 * gcm_calibrate_set_from_exif:
 **/
gboolean
gcm_calibrate_set_from_exif (GcmCalibrate *calibrate, const gchar *filename, GError **error)
{
	const gchar *manufacturer;
	const gchar *model;
	const gchar *serial;
	gchar *description = NULL;
	gboolean ret;
	GcmExif *exif;
	GFile *file;

	/* parse file */
	exif = gcm_exif_new ();
	file = g_file_new_for_path (filename);
	ret = gcm_exif_parse (exif, file, error);
	if (!ret)
		goto out;

	/* get data */
	manufacturer = gcm_exif_get_manufacturer (exif);
	model = gcm_exif_get_model (exif);
	serial = gcm_exif_get_serial (exif);

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
	g_object_unref (file);
	g_object_unref (exif);
	g_free (description);
	return ret;
}

void
gcm_calibrate_set_sensor (GcmCalibrate *calibrate, CdSensor *sensor)
{
	calibrate->priv->sensor = g_object_ref (sensor);
}

CdSensor *
gcm_calibrate_get_sensor (GcmCalibrate *calibrate)
{
	/* do not refcount */
	return calibrate->priv->sensor;
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
	ret = gcm_calibrate_mkdir_with_parents (priv->working_path, error);
	g_free (timespec);
	g_free (folder);
	return ret;
}

/**
 * gcm_calibrate_interaction:
 **/
void
gcm_calibrate_interaction (GcmCalibrate *calibrate, GtkResponseType response)
{
	GcmCalibrateClass *klass = GCM_CALIBRATE_GET_CLASS (calibrate);

	/* restore old status */
	if (response == GTK_RESPONSE_OK) {
		if (calibrate->priv->old_title != NULL) {
			gcm_calibrate_set_title (calibrate,
						 calibrate->priv->old_title,
						 GCM_CALIBRATE_UI_POST_INTERACTION);
		}
		if (calibrate->priv->old_message != NULL) {
			gcm_calibrate_set_message (calibrate,
						   calibrate->priv->old_message,
						   GCM_CALIBRATE_UI_POST_INTERACTION);
		}

		/* ensure the picture is cleared */
		gcm_calibrate_set_image (calibrate, NULL);
	}

	/* coldplug source */
	if (klass->interaction == NULL)
		return;

	/* proxy */
	klass->interaction (calibrate, response);
}

/**
 * gcm_calibrate_copy_file:
 **/
static gboolean
gcm_calibrate_copy_file (const gchar *src,
			 const gchar *dest,
			 GError **error)
{
	gboolean ret;
	GFile *file_src;
	GFile *file_dest;

	g_debug ("copying from %s to %s", src, dest);
	file_src = g_file_new_for_path (src);
	file_dest = g_file_new_for_path (dest);
	ret = g_file_copy (file_src,
			   file_dest,
			   G_FILE_COPY_NONE,
			   NULL,
			   NULL,
			   NULL,
			   error);
	g_object_unref (file_src);
	g_object_unref (file_dest);
	return ret;
}

/**
 * gcm_calibrate_interaction_attach:
 **/
void
gcm_calibrate_interaction_attach (GcmCalibrate *calibrate)
{
	const gchar *filename;
	GString *message;

	/* TRANSLATORS: title, instrument is a hardware color calibration sensor */
	gcm_calibrate_set_title (calibrate,
				 _("Please attach instrument"),
				 GCM_CALIBRATE_UI_INTERACTION);

	/* different messages with or without image */
	message = g_string_new ("");
	filename = cd_sensor_get_metadata_item (calibrate->priv->sensor,
						CD_SENSOR_METADATA_IMAGE_ATTACH);
	if (filename != NULL) {
		/* TRANSLATORS: dialog message, ask user to attach device, and there's an example image */
		g_string_append (message, _("Please attach the measuring instrument to the center of the screen on the gray square like the image below."));
	} else {
		/* TRANSLATORS: dialog message, ask user to attach device */
		g_string_append (message, _("Please attach the measuring instrument to the center of the screen on the gray square."));
	}

	/* this hardware doesn't suck :) */
	if (cd_sensor_get_kind (calibrate->priv->sensor) == CD_SENSOR_KIND_COLORHUG) {
		g_string_append (message, "\n\n");
		/* TRANSLATORS: dialog message, ask user to attach device */
		g_string_append (message, _("You will need to hold the device on the screen for the duration of the calibration."));
	}

	gcm_calibrate_set_message (calibrate,
				   message->str,
				   GCM_CALIBRATE_UI_INTERACTION);
	gcm_calibrate_set_image (calibrate, filename);
	gcm_calibrate_interaction_required (calibrate, _("Continue"));

	/* play sound from the naming spec */
	ca_context_play (ca_gtk_context_get (), 0,
			 CA_PROP_EVENT_ID, "dialog-information",
			 /* TRANSLATORS: this is the application name for libcanberra */
			 CA_PROP_APPLICATION_NAME, _("GNOME Color Manager"),
			 CA_PROP_EVENT_DESCRIPTION, "interaction required", NULL);

	g_string_free (message, TRUE);
}

/**
 * gcm_calibrate_interaction_screen:
 **/
void
gcm_calibrate_interaction_screen (GcmCalibrate *calibrate)
{
	const gchar *filename;

	/* TRANSLATORS: title, instrument is a hardware color calibration sensor */
	gcm_calibrate_set_title (calibrate,
				 _("Please configure instrument"),
				 GCM_CALIBRATE_UI_INTERACTION);

	/* get the image, if we have one */
	filename = cd_sensor_get_metadata_item (calibrate->priv->sensor,
						CD_SENSOR_METADATA_IMAGE_CALIBRATE);
	gcm_calibrate_set_image (calibrate, filename);
	gcm_calibrate_interaction_required (calibrate, _("Continue"));
	if (filename != NULL) {
		/* TRANSLATORS: this is when the user has to change a setting on the sensor, and we're showing a picture */
		gcm_calibrate_set_message (calibrate,
					   _("Please set the measuring instrument to screen mode like the image below."),
					   GCM_CALIBRATE_UI_INTERACTION);
	} else {
		/* TRANSLATORS: this is when the user has to change a setting on the sensor */
		gcm_calibrate_set_message (calibrate,
					   _("Please set the measuring instrument to screen mode."),
					   GCM_CALIBRATE_UI_INTERACTION);
	}

	/* play sound from the naming spec */
	ca_context_play (ca_gtk_context_get (), 0,
			 CA_PROP_EVENT_ID, "dialog-information",
			 /* TRANSLATORS: this is the application name for libcanberra */
			 CA_PROP_APPLICATION_NAME, _("GNOME Color Manager"),
			 CA_PROP_EVENT_DESCRIPTION, "correct hardware state", NULL);
}

/**
 * gcm_calibrate_interaction_calibrate:
 **/
void
gcm_calibrate_interaction_calibrate (GcmCalibrate *calibrate)
{
	const gchar *filename;

	/* TRANSLATORS: title, instrument is a hardware color calibration sensor */
	gcm_calibrate_set_title (calibrate,
				 _("Please configure instrument"),
				 GCM_CALIBRATE_UI_INTERACTION);

	/* block for a response */
	g_debug ("blocking waiting for user input");

	/* get the image, if we have one */
	filename = cd_sensor_get_metadata_item (calibrate->priv->sensor,
						CD_SENSOR_METADATA_IMAGE_CALIBRATE);
	gcm_calibrate_set_image (calibrate, filename);
	gcm_calibrate_interaction_required (calibrate, _("Continue"));
	if (filename != NULL) {
		/* TRANSLATORS: this is when the user has to change a setting on the sensor, and we're showing a picture */
		gcm_calibrate_set_message (calibrate,
					   _("Please set the measuring instrument to calibration mode like the image below."),
					   GCM_CALIBRATE_UI_INTERACTION);
	} else {
		/* TRANSLATORS: this is when the user has to change a setting on the sensor */
		gcm_calibrate_set_message (calibrate,
					   _("Please set the measuring instrument to calibration mode."),
					   GCM_CALIBRATE_UI_INTERACTION);
	}

	/* play sound from the naming spec */
	ca_context_play (ca_gtk_context_get (), 0,
			 CA_PROP_EVENT_ID, "dialog-information",
			 /* TRANSLATORS: this is the application name for libcanberra */
			 CA_PROP_APPLICATION_NAME, _("GNOME Color Manager"),
			 CA_PROP_EVENT_DESCRIPTION, "setup calibration tool", NULL);
}

/**
 * gcm_calibrate_set_brightness:
 **/
static void
gcm_calibrate_set_brightness (GcmCalibrate *calibrate, CdDevice *device)
{
	const gchar *xrandr_name;
	gboolean ret;
	GError *error = NULL;

	/* is this a laptop */
	xrandr_name = cd_device_get_metadata_item (device,
						   CD_DEVICE_METADATA_XRANDR_NAME);
	if (xrandr_name == NULL)
		return;
	ret = gcm_utils_output_is_lcd_internal (xrandr_name);
	if (!ret)
		return;

	/* set the brightness to max */
	ret = gcm_brightness_get_percentage (calibrate->priv->brightness,
					     &calibrate->priv->old_brightness,
					     &error);
	if (!ret) {
		g_warning ("failed to get brightness: %s",
			   error->message);
		g_clear_error (&error);
	}
	/* FIXME: allow the user to set this */
	calibrate->priv->new_brightness = 100;
	ret = gcm_brightness_set_percentage (calibrate->priv->brightness,
					     calibrate->priv->new_brightness,
					     &error);
	if (!ret) {
		g_warning ("failed to set brightness: %s",
			   error->message);
		g_error_free (error);
	}
}

/**
 * gcm_calibrate_unset_brightness:
 **/
static void
gcm_calibrate_unset_brightness (GcmCalibrate *calibrate, CdDevice *device)
{
	const gchar *xrandr_name;
	gboolean ret;
	GError *error = NULL;

	/* is this a laptop */
	xrandr_name = cd_device_get_metadata_item (device,
						   CD_DEVICE_METADATA_XRANDR_NAME);
	if (xrandr_name == NULL)
		return;
	ret = gcm_utils_output_is_lcd_internal (xrandr_name);
	if (!ret)
		return;

	/* never set */
	if (calibrate->priv->old_brightness == G_MAXUINT)
		return;

	/* reset the brightness to what it was before */
	ret = gcm_brightness_set_percentage (calibrate->priv->brightness,
					     calibrate->priv->old_brightness,
					     &error);
	if (!ret) {
		g_warning ("failed to set brightness: %s",
			   error->message);
		g_error_free (error);
	}
}

/**
 * gcm_calibrate_display:
 **/
static gboolean
gcm_calibrate_display (GcmCalibrate *calibrate,
		       CdDevice *device,
		       GtkWindow *window,
		       GError **error)
{
	const gchar *filename_tmp;
	gboolean ret = TRUE;
	gchar *cal_fn = NULL;
	gchar *ti1_dest_fn = NULL;
	gchar *ti1_src_fn = NULL;
	gchar *ti3_fn = NULL;
	GcmCalibrateClass *klass = GCM_CALIBRATE_GET_CLASS (calibrate);
	GcmCalibratePrivate *priv = calibrate->priv;

	/* set brightness */
	gcm_calibrate_set_brightness (calibrate, device);

	/* get a ti1 file suitable for the calibration */
	ti1_dest_fn = g_strdup_printf ("%s/%s.ti1",
				       calibrate->priv->working_path,
				       calibrate->priv->basename);

	/* copy a pre-generated file into the working path */
	switch (priv->precision) {
	case GCM_CALIBRATE_PRECISION_SHORT:
		filename_tmp = "display-short.ti1";
		break;
	case GCM_CALIBRATE_PRECISION_NORMAL:
		filename_tmp = "display-normal.ti1";
		break;
	case GCM_CALIBRATE_PRECISION_LONG:
		filename_tmp = "display-long.ti1";
		break;
	default:
		g_assert_not_reached ();
	}
	ti1_src_fn = g_build_filename (GCM_DATA, "ti1", filename_tmp, NULL);
	ret = gcm_calibrate_copy_file (ti1_src_fn, ti1_dest_fn, error);
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

	/* proxy */
	ret = klass->calibrate_display (calibrate, device, priv->sensor, window, error);
	if (!ret)
		goto out;
out:
	/* unset brightness */
	gcm_calibrate_unset_brightness (calibrate, device);
	g_free (cal_fn);
	g_free (ti1_src_fn);
	g_free (ti1_dest_fn);
	g_free (ti3_fn);
	return ret;
}

/**
 * gcm_calibrate_camera_get_reference_image:
 **/
static gchar *
gcm_calibrate_camera_get_reference_image (const gchar *directory, GtkWindow *window)
{
	gchar *filename = NULL;
	GtkWidget *dialog;
	GtkFileFilter *filter;

	/* TRANSLATORS: dialog for file->open dialog. A calibration target image is the
	 * aquired image of the calibration target, e.g. an image file that looks
	 * a bit like this: http://www.colorreference.de/targets/target.jpg */
	dialog = gtk_file_chooser_dialog_new (_("Select calibration target image"), window,
					       GTK_FILE_CHOOSER_ACTION_OPEN,
					       _("_Cancel"), GTK_RESPONSE_CANCEL,
					       _("_Open"), GTK_RESPONSE_ACCEPT,
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
 * gcm_calibrate_camera_get_reference_data:
 **/
static gchar *
gcm_calibrate_camera_get_reference_data (const gchar *directory, GtkWindow *window)
{
	gchar *filename = NULL;
	GtkWidget *dialog;
	GtkFileFilter *filter;

	/* TRANSLATORS: dialog for file->open dialog */
	dialog = gtk_file_chooser_dialog_new (_("Select CIE reference values file"), window,
					       GTK_FILE_CHOOSER_ACTION_OPEN,
					       _("_Cancel"), GTK_RESPONSE_CANCEL,
					       _("_Open"), GTK_RESPONSE_ACCEPT,
					      NULL);
	gtk_window_set_icon_name (GTK_WINDOW (dialog), GCM_STOCK_ICON);
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER(dialog), directory);
	gtk_file_chooser_set_create_folders (GTK_FILE_CHOOSER(dialog), FALSE);

	/* setup the filter */
	filter = gtk_file_filter_new ();
	gtk_file_filter_add_mime_type (filter, "application/x-it87");

	/* we can remove this when we depend on a new shared-mime-info */
	gtk_file_filter_add_pattern (filter, "*.cie");
	gtk_file_filter_add_pattern (filter, "*.CIE");
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
		g_warning ("failed to get contents: %s", error->message);
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
					       _("_Cancel"), GTK_RESPONSE_CANCEL,
					       _("_Open"), GTK_RESPONSE_ACCEPT,
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
static gboolean
gcm_calibrate_printer (GcmCalibrate *calibrate, CdDevice *device, GtkWindow *window, GError **error)
{
	gboolean ret = FALSE;
	gchar *ptr;
	GtkWindow *window_tmp = NULL;
	gchar *precision = NULL;
	const gchar *filename_tmp;
	gchar *ti1_dest_fn = NULL;
	gchar *ti1_src_fn = NULL;
	GcmCalibrateClass *klass = GCM_CALIBRATE_GET_CLASS (calibrate);
	GcmCalibratePrivate *priv = calibrate->priv;

	/* get a ti1 file suitable for the calibration */
	ti1_dest_fn = g_strdup_printf ("%s/%s.ti1",
				    calibrate->priv->working_path,
				    calibrate->priv->basename);

	/* copy a pre-generated file into the working path */
	switch (priv->precision) {
	case GCM_CALIBRATE_PRECISION_SHORT:
		filename_tmp = "printer-short.ti1";
		break;
	case GCM_CALIBRATE_PRECISION_NORMAL:
		filename_tmp = "printer-normal.ti1";
		break;
	case GCM_CALIBRATE_PRECISION_LONG:
		filename_tmp = "printer-long.ti1";
		break;
	default:
		g_assert_not_reached ();
	}
	ti1_src_fn = g_build_filename (GCM_DATA, "ti1", filename_tmp, NULL);
	ret = gcm_calibrate_copy_file (ti1_src_fn, ti1_dest_fn, error);
	if (!ret)
		goto out;

	/* copy */
	if (priv->print_kind == GCM_CALIBRATE_PRINT_KIND_ANALYZE) {

		/* remove previously set value (if any) */
		g_free (priv->working_path);
		priv->working_path = NULL;

		/* get from the user */
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
	ret = klass->calibrate_printer (calibrate, device, priv->sensor, window, error);
out:
	g_free (ti1_src_fn);
	g_free (ti1_dest_fn);
	g_free (precision);
	return ret;
}

/**
 * gcm_calibrate_get_enabled:
 **/
gboolean
gcm_calibrate_get_enabled (GcmCalibrate *calibrate)
{
	GcmCalibrateClass *klass = GCM_CALIBRATE_GET_CLASS (calibrate);
	if (klass->get_enabled != NULL)
		return klass->get_enabled (calibrate);
	return TRUE;
}

/**
 * gcm_calibrate_get_reference_cie:
 **/
static const gchar *
gcm_calibrate_get_reference_cie (GcmCalibrateReferenceKind kind)
{
	if (kind == GCM_CALIBRATE_REFERENCE_KIND_CMP_DIGITAL_TARGET_3)
		return "CMP_Digital_Target-3.cie";
	if (kind == GCM_CALIBRATE_REFERENCE_KIND_COLOR_CHECKER)
		return "ColorChecker.cie";
	if (kind == GCM_CALIBRATE_REFERENCE_KIND_QPCARD_201)
		return "QPcard_201.cie";
	return NULL;
}

/**
 * gcm_calibrate_camera:
 **/
static gboolean
gcm_calibrate_camera (GcmCalibrate *calibrate, CdDevice *device, GtkWindow *window, GError **error)
{
	gboolean ret = FALSE;
	gboolean has_shared_targets = TRUE;
	gchar *reference_image = NULL;
	gchar *reference_data = NULL;
	gchar *device_str = NULL;
	const gchar *directory;
	const gchar *tmp;
	GString *string;
	GtkWindow *window_tmp = NULL;
	gchar *precision = NULL;
	GcmCalibratePrivate *priv = calibrate->priv;
	GcmCalibrateClass *klass = GCM_CALIBRATE_GET_CLASS (calibrate);

	string = g_string_new ("");

	/* get scanned image */
	directory = g_get_home_dir ();
	reference_image = gcm_calibrate_camera_get_reference_image (directory, window_tmp);
	if (reference_image == NULL) {
		g_set_error_literal (error,
				     GCM_CALIBRATE_ERROR,
				     GCM_CALIBRATE_ERROR_USER_ABORT,
				     "could not get calibration target image");
		ret = FALSE;
		goto out;
	}

	/* use the exif data if there is any present */
	ret = gcm_calibrate_set_from_exif (calibrate, reference_image, NULL);
	if (!ret)
		g_debug ("no EXIF data, so using device attributes");

	/* get reference data */
	tmp = gcm_calibrate_get_reference_cie (priv->reference_kind);
	if (tmp != NULL) {
		/* some references don't have custom CIE files available
		 * to them and instead rely on a fixed reference */
		reference_data = g_build_filename ("/usr/share/color/argyll/ref",
						   tmp,
						   NULL);
	} else {
		/* ask the user to find the profile */
		directory = has_shared_targets ? "/usr/share/color/targets" : "/media";
		reference_data = gcm_calibrate_camera_get_reference_data (directory, window_tmp);
		if (reference_data == NULL) {
			g_set_error_literal (error,
					     GCM_CALIBRATE_ERROR,
					     GCM_CALIBRATE_ERROR_USER_ABORT,
					     "could not get reference target");
			ret = FALSE;
			goto out;
		}
	}

	/* use the ORIGINATOR in the it8 file */
	device_str = gcm_calibrate_get_device_for_it8_file (reference_data);
	if (device_str == NULL)
		device_str = g_strdup ("IT8.7");

	/* set the calibration parameters */
	g_object_set (calibrate,
		      "filename-source", reference_image,
		      "filename-reference", reference_data,
		      "device", device,
		      NULL);

	/* coldplug source */
	if (klass->calibrate_device == NULL) {
		g_set_error_literal (error,
				     GCM_CALIBRATE_ERROR,
				     GCM_CALIBRATE_ERROR_INTERNAL,
				     "no klass support");
		goto out;
	}

	/* proxy */
	ret = klass->calibrate_device (calibrate, device, priv->sensor, window, error);
out:
	if (string != NULL)
		g_string_free (string, TRUE);
	g_free (precision);
	g_free (device_str);
	g_free (reference_image);
	g_free (reference_data);
	return ret;
}

/**
 * gcm_calibrate_device:
 **/
gboolean
gcm_calibrate_device (GcmCalibrate *calibrate,
		      CdDevice *device,
		      GtkWindow *window,
		      GError **error)
{
	gboolean ret = TRUE;

	/* set up some initial parameters */
	gcm_calibrate_set_basename (calibrate);
	ret = gcm_calibrate_set_working_path (calibrate, error);
	if (!ret)
		goto out;

	/* mark device to be profiled in colord */
	ret = cd_device_profiling_inhibit_sync (device,
						NULL,
						error);
	if (!ret)
		goto out;

	/* set progress */
	gcm_calibrate_set_progress (calibrate, 0);

	switch (cd_device_get_kind (device)) {
	case CD_DEVICE_KIND_DISPLAY:
		ret = gcm_calibrate_display (calibrate,
					     device,
					     window,
					     error);
		if (!ret) {
			/* we don't care about the success or error */
			cd_device_profiling_uninhibit_sync (device,
							    NULL,
							    NULL);
			goto out;
		}
		break;
	case CD_DEVICE_KIND_PRINTER:
		ret = gcm_calibrate_printer (calibrate,
					     device,
					     window,
					     error);
		if (!ret) {
			/* we don't care about the success or error */
			cd_device_profiling_uninhibit_sync (device,
							    NULL,
							    NULL);
			goto out;
		}
		break;
	default:
		ret = gcm_calibrate_camera (calibrate,
					    device,
					    window,
					    error);
		if (!ret) {
			/* we don't care about the success or error */
			cd_device_profiling_uninhibit_sync (device,
							    NULL,
							    NULL);
			goto out;
		}
		break;
	}

	/* device back to normal */
	ret = cd_device_profiling_uninhibit_sync (device,
						  NULL,
						  error);
	if (!ret)
		goto out;

	/* set progress */
	gcm_calibrate_set_progress (calibrate, 100);
out:
	return ret;
}

void
gcm_calibrate_set_title (GcmCalibrate *calibrate,
			 const gchar *title,
			 GcmCalibrateUiType ui_type)
{
	g_signal_emit (calibrate, signals[SIGNAL_TITLE_CHANGED], 0, title);
	if (ui_type == GCM_CALIBRATE_UI_STATUS && title != NULL) {
		g_free (calibrate->priv->old_title);
		calibrate->priv->old_title = g_strdup (title);
	}
}

void
gcm_calibrate_set_message (GcmCalibrate *calibrate,
			   const gchar *message,
			   GcmCalibrateUiType ui_type)
{
	g_signal_emit (calibrate, signals[SIGNAL_MESSAGE_CHANGED], 0, message);
	if (ui_type == GCM_CALIBRATE_UI_STATUS && message != NULL) {
		g_free (calibrate->priv->old_message);
		calibrate->priv->old_message = g_strdup (message);
	}
}

void
gcm_calibrate_set_image (GcmCalibrate *calibrate,
			 const gchar *filename)
{
	g_signal_emit (calibrate, signals[SIGNAL_IMAGE_CHANGED], 0, filename);
}

void
gcm_calibrate_set_progress (GcmCalibrate *calibrate, guint percentage)
{
	g_signal_emit (calibrate, signals[SIGNAL_PROGRESS_CHANGED], 0, percentage);
}

void
gcm_calibrate_interaction_required (GcmCalibrate *calibrate, const gchar *button_text)
{
	g_signal_emit (calibrate, signals[SIGNAL_INTERACTION_REQUIRED], 0, button_text);
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
	case PROP_DISPLAY_KIND:
		g_value_set_uint (value, priv->display_kind);
		break;
	case PROP_SENSOR_KIND:
		g_assert (priv->sensor != NULL);
		g_value_set_uint (value, cd_sensor_get_kind (priv->sensor));
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
	case PROP_COPYRIGHT:
		g_value_set_string (value, priv->copyright);
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
	case PROP_TARGET_WHITEPOINT:
		g_value_set_uint (value, priv->target_whitepoint);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
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
	case PROP_COPYRIGHT:
		g_free (priv->copyright);
		priv->copyright = g_strdup (g_value_get_string (value));
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
	case PROP_DEVICE_KIND:
		priv->device_kind = g_value_get_uint (value);
		break;
	case PROP_PRECISION:
		priv->precision = g_value_get_uint (value);
		break;
	case PROP_DISPLAY_KIND:
		priv->display_kind = g_value_get_uint (value);
		break;
	case PROP_PRINT_KIND:
		priv->print_kind = g_value_get_uint (value);
		break;
	case PROP_REFERENCE_KIND:
		priv->reference_kind = g_value_get_uint (value);
		break;
	case PROP_WORKING_PATH:
		g_free (priv->working_path);
		priv->working_path = g_strdup (g_value_get_string (value));
		break;
	case PROP_TARGET_WHITEPOINT:
		priv->target_whitepoint = g_value_get_uint (value);
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

	pspec = g_param_spec_uint ("reference-kind", NULL, NULL,
				   0, G_MAXUINT, 0,
				   G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_REFERENCE_KIND, pspec);

	pspec = g_param_spec_uint ("display-kind", NULL, NULL,
				   0, G_MAXUINT, 0,
				   G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_DISPLAY_KIND, pspec);

	pspec = g_param_spec_uint ("print-kind", NULL, NULL,
				   0, G_MAXUINT, 0,
				   G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_PRINT_KIND, pspec);

	pspec = g_param_spec_uint ("device-kind", NULL, NULL,
				   0, G_MAXUINT, 0,
				   G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_DEVICE_KIND, pspec);

	pspec = g_param_spec_uint ("sensor-kind", NULL, NULL,
				   0, G_MAXUINT, 0,
				   G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_SENSOR_KIND, pspec);

	pspec = g_param_spec_string ("output-name", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_OUTPUT_NAME, pspec);

	pspec = g_param_spec_string ("filename-source", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_FILENAME_SOURCE, pspec);

	pspec = g_param_spec_string ("filename-reference", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_FILENAME_REFERENCE, pspec);

	pspec = g_param_spec_string ("filename-result", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_FILENAME_RESULT, pspec);

	pspec = g_param_spec_string ("basename", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_BASENAME, pspec);

	pspec = g_param_spec_string ("copyright", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_COPYRIGHT, pspec);

	pspec = g_param_spec_string ("model", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_MODEL, pspec);

	pspec = g_param_spec_string ("description", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_DESCRIPTION, pspec);

	pspec = g_param_spec_string ("serial", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_SERIAL, pspec);

	pspec = g_param_spec_string ("device", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_DEVICE, pspec);

	pspec = g_param_spec_string ("manufacturer", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_MANUFACTURER, pspec);

	pspec = g_param_spec_string ("working-path", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_WORKING_PATH, pspec);

	pspec = g_param_spec_uint ("precision", NULL, NULL,
				   0, G_MAXUINT, 0,
				   G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_PRECISION, pspec);

	pspec = g_param_spec_uint ("target-whitepoint", NULL, NULL,
				   0, G_MAXUINT, 0,
				   G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_TARGET_WHITEPOINT, pspec);

	signals[SIGNAL_TITLE_CHANGED] =
		g_signal_new ("title-changed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GcmCalibrateClass, title_changed),
			      NULL, NULL, g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);
	signals[SIGNAL_MESSAGE_CHANGED] =
		g_signal_new ("message-changed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GcmCalibrateClass, message_changed),
			      NULL, NULL, g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);
	signals[SIGNAL_IMAGE_CHANGED] =
		g_signal_new ("image-changed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GcmCalibrateClass, image_changed),
			      NULL, NULL, g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);
	signals[SIGNAL_PROGRESS_CHANGED] =
		g_signal_new ("progress-changed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GcmCalibrateClass, progress_changed),
			      NULL, NULL, g_cclosure_marshal_VOID__UINT,
			      G_TYPE_NONE, 1, G_TYPE_UINT);
	signals[SIGNAL_INTERACTION_REQUIRED] =
		g_signal_new ("interaction-required",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GcmCalibrateClass, interaction_required),
			      NULL, NULL, g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);

	g_type_class_add_private (klass, sizeof (GcmCalibratePrivate));
}

/**
 * gcm_calibrate_init:
 **/
static void
gcm_calibrate_init (GcmCalibrate *calibrate)
{
	calibrate->priv = GCM_CALIBRATE_GET_PRIVATE (calibrate);
	calibrate->priv->print_kind = GCM_CALIBRATE_PRINT_KIND_UNKNOWN;
	calibrate->priv->reference_kind = GCM_CALIBRATE_REFERENCE_KIND_UNKNOWN;
	calibrate->priv->precision = GCM_CALIBRATE_PRECISION_UNKNOWN;
	calibrate->priv->sample_window = cd_sample_window_new ();
	calibrate->priv->old_brightness = G_MAXUINT;
	calibrate->priv->brightness = gcm_brightness_new ();

	// FIXME: this has to be per-run specific
	calibrate->priv->working_path = g_strdup ("/tmp");
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
	g_free (calibrate->priv->old_title);
	g_free (calibrate->priv->old_message);
	gtk_widget_destroy (GTK_WIDGET (calibrate->priv->sample_window));
	g_object_unref (priv->brightness);
	if (priv->sensor != NULL)
		g_object_unref (priv->sensor);

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

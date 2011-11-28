/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009-2011 Richard Hughes <richard@hughsie.com>
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
#include <colord.h>
#include <lcms2.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-rr.h>

#include "gcm-calibrate.h"
#include "gcm-utils.h"
#include "gcm-brightness.h"
#include "gcm-exif.h"
#include "gcm-sample-window.h"

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
	guint				 target_whitepoint;
	GtkWidget			*content_widget;
	GtkWindow			*sample_window;
	GPtrArray			*old_message;
	GPtrArray			*old_title;
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

#if 0
	/* can this device support projectors? */
	if (priv->display_kind == GCM_CALIBRATE_DEVICE_KIND_PROJECTOR &&
	    !cd_sensor_has_cap (priv->sensor, CD_SENSOR_CAP_PROJECTOR)) {
		/* TRANSLATORS: title, the hardware calibration device does not support projectors */
		title = _("Could not calibrate and profile using this color measuring instrument");

		/* TRANSLATORS: dialog message */
		message = _("This color measuring instrument is not designed to support calibration and profiling projectors.");
	}
#endif

void
gcm_calibrate_set_sensor (GcmCalibrate *calibrate, CdSensor *sensor)
{
	/* do not refcount */
	calibrate->priv->sensor = sensor;
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

	/* coldplug source */
	if (klass->interaction == NULL)
		return;

	/* proxy */
	klass->interaction (calibrate, response);
}

/**
 * gcm_calibrate_get_sensor_image_attach:
 **/
const gchar *
gcm_calibrate_get_sensor_image_attach (GcmCalibrate *calibrate)
{
	switch (cd_sensor_get_kind (calibrate->priv->sensor)) {
	case CD_SENSOR_KIND_HUEY:
		return "huey-attach.svg";
	case CD_SENSOR_KIND_COLOR_MUNKI:
		return "munki-attach.svg";
	case CD_SENSOR_KIND_SPYDER:
		return "spyder-attach.svg";
	case CD_SENSOR_KIND_COLORIMTRE_HCFR:
		return "hcfr-attach.svg";
	case CD_SENSOR_KIND_I1_PRO:
		return "i1-pro-attach.svg";
	case CD_SENSOR_KIND_DTP94:
		return "dtp94-attach.svg";
#if CD_CHECK_VERSION(0,1,14)
	case CD_SENSOR_KIND_I1_DISPLAY3:
		return "i1-display3-attach.svg";
#endif
	default:
		break;
	}
	return NULL;
}

/**
 * gcm_calibrate_get_sensor_image_calibrate:
 **/
const gchar *
gcm_calibrate_get_sensor_image_calibrate (GcmCalibrate *calibrate)
{
	CdSensorKind sensor_kind;

	sensor_kind = cd_sensor_get_kind (calibrate->priv->sensor);
	if (sensor_kind == CD_SENSOR_KIND_COLOR_MUNKI)
		return "munki-calibrate.svg";
	return NULL;
}

/**
 * gcm_calibrate_get_sensor_image_screen:
 **/
const gchar *
gcm_calibrate_get_sensor_image_screen (GcmCalibrate *calibrate)
{
	CdSensorKind sensor_kind;

	sensor_kind = cd_sensor_get_kind (calibrate->priv->sensor);
	if (sensor_kind == CD_SENSOR_KIND_COLOR_MUNKI)
		return "munki-screen.svg";
	return NULL;
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
 * gcm_calibrate_lcms_error_cb:
 **/
static void
gcm_calibrate_lcms_error_cb (cmsContext ContextID,
			     cmsUInt32Number error_code,
			     const char *text)
{
	g_warning ("LCMS: %s", text);
}

/**
 * gcm_calibrate_delay_cb:
 **/
static gboolean
gcm_calibrate_delay_cb (gpointer user_data)
{
	GMainLoop *loop = (GMainLoop *) user_data;
	g_main_loop_quit (loop);
	return FALSE;
}

/**
 * gcm_calibrate_delay:
 **/
static void
gcm_calibrate_delay (guint ms)
{
	GMainLoop *loop;
	loop = g_main_loop_new (NULL, FALSE);
	g_timeout_add (ms, gcm_calibrate_delay_cb, loop);
	g_main_loop_run (loop);
	g_main_loop_unref (loop);
}

/**
 * gcm_calibrate_interaction_attach_sensor:
 **/
static void
gcm_calibrate_interaction_attach_sensor (GcmCalibrate *calibrate)
{
	const gchar *filename;
	const gchar *message;

	/* TRANSLATORS: title, instrument is a hardware color calibration sensor */
	gcm_calibrate_set_title (calibrate, _("Please attach instrument"));

	/* different messages with or without image */
	filename = gcm_calibrate_get_sensor_image_attach (calibrate);
	if (filename != NULL) {
		/* TRANSLATORS: dialog message, ask user to attach device, and there's an example image */
		message = _("Please attach the measuring instrument to the center of the screen on the gray square like the image below.");
	} else {
		/* TRANSLATORS: dialog message, ask user to attach device */
		message = _("Please attach the measuring instrument to the center of the screen on the gray square.");
	}
	gcm_calibrate_set_message (calibrate, message);
	gcm_calibrate_set_image (calibrate, filename);
	gcm_calibrate_interaction_required (calibrate, _("Continue"));

	//FIXME
	gcm_calibrate_delay (3000);
}

/**
 * gcm_calibrate_get_samples:
 **/
static gboolean
gcm_calibrate_get_samples (GcmCalibrate *calibrate,
			   GPtrArray *samples_rgb,
			   GPtrArray *samples_xyz,
			   GError **error)
{
	CdColorRGB *rgb;
	CdColorRGB rgb_tmp;
	CdColorXYZ *xyz;
	gboolean ret;
	guint i;
	GcmCalibratePrivate *priv = calibrate->priv;

	/* setup the measure window */
	cd_color_set_rgb (&rgb_tmp, 1.0f, 1.0f, 1.0f);
	gcm_sample_window_set_color (GCM_SAMPLE_WINDOW (priv->sample_window), &rgb_tmp);
	gcm_sample_window_set_percentage (GCM_SAMPLE_WINDOW (priv->sample_window), 0);
	gtk_window_set_modal (priv->sample_window, TRUE);
	gtk_window_stick (priv->sample_window);
	gtk_window_present (priv->sample_window);

	/* lock the sensor */
	ret = cd_sensor_lock_sync (calibrate->priv->sensor,
				   NULL,
				   error);
	if (!ret)
		goto out;

	/* capture all the source colors */
	for (i = 0; i < samples_rgb->len; i++) {

		/* get the source color */
		rgb = g_ptr_array_index (samples_rgb, i);
		xyz = g_ptr_array_index (samples_xyz, i);

		g_debug ("Using source color %f,%f,%f",
			 rgb->R, rgb->G, rgb->B);

		/* set the window color */
		gcm_sample_window_set_color (GCM_SAMPLE_WINDOW (priv->sample_window), rgb);
		gcm_sample_window_set_percentage (GCM_SAMPLE_WINDOW (priv->sample_window),
						  100 * i / samples_rgb->len);

		/* wait for the refresh to set the new color */
		if (i == 0 && !priv->sensor_on_screen) {
			gcm_calibrate_interaction_attach_sensor (calibrate);
			priv->sensor_on_screen = TRUE;
		}
		gcm_calibrate_delay (100);

		/* get the sample from the hardware */
		xyz = cd_sensor_get_sample_sync (priv->sensor,
						 CD_SENSOR_CAP_LCD,
						 NULL,
						 error);
		if (xyz == NULL) {
			ret = FALSE;
			goto out;
		}
		g_debug ("sampled %f,%f,%f as %f,%f,%f",
			 rgb->R, rgb->G, rgb->B,
			 xyz->X, xyz->Y, xyz->Z);
		g_ptr_array_add (samples_xyz, xyz);
	}

	/* unlock the sensor */
	ret = cd_sensor_unlock_sync (calibrate->priv->sensor,
				     NULL,
				     error);
	if (!ret)
		goto out;
out:
	/* hide the sample window */
	gtk_widget_hide (GTK_WIDGET (priv->sample_window));
	return ret;
}

/**
 * gcm_calibrate_normalize_to_y:
 **/
static void
gcm_calibrate_normalize_to_y (GPtrArray *samples_xyz, gdouble scale)
{
	CdColorXYZ *xyz;
	guint i;
	gdouble normalize = 0.0f;

	/* first, find largest */
	for (i = 0; i < samples_xyz->len; i++) {
		xyz = g_ptr_array_index (samples_xyz, i);
		if (xyz->Y > normalize)
			normalize = xyz->Y;
	}

	/* scale all the readings to 100 */
	normalize = scale / normalize;
	for (i = 0; i < samples_xyz->len; i++) {
		xyz = g_ptr_array_index (samples_xyz, i);
		xyz->X *= normalize;
		xyz->Y *= normalize;
		xyz->Z *= normalize;
	}
}

/**
 * gcm_calibrate_set_calibration_to_device:
 **/
static gboolean
gcm_calibrate_set_calibration_to_device (const gchar *cal_fn,
					 CdDevice *device,
					 GError **error)
{
	CdColorRGB *rgb;
	cmsHANDLE cal = NULL;
	const gchar *tmp;
	gboolean ret;
	gchar *cal_data = NULL;
	GnomeRRCrtc *crtc;
	GnomeRROutput *output;
	GnomeRRScreen *screen = NULL;
	GPtrArray *samples_rgb = NULL;
	gsize cal_size;
	guint col;
	guint i;
	guint number_of_sets = 0;
	gushort blue[256];
	gushort green[256];
	gushort red[256];

	/* open .cal file */
	g_debug ("loading %s", cal_fn);
	ret = g_file_get_contents (cal_fn, &cal_data, &cal_size, error);
	if (!ret)
		goto out;

	/* load the cal data */
	cal = cmsIT8LoadFromMem (NULL, cal_data, cal_size);
	if (cal == NULL) {
		ret = FALSE;
		g_set_error (error, 1, 0, "Cannot open %s", cal_fn);
		goto out;
	}

	/* check color format */
	tmp = cmsIT8GetProperty (cal, "COLOR_REP");
	if (g_strcmp0 (tmp, "RGB") != 0) {
		ret = FALSE;
		g_set_error (error, 1, 0, "Invalid format: %s", tmp);
		goto out;
	}
	number_of_sets = cmsIT8GetPropertyDbl (cal, "NUMBER_OF_SETS");
	if (number_of_sets != 256) {
		ret = FALSE;
		g_set_error (error, 1, 0, "Invalid cal size: %i", number_of_sets);
		goto out;
	}

	/* get data */
	samples_rgb = g_ptr_array_new_with_free_func ((GDestroyNotify) cd_color_rgb_free);
	for (i = 0; i < number_of_sets; i++) {
		rgb = cd_color_rgb_new ();
		col = cmsIT8FindDataFormat(cal, "RGB_R");
		rgb->R = cmsIT8GetDataRowColDbl(cal, i, col) / 100.0f;
		col = cmsIT8FindDataFormat(cal, "RGB_G");
		rgb->G = cmsIT8GetDataRowColDbl(cal, i, col) / 100.0f;
		col = cmsIT8FindDataFormat(cal, "RGB_B");
		rgb->B = cmsIT8GetDataRowColDbl(cal, i, col) / 100.0f;
		g_ptr_array_add (samples_rgb, rgb);
	}

	/* send it to the screen */
	screen = gnome_rr_screen_new (gdk_screen_get_default (), error);
	if (screen == NULL) {
		ret = FALSE;
		goto out;
	}
	output = gnome_rr_screen_get_output_by_name (screen, "LVDS1");
	if (output == NULL) {
		ret = FALSE;
		g_set_error (error, 1, 0,
			     "No display with name %s",
			     "LVDS1");
	}
	crtc = gnome_rr_output_get_crtc (output);
	if (crtc == NULL) {
		ret = FALSE;
		g_set_error (error, 1, 0,
			     "No crtc with for output %s",
			     gnome_rr_output_get_name (output));
	}
	for (i = 0; i < samples_rgb->len; i++) {
		rgb = g_ptr_array_index (samples_rgb, i);
		red[0] = rgb->R * 0xffff;
		green[0] = rgb->G * 0xffff;
		blue[0] = rgb->B * 0xffff;
	}
	gnome_rr_crtc_set_gamma (crtc,
				 samples_rgb->len,
				 red,
				 green,
				 blue);
out:
	g_free (cal_data);
	if (cal != NULL)
		cmsIT8Free (cal);
	if (samples_rgb != NULL)
		g_ptr_array_unref (samples_rgb);
	if (screen != NULL)
		g_object_unref (screen);
	return ret;
}

/**
 * gcm_calibrate_display_characterize:
 **/
gboolean
gcm_calibrate_display_characterize (GcmCalibrate *calibrate,
				    const gchar *ti1_fn,
				    const gchar *cal_fn,
				    const gchar *ti3_fn,
				    CdDevice *device,
				    GtkWindow *window,
				    GError **error)
{
	CdColorRGB *rgb;
	CdColorXYZ *xyz;
	cmsHANDLE ti1 = NULL;
	cmsHANDLE ti3 = NULL;
	const gchar *tmp;
	gboolean is_spectral;
	gboolean ret;
	gchar *data_cal = NULL;
	gchar *data_patches = NULL;
	gchar *found_lcms2_bodge;
	gchar *ti1_data = NULL;
	GPtrArray *samples_rgb = NULL;
	GPtrArray *samples_xyz = NULL;
	gsize ti1_size;
	GString *string = NULL;
	guint col;
	guint i;
	guint number_of_sets = 0;
	guint table_count;
	GcmCalibratePrivate *priv = calibrate->priv;

	/* need to set cal to the gamma ramps */
	ret = gcm_calibrate_set_calibration_to_device (cal_fn,
						       device,
						       error);
	if (!ret)
		goto out;

	/* open ti1 file as input */
	g_debug ("loading %s", ti1_fn);
	ret = g_file_get_contents (ti1_fn, &ti1_data, &ti1_size, error);
	if (!ret)
		goto out;

	/* hack to fix lcms2 */
	found_lcms2_bodge = g_strstr_len (ti1_data, ti1_size, "END_DATA\n");
	if (found_lcms2_bodge != NULL)
		found_lcms2_bodge[9] = '\0';

	/* load the ti1 data */
	cmsSetLogErrorHandler (gcm_calibrate_lcms_error_cb);
	ti1 = cmsIT8LoadFromMem (NULL, ti1_data, ti1_size);
	if (ti1 == NULL) {
		ret = FALSE;
		g_set_error (error, 1, 0, "Cannot open %s", ti1_fn);
		goto out;
	}

	/* select correct sheet */
	table_count = cmsIT8TableCount (ti1);
	g_debug ("selecting sheet %i of %i (%s)",
		 0, table_count,
		 cmsIT8GetSheetType (ti1));
	cmsIT8SetTable (ti1, 0);

	/* check color format */
	tmp = cmsIT8GetProperty (ti1, "COLOR_REP");
	if (g_strcmp0 (tmp, "RGB") != 0) {
		ret = FALSE;
		g_set_error (error, 1, 0, "Invalid format: %s", tmp);
		goto out;
	}
	number_of_sets = cmsIT8GetPropertyDbl (ti1, "NUMBER_OF_SETS");

	/* write to a ti3 file as output */
	ti3 = cmsIT8Alloc (NULL);
	cmsIT8SetSheetType (ti3, "CTI3");
	cmsIT8SetPropertyStr (ti3, "DESCRIPTOR",
			      "Calibration Target chart information 3");
	cmsIT8SetPropertyStr (ti3, "ORIGINATOR",
			      "GNOME Color Manager");
	cmsIT8SetPropertyStr (ti3, "DEVICE_CLASS",
			      "DISPLAY");
	cmsIT8SetPropertyStr (ti3, "COLOR_REP",
			      "RGB_XYZ");
	cmsIT8SetPropertyStr (ti3, "TARGET_INSTRUMENT",
			      cd_sensor_get_model (priv->sensor));
	is_spectral = cd_sensor_has_cap (priv->sensor,
					 CD_SENSOR_CAP_SPOT);
	cmsIT8SetPropertyStr (ti3, "INSTRUMENT_TYPE_SPECTRAL",
			      is_spectral ? "YES" : "NO");
	cmsIT8SetPropertyStr (ti3, "LUMINANCE_XYZ_CDM2",
			      "132.922451 129.524179 165.093861");
	cmsIT8SetPropertyStr (ti3, "NORMALIZED_TO_Y_100", "YES");
	cmsIT8SetPropertyDbl (ti3, "NUMBER_OF_FIELDS", 7);
	cmsIT8SetPropertyDbl (ti3, "NUMBER_OF_SETS", number_of_sets);
	cmsIT8SetDataFormat (ti3, 0, "SAMPLE_ID");
	cmsIT8SetDataFormat (ti3, 1, "RGB_R");
	cmsIT8SetDataFormat (ti3, 2, "RGB_G");
	cmsIT8SetDataFormat (ti3, 3, "RGB_B");
	cmsIT8SetDataFormat (ti3, 4, "XYZ_X");
	cmsIT8SetDataFormat (ti3, 5, "XYZ_Y");
	cmsIT8SetDataFormat (ti3, 6, "XYZ_Z");

	/* work out what colors to show */
	samples_rgb = g_ptr_array_new_with_free_func ((GDestroyNotify) cd_color_rgb_free);
	for (i = 0; i < number_of_sets; i++) {
		rgb = cd_color_rgb_new ();
		col = cmsIT8FindDataFormat(ti1, "RGB_R");
		rgb->R = cmsIT8GetDataRowColDbl(ti1, i, col) / 100.0f;
		col = cmsIT8FindDataFormat(ti1, "RGB_G");
		rgb->G = cmsIT8GetDataRowColDbl(ti1, i, col) / 100.0f;
		col = cmsIT8FindDataFormat(ti1, "RGB_B");
		rgb->B = cmsIT8GetDataRowColDbl(ti1, i, col) / 100.0f;
		g_ptr_array_add (samples_rgb, rgb);
	}

	/* actually get samples from the hardware */
	samples_xyz = g_ptr_array_new_with_free_func ((GDestroyNotify) cd_color_xyz_free);
	ret = gcm_calibrate_get_samples (calibrate,
					 samples_rgb,
					 samples_xyz,
					 error);
	if (!ret)
		goto out;

	/* normalize */
	gcm_calibrate_normalize_to_y (samples_xyz, 100.0f);

	/* write to the ti3 file */
	for (i = 0; i < samples_rgb->len; i++) {
		rgb = g_ptr_array_index (samples_rgb, i);
		xyz = g_ptr_array_index (samples_xyz, i);
		cmsIT8SetDataRowColDbl(ti3, i, 0, i + 1);
		cmsIT8SetDataRowColDbl(ti3, i, 1, rgb->R * 100.0f);
		cmsIT8SetDataRowColDbl(ti3, i, 2, rgb->G * 100.0f);
		cmsIT8SetDataRowColDbl(ti3, i, 3, rgb->B * 100.0f);
		cmsIT8SetDataRowColDbl(ti3, i, 4, xyz->X);
		cmsIT8SetDataRowColDbl(ti3, i, 5, xyz->Y);
		cmsIT8SetDataRowColDbl(ti3, i, 6, xyz->Z);
	}

	/* write the file */
	ret = cmsIT8SaveToFile (ti3, ti3_fn);
	if (!ret)
		g_assert_not_reached ();

	/* get the patches data */
	ret = g_file_get_contents (ti3_fn, &data_patches, NULL, error);
	if (!ret)
		goto out;

	/* get the cal data */
	ret = g_file_get_contents (cal_fn, &data_cal, NULL, error);
	if (!ret)
		goto out;

	/* write new ti3 file with cal file appended */
	string = g_string_new ("");
	g_string_append (string, data_patches);
	g_string_append (string, data_cal);
	ret = g_file_set_contents (ti3_fn, string->str, -1, error);
	if (!ret)
		goto out;
out:
	if (ti1 != NULL)
		cmsIT8Free (ti1);
	if (ti3 != NULL)
		cmsIT8Free (ti3);
	if (samples_rgb != NULL)
		g_ptr_array_unref (samples_rgb);
	if (samples_xyz != NULL)
		g_ptr_array_unref (samples_xyz);
	if (string != NULL)
		g_string_free (string, TRUE);
	g_free (ti1_data);
	g_free (data_patches);
	g_free (data_cal);
	return ret;
}

/**
 * gcm_calibrate_calculate_native_gamma:
 **/
static gboolean
gcm_calibrate_calculate_native_gamma (GPtrArray *samples_xyz,
				      gdouble *native_gamma,
				      GError **error)
{
	CdColorXYZ *xyz;
	cmsFloat32Number values[256];
	cmsToneCurve *curve;
	gboolean ret = TRUE;
	gdouble largest = G_MINDOUBLE;
	gdouble smallest = G_MAXDOUBLE;
	gdouble tmp;
	guint i;

	/* scale from 0.0 to 1.0 */
	for (i = 0; i < samples_xyz->len; i++) {
		xyz = g_ptr_array_index (samples_xyz, i);
		if (xyz->Y < smallest)
			smallest = xyz->Y;
		if (xyz->Y > largest)
			largest = xyz->Y;
	}
	for (i = 0; i < samples_xyz->len; i++) {
		xyz = g_ptr_array_index (samples_xyz, i);
		values[i] = (xyz->Y - smallest) * 1.0f / (largest - smallest);
	}

	/* find estimate */
	curve = cmsBuildTabulatedToneCurveFloat (NULL,
						 samples_xyz->len,
						 values);
	tmp = cmsEstimateGamma (curve, 1000.1f);
	if (tmp < 0) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0,
				     "Failed to calculate native gamma");
		goto out;
	}

	/* success */
	ret = TRUE;
	if (native_gamma != NULL)
		*native_gamma = tmp;
out:
	cmsFreeToneCurve (curve);
	return ret;
}

/**
 * gcm_calibrate_convert_xyz_to_rgb:
 **/
static gboolean
gcm_calibrate_convert_xyz_to_rgb (GPtrArray *samples_xyz,
				  GPtrArray *samples_primaries,
				  GPtrArray *results_rgb,
				  GError **error)
{
	CdColorRGB *rgb;
	CdColorXYZ *xyz;
	cmsCIExyYTRIPLE primaries;
	cmsCIExyY whitepoint;
	cmsHPROFILE conversion_profile = NULL;
	cmsHPROFILE xyz_profile = NULL;
	cmsHTRANSFORM transform = NULL;
	cmsToneCurve *transfer_curve[3] = { NULL, NULL, NULL };
	gdouble target_gamma;
	gboolean ret = TRUE;
	gdouble temperature;
	guint i;

	/* hardcode the gamma */
	target_gamma = 2.4f;

	/* calculate the native whitepoint */
	xyz = g_ptr_array_index (samples_primaries, 3);
	cmsXYZ2xyY (&whitepoint, (cmsCIEXYZ*) xyz);
	cmsTempFromWhitePoint (&temperature, &whitepoint);
	g_debug ("native whitepoint=%f,%f [scale:%f] (%fK)",
		 whitepoint.x, whitepoint.y, whitepoint.Y, temperature);

	/* get the primaries */
	xyz = g_ptr_array_index (samples_primaries, 0);
	cmsXYZ2xyY (&primaries.Red, (cmsCIEXYZ*) xyz);
	xyz = g_ptr_array_index (samples_primaries, 1);
	cmsXYZ2xyY (&primaries.Green, (cmsCIEXYZ*) xyz);
	xyz = g_ptr_array_index (samples_primaries, 2);
	cmsXYZ2xyY (&primaries.Blue, (cmsCIEXYZ*) xyz);
	g_debug ("red=%f,%f green=%f,%f blue=%f,%f",
		 primaries.Red.x, primaries.Red.y,
		 primaries.Green.x, primaries.Green.y,
		 primaries.Blue.x, primaries.Blue.y);

	/* setup transform */
	transfer_curve[0] = transfer_curve[1] = transfer_curve[2] = cmsBuildGamma (NULL, target_gamma);
	conversion_profile = cmsCreateRGBProfile (&whitepoint,
						  &primaries,
						  transfer_curve);
	if (conversion_profile == NULL) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0,
				     "Failed to create conversion profile");
		goto out;
	}
	xyz_profile = cmsCreateXYZProfile ();
	transform = cmsCreateTransform (xyz_profile, TYPE_XYZ_DBL,
					conversion_profile, TYPE_RGB_DBL,
					INTENT_RELATIVE_COLORIMETRIC, 0);
	if (transform == NULL) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0,
				     "Failed to create transform");
		goto out;
	}

	/* convert samples */
	for (i = 0; i < samples_xyz->len; i++) {
		xyz = g_ptr_array_index (samples_xyz, i);
		rgb = cd_color_rgb_new ();
		cmsDoTransform (transform, xyz, rgb, 1);
		g_debug ("%f,%f,%f -> %f,%f,%f",
			 xyz->X, xyz->Y, xyz->Z,
			 rgb->R, rgb->G, rgb->B);
		g_ptr_array_add (results_rgb, rgb);
	}

	/* done */
out:
	cmsFreeToneCurve (*transfer_curve);
	cmsCloseProfile (xyz_profile);
	cmsCloseProfile (conversion_profile);
	if (transform != NULL)
		cmsDeleteTransform (transform);
	return ret;
}

/**
 * gcm_calibrate_get_primaries:
 **/
static gboolean
gcm_calibrate_get_primaries (GcmCalibrate *calibrate,
			     GPtrArray *samples_primaries,
			     GError **error)
{
	CdColorRGB *rgb;
	gboolean ret;
	GPtrArray *samples_rgb = NULL;

	/* add primaries to array */
	samples_rgb = g_ptr_array_new_with_free_func ((GDestroyNotify) cd_color_rgb_free);

	/* red */
	rgb = cd_color_rgb_new ();
	cd_color_set_rgb (rgb, 1.0f, 0.0f, 0.0f);
	g_ptr_array_add (samples_rgb, rgb);

	/* green */
	rgb = cd_color_rgb_new ();
	cd_color_set_rgb (rgb, 0.0f, 1.0f, 0.0f);
	g_ptr_array_add (samples_rgb, rgb);

	/* blue */
	rgb = cd_color_rgb_new ();
	cd_color_set_rgb (rgb, 0.0f, 0.0f, 1.0f);
	g_ptr_array_add (samples_rgb, rgb);

	/* white */
	rgb = cd_color_rgb_new ();
	cd_color_set_rgb (rgb, 1.0f, 1.0f, 1.0f);
	g_ptr_array_add (samples_rgb, rgb);

	/* black */
	rgb = cd_color_rgb_new ();
	cd_color_set_rgb (rgb, 0.0f, 0.0f, 0.0f);
	g_ptr_array_add (samples_rgb, rgb);

	/* measure */
	ret = gcm_calibrate_get_samples (calibrate,
					 samples_rgb,
					 samples_primaries,
					 error);
	if (!ret)
		goto out;
out:
	if (samples_rgb != NULL)
		g_ptr_array_unref (samples_rgb);
	return ret;
}

/**
 * gcm_calibrate_resize_results:
 *
 * Linearly interpolate an array of data into a differently sized array.
 **/
static void
gcm_calibrate_resize_results (GPtrArray *src_array,
			      GPtrArray *dest_array,
			      guint new_size)
{
	gdouble amount;
	gdouble div;
	guint i;
	guint reg1, reg2;
	CdColorRGB *dest;
	CdColorRGB *src1;
	CdColorRGB *src2;

	/* gcm_calibrate_native_ */
	for (i = 0; i < new_size; i++) {
		div = i * ((src_array->len - 1) / (gdouble) (new_size - 1));
		reg1 = floor (div);
		reg2 = ceil (div);
		dest = cd_color_rgb_new ();
		if (reg1 == reg2) {
			/* no interpolation reqd */
			src1 = g_ptr_array_index (src_array, reg1);
			cd_color_copy_rgb (src1, dest);
		} else {
			amount = div - (gdouble) reg1;
			src1 = g_ptr_array_index (src_array, reg1);
			src2 = g_ptr_array_index (src_array, reg2);
			dest->R = src1->R * (1.0f - amount) + src2->R * amount;
			dest->G = src1->G * (1.0f - amount) + src2->G * amount;
			dest->B = src1->B * (1.0f - amount) + src2->B * amount;
		}
		g_ptr_array_add (dest_array, dest);
	}
}

/**
 * gcm_calibrate_array_remove_offset:
 **/
static void
gcm_calibrate_array_remove_offset (GPtrArray *array)
{
	CdColorRGB offset;
	CdColorRGB *rgb;
	guint i;

	/* remove the backlight leakage */
	cd_color_set_rgb (&offset,
			  G_MAXDOUBLE,
			  G_MAXDOUBLE,
			  G_MAXDOUBLE);
	for (i = 0; i < array->len; i++) {
		rgb = g_ptr_array_index (array, i);
		if (rgb->R < offset.R)
			offset.R = rgb->R;
		if (rgb->G < offset.G)
			offset.G = rgb->G;
		if (rgb->B < offset.B)
			offset.B = rgb->B;
	}
	for (i = 0; i < array->len; i++) {
		rgb = g_ptr_array_index (array, i);
		rgb->R -= offset.R;
		rgb->G -= offset.G;
		rgb->B -= offset.B;
		if (rgb->R < 0.0f)
			rgb->R = 0.0f;
		if (rgb->G < 0.0f)
			rgb->G = 0.0f;
		if (rgb->B < 0.0f)
			rgb->B = 0.0f;
	}
	g_debug ("removed offset = %f,%f,%f",
		 offset.R, offset.G, offset.B);
}

/**
 * gcm_calibrate_array_scale:
 **/
static void
gcm_calibrate_array_scale (GPtrArray *array, gdouble value)
{
	CdColorRGB *rgb;
	CdColorRGB scale;
	guint i;

	/* scale all values */
	cd_color_set_rgb (&scale,
			  G_MINDOUBLE,
			  G_MINDOUBLE,
			  G_MINDOUBLE);
	for (i = 0; i < array->len; i++) {
		rgb = g_ptr_array_index (array, i);
		if (rgb->R > scale.R)
			scale.R = rgb->R;
		if (rgb->G > scale.G)
			scale.G = rgb->G;
		if (rgb->B > scale.B)
			scale.B = rgb->B;
	}
	for (i = 0; i < array->len; i++) {
		rgb = g_ptr_array_index (array, i);
		rgb->R /= scale.R;
		rgb->G /= scale.G;
		rgb->B /= scale.B;
	}
	g_debug ("scaled to 1.0 using = %f,%f,%f",
		 scale.R, scale.G, scale.B);
}

/**
 * gcm_calibrate_display_calibration:
 **/
gboolean
gcm_calibrate_display_calibration (GcmCalibrate *calibrate,
				   const gchar *cal_fn,
				   CdDevice *device,
				   GtkWindow *window,
				   GError **error)
{
	CdColorRGB *rgb;
	CdColorXYZ *xyz_black;
	CdColorXYZ *xyz_white;
	cmsHANDLE cal = NULL;
	gboolean ret;
	gdouble frac = 0.0f;
	gdouble native_gamma = 0.0f;
	GPtrArray *results_rgb = NULL;
	GPtrArray *results_vcgt = NULL;
	GPtrArray *samples_rgb = NULL;
	GPtrArray *samples_primaries = NULL;
	GPtrArray *samples_xyz = NULL;
	guint i;
	guint vcgt_size = 32;

	/* this is global, ick */
	cmsSetLogErrorHandler (gcm_calibrate_lcms_error_cb);

	/* get the primaries */
	samples_primaries = g_ptr_array_new_with_free_func ((GDestroyNotify) cd_color_yxy_free);
	ret = gcm_calibrate_get_primaries (calibrate,
					   samples_primaries,
					   error);
	if (!ret)
		goto out;

	/* check this was sane */
	xyz_white = g_ptr_array_index (samples_primaries, 3);
	xyz_black = g_ptr_array_index (samples_primaries, 4);
	if (xyz_white->X / xyz_black->X < 3) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0,
				     "Invalid read, is sensor attached to the screen?");
		goto out;
	}

	/* get the gamma values */
	samples_rgb = g_ptr_array_new_with_free_func ((GDestroyNotify) cd_color_rgb_free);
	for (i = 0; i < vcgt_size; i++) {
		rgb = cd_color_rgb_new ();
		frac = (1.0f * (gdouble) i) / (gdouble) (vcgt_size - 1);
		rgb->R = frac;
		rgb->G = frac;
		rgb->B = frac;
		g_ptr_array_add (samples_rgb, rgb);
	}

	/* actually get samples from the hardware */
	samples_xyz = g_ptr_array_new_with_free_func ((GDestroyNotify) cd_color_xyz_free);
	ret = gcm_calibrate_get_samples (calibrate,
					 samples_rgb,
					 samples_xyz,
					 error);
	if (!ret)
		goto out;

	/* calculate the native gamma */
	ret = gcm_calibrate_calculate_native_gamma (samples_xyz,
						    &native_gamma,
						    error);
	if (!ret)
		goto out;
	g_debug ("Native Gamma: %f", native_gamma);

	/* convert from XYZ to RGB */
	results_rgb = g_ptr_array_new_with_free_func ((GDestroyNotify) cd_color_rgb_free);
	ret = gcm_calibrate_convert_xyz_to_rgb (samples_xyz,
						samples_primaries,
						results_rgb,
						error);
	if (!ret)
		goto out;

	/* bias the values to zero */
	gcm_calibrate_array_remove_offset (results_rgb);

	/* scale the largest number to 1.0 */
	gcm_calibrate_array_scale (results_rgb, 1.0f);

	/* interpolate new array with correct size */
	results_vcgt = g_ptr_array_new_with_free_func ((GDestroyNotify) cd_color_rgb_free);
	gcm_calibrate_resize_results (results_rgb, results_vcgt, 256);

	/* write to a cal file as calibration */
	cal = cmsIT8Alloc (NULL);
	cmsIT8SetSheetType (cal, "CAL");
	cmsIT8SetPropertyStr (cal, "DESCRIPTOR",
			      "Device Calibration State");
	cmsIT8SetPropertyStr (cal, "ORIGINATOR",
			      "GNOME Color Manager");
	cmsIT8SetPropertyStr (cal, "DEVICE_CLASS",
			      "DISPLAY");
	cmsIT8SetPropertyStr (cal, "COLOR_REP",
			      "RGB");
	cmsIT8SetPropertyDbl (cal, "NUMBER_OF_FIELDS", 4);
	cmsIT8SetPropertyDbl (cal, "NUMBER_OF_SETS", results_vcgt->len);
	cmsIT8SetDataFormat (cal, 0, "RGB_I");
	cmsIT8SetDataFormat (cal, 1, "RGB_R");
	cmsIT8SetDataFormat (cal, 2, "RGB_G");
	cmsIT8SetDataFormat (cal, 3, "RGB_B");
	for (i = 0; i < results_vcgt->len; i++) {
		frac = (1.0f * (gdouble) i) / (gdouble) (results_vcgt->len - 1);
		rgb = g_ptr_array_index (results_vcgt, i);
		cmsIT8SetDataRowColDbl(cal, i, 0, frac);
		cmsIT8SetDataRowColDbl(cal, i, 1, rgb->R);
		cmsIT8SetDataRowColDbl(cal, i, 2, rgb->G);
		cmsIT8SetDataRowColDbl(cal, i, 3, rgb->B);
	}

	/* write the file */
	ret = cmsIT8SaveToFile (cal, cal_fn);
	if (!ret)
		g_assert_not_reached ();
out:
	if (cal != NULL)
		cmsIT8Free (cal);
	if (results_rgb != NULL)
		g_ptr_array_unref (results_rgb);
	if (results_vcgt != NULL)
		g_ptr_array_unref (results_vcgt);
	if (samples_rgb != NULL)
		g_ptr_array_unref (samples_rgb);
	if (samples_primaries != NULL)
		g_ptr_array_unref (samples_primaries);
	if (samples_xyz != NULL)
		g_ptr_array_unref (samples_xyz);
	return ret;
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
	ret = gcm_brightness_set_percentage (calibrate->priv->brightness,
					     100,
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

	/* if sensor is native, then take some measurements */
	if (cd_sensor_get_native (calibrate->priv->sensor) &&
	    cd_sensor_get_kind (calibrate->priv->sensor) == CD_SENSOR_KIND_COLORHUG) {
		cal_fn = g_strdup_printf ("%s/%s.cal",
					  priv->working_path,
					  priv->basename);
		ret = gcm_calibrate_display_calibration (calibrate,
							 cal_fn,
							 device,
							 window,
							 error);
		if (!ret)
			goto out;
		ti3_fn = g_strdup_printf ("%s/%s.ti3",
					  priv->working_path,
					  priv->basename);
		ret = gcm_calibrate_display_characterize (calibrate,
							  ti1_dest_fn,
							  cal_fn,
							  ti3_fn,
							  device,
							  window,
							  error);
		goto out;
	}

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
	g_object_get (NULL, "print-kind", &priv->print_kind, NULL);

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
		if (!ret)
			goto out;
		break;
	case CD_DEVICE_KIND_PRINTER:
		ret = gcm_calibrate_printer (calibrate,
					     device,
					     window,
					     error);
		if (!ret)
			goto out;
		break;
	default:
		ret = gcm_calibrate_camera (calibrate,
					    device,
					    window,
					    error);
		if (!ret)
			goto out;
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
gcm_calibrate_set_title (GcmCalibrate *calibrate, const gchar *title)
{
	g_signal_emit (calibrate, signals[SIGNAL_TITLE_CHANGED], 0, title);
	g_ptr_array_add (calibrate->priv->old_title, g_strdup (title));
}

void
gcm_calibrate_set_message (GcmCalibrate *calibrate, const gchar *message)
{
	g_signal_emit (calibrate, signals[SIGNAL_MESSAGE_CHANGED], 0, message);
	g_ptr_array_add (calibrate->priv->old_message, g_strdup (message));
}

void
gcm_calibrate_set_image (GcmCalibrate *calibrate, const gchar *filename)
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

void
gcm_calibrate_pop (GcmCalibrate *calibrate)
{
	const gchar *tmp;
	tmp = g_ptr_array_index (calibrate->priv->old_title, calibrate->priv->old_title->len - 2);
	g_signal_emit (calibrate, signals[SIGNAL_TITLE_CHANGED], 0, tmp);
	tmp = g_ptr_array_index (calibrate->priv->old_message, calibrate->priv->old_message->len - 2);
	g_signal_emit (calibrate, signals[SIGNAL_MESSAGE_CHANGED], 0, tmp);
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
				   G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_REFERENCE_KIND, pspec);

	pspec = g_param_spec_uint ("display-kind", NULL, NULL,
				   0, G_MAXUINT, 0,
				   G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_DISPLAY_KIND, pspec);

	pspec = g_param_spec_uint ("print-kind", NULL, NULL,
				   0, G_MAXUINT, 0,
				   G_PARAM_READABLE);
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
	calibrate->priv->sample_window = gcm_sample_window_new ();
	calibrate->priv->old_brightness = G_MAXUINT;
	calibrate->priv->brightness = gcm_brightness_new ();

	// FIXME: this has to be per-run specific
	calibrate->priv->working_path = g_strdup ("/tmp");
	calibrate->priv->old_title = g_ptr_array_new_with_free_func (g_free);
	calibrate->priv->old_message = g_ptr_array_new_with_free_func (g_free);
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
	g_ptr_array_unref (calibrate->priv->old_title);
	g_ptr_array_unref (calibrate->priv->old_message);
	gtk_widget_destroy (GTK_WIDGET (calibrate->priv->sample_window));
	g_object_unref (priv->brightness);

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

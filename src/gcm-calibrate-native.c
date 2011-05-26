/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009-2010 Richard Hughes <richard@hughsie.com>
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
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <lcms2.h>
#include <math.h>
#include <colord.h>

#include "gcm-calibrate-native.h"
#include "gcm-calibrate-dialog.h"
#include "gcm-sample-window.h"
#include "gcm-profile.h"

static void     gcm_calibrate_native_finalize	(GObject     *object);

#define GCM_CALIBRATE_NATIVE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GCM_TYPE_CALIBRATE_NATIVE, GcmCalibrateNativePrivate))

/**
 * GcmCalibrateNativePrivate:
 *
 * Private #GcmCalibrateNative data
 **/
struct _GcmCalibrateNativePrivate
{
	GtkWindow			*sample_window;
	GMainLoop			*loop;
	GCancellable			*cancellable;
	GcmCalibrateDialog		*calibrate_dialog;
};

G_DEFINE_TYPE (GcmCalibrateNative, gcm_calibrate_native, GCM_TYPE_CALIBRATE)

typedef struct {
	gchar		*patch_name;
	CdColorRGB	*source;
	CdColorXYZ	*result;
} GcmColorSample;

/**
 * gcm_calibrate_native_sample_free_cb:
 **/
static void
gcm_calibrate_native_sample_free_cb (GcmColorSample *sample)
{
	g_free (sample->patch_name);
	g_free (sample->source);
	g_free (sample->result);
	g_free (sample);
}

/**
 * gcm_calibrate_native_sample_measure:
 **/
static void
gcm_calibrate_native_sample_measure (CdSensor *sensor, GCancellable *cancellable, GcmColorSample *sample)
{
	GError *error = NULL;
	gboolean ret;

	ret = cd_sensor_get_sample_sync (sensor,
					 CD_SENSOR_CAP_LCD,
					 sample->result,
					 NULL,
					 cancellable,
					 &error);
	g_assert_no_error (error);
	g_assert (ret);
}

/**
 * gcm_calibrate_native_sample_add:
 **/
static void
gcm_calibrate_native_sample_add (GPtrArray *array, const gchar *patch_name, const CdColorRGB *source)
{
	GcmColorSample *sample;

	sample = g_new0 (GcmColorSample, 1);
	sample->patch_name = g_strdup (patch_name);
	sample->source = g_new0 (CdColorRGB, 1);
	sample->result = g_new0 (CdColorXYZ, 1);
	cd_color_copy_rgb (source, sample->source);
	g_ptr_array_add (array, sample);
}

/**
 * gcm_calibrate_native_sample_write:
 **/
static void
gcm_calibrate_native_sample_write (GPtrArray *array, const gchar *filename)
{
	guint i;
	gboolean ret;
	cmsHANDLE it8;
	GcmColorSample *sample;

	it8 = cmsIT8Alloc (NULL);
	cmsIT8SetSheetType (it8, "LCMS/MEASUREMENT");
	cmsIT8SetPropertyStr (it8, "ORIGINATOR", "LPROF");
	cmsIT8SetPropertyStr (it8, "DESCRIPTOR", "measurement sheet");
	cmsIT8SetPropertyStr (it8, "MANUFACTURER", "GNOME Color Manager");
	cmsIT8SetPropertyStr (it8, "CREATED", "Today...");
	cmsIT8SetPropertyStr (it8, "SERIAL", "123456789");
	cmsIT8SetPropertyStr (it8, "MATERIAL", "");

	cmsIT8SetPropertyDbl (it8, "NUMBER_OF_FIELDS", 7);
	cmsIT8SetPropertyDbl (it8, "NUMBER_OF_SETS", array->len);
	cmsIT8SetDataFormat (it8, 0, "SAMPLE_ID");
	cmsIT8SetDataFormat (it8, 1, "RGB_R");
	cmsIT8SetDataFormat (it8, 2, "RGB_G");
	cmsIT8SetDataFormat (it8, 3, "RGB_B");
	cmsIT8SetDataFormat (it8, 4, "XYZ_X");
	cmsIT8SetDataFormat (it8, 5, "XYZ_Y");
	cmsIT8SetDataFormat (it8, 6, "XYZ_Z");

	for (i=0; i<array->len; i++) {
		sample = g_ptr_array_index (array, i);
		cmsIT8SetData (it8, sample->patch_name, "SAMPLE_ID", sample->patch_name);
		cmsIT8SetDataDbl (it8, sample->patch_name, "RGB_R", sample->source->R);
		cmsIT8SetDataDbl (it8, sample->patch_name, "RGB_G", sample->source->G);
		cmsIT8SetDataDbl (it8, sample->patch_name, "RGB_B", sample->source->B);
		cmsIT8SetDataDbl (it8, sample->patch_name, "XYZ_X", sample->result->X);
		cmsIT8SetDataDbl (it8, sample->patch_name, "XYZ_Y", sample->result->Y);
		cmsIT8SetDataDbl (it8, sample->patch_name, "XYZ_Z", sample->result->Z);
	}
	ret = cmsIT8SaveToFile (it8, filename);
	g_assert (ret);

	cmsIT8Free (it8);
}

/**
 * gcm_calibrate_native_create_it8_file:
 **/
static void
gcm_calibrate_native_create_it8_file (GcmCalibrateNative *calibrate_native, CdSensor *sensor, const gchar *filename, guint steps)
{
	GcmCalibrateNativePrivate *priv = calibrate_native->priv;
	guint i;
	gchar *patch_name;
	GcmColorSample *sample;
	GPtrArray *array;
	CdColorRGB source;
	gdouble divisions;

	/* step size */
	divisions = 1.0f / (gfloat) (steps - 1);

	array = g_ptr_array_new_with_free_func ((GDestroyNotify) gcm_calibrate_native_sample_free_cb);

	/* blue */
	source.R = 0.0;
	source.G = 0.0;
	source.B = 1.0;
	gcm_calibrate_native_sample_add (array, "CBL", &source);

	/* blue ramp */
	for (i=0; i<steps; i++) {
		patch_name = g_strdup_printf ("BR%i", i+1);
		source.B = 1.0f - (divisions * i);
		gcm_calibrate_native_sample_add (array, patch_name, &source);
		g_free (patch_name);
	}

	/* green */
	source.R = 0.0;
	source.G = 1.0;
	source.B = 0.0;
	gcm_calibrate_native_sample_add (array, "CGR", &source);

	/* green ramp */
	for (i=0; i<steps; i++) {
		patch_name = g_strdup_printf ("GR%i", i+1);
		source.G = 1.0f - (divisions * i);
		gcm_calibrate_native_sample_add (array, patch_name, &source);
		g_free (patch_name);
	}

	/* red */
	source.R = 1.0;
	source.G = 0.0;
	source.B = 0.0;
	gcm_calibrate_native_sample_add (array, "CRD", &source);

	/* red ramp */
	for (i=0; i<steps; i++) {
		patch_name = g_strdup_printf ("RR%i", i+1);
		source.R = 1.0f - (divisions * i);
		gcm_calibrate_native_sample_add (array, patch_name, &source);
		g_free (patch_name);
	}

	/* black */
	source.R = 0.0;
	source.G = 0.0;
	source.B = 0.0;
	gcm_calibrate_native_sample_add (array, "DMIN", &source);

	/* grey ramp */
	for (i=0; i<steps; i++) {
		patch_name = g_strdup_printf ("GS%i", i+1);
		source.R = divisions * i;
		source.G = divisions * i;
		source.B = divisions * i;
		gcm_calibrate_native_sample_add (array, patch_name, &source);
		g_free (patch_name);
	}

	/* white */
	source.R = 1.0;
	source.G = 1.0;
	source.B = 1.0;
	gcm_calibrate_native_sample_add (array, "DMAX", &source);

	/* measure */
	divisions = 100.0f / array->len;
	for (i=0; i<array->len; i++) {

		/* is cancelled */
		if (g_cancellable_is_cancelled (priv->cancellable))
			goto out;

		sample = g_ptr_array_index (array, i);
		gcm_sample_window_set_color (GCM_SAMPLE_WINDOW (priv->sample_window), sample->source);
		gcm_sample_window_set_percentage (GCM_SAMPLE_WINDOW (priv->sample_window), i*divisions);
		gcm_calibrate_native_sample_measure (sensor, priv->cancellable, sample);
	}

	/* write to disk */
	gcm_calibrate_native_sample_write (array, filename);
out:

	g_ptr_array_unref (array);
}

/**
 * gcm_calibrate_native_get_it8_patch_rgb_xyz:
 **/
static void
gcm_calibrate_native_get_it8_patch_rgb_xyz (cmsHANDLE it8_handle, const gchar *patch, CdColorRGB *rgb, CdColorXYZ *xyz)
{
	if (rgb != NULL) {
		rgb->R = cmsIT8GetDataDbl (it8_handle, patch, "RGB_R");
		rgb->G = cmsIT8GetDataDbl (it8_handle, patch, "RGB_G");
		rgb->B = cmsIT8GetDataDbl (it8_handle, patch, "RGB_B");
	}
	if (xyz != NULL) {
		xyz->X = cmsIT8GetDataDbl (it8_handle, patch, "XYZ_X");
		xyz->Y = cmsIT8GetDataDbl (it8_handle, patch, "XYZ_Y");
		xyz->Z = cmsIT8GetDataDbl (it8_handle, patch, "XYZ_Z");
	}
}

/**
 * gcm_calibrate_native_get_it8_patch_xyY:
 **/
static void
gcm_calibrate_native_get_it8_patch_xyY (cmsHANDLE it8_handle, const gchar *patch, cmsCIExyY *result)
{
	CdColorXYZ sample;
	gcm_calibrate_native_get_it8_patch_rgb_xyz (it8_handle, patch, NULL, &sample);
	cmsXYZ2xyY (result, (cmsCIEXYZ*) &sample);
}

/**
 * gcm_calibrate_native_interpolate_array:
 *
 * Linearly interpolate an array of data into a differently sized array.
 **/
static void
gcm_calibrate_native_interpolate_array (gdouble *data, guint data_size, gdouble *new, guint new_size)
{
	gdouble amount;
	gdouble div;
	guint i;
	guint reg1, reg2;

	/* gcm_calibrate_native_ */
	for (i=0; i<new_size; i++) {
		div = i * ((data_size - 1) / (gdouble) (new_size - 1));
		reg1 = floor (div);
		reg2 = ceil (div);
		if (reg1 == reg2) {
			/* no interpolation reqd */
			new[i] = data[reg1];
		} else {
			amount = div - (gdouble) reg1;
			new[i] = data[reg1] * (1.0f - amount) + data[reg2] * amount;
		}
	}
}

#if 0
/**
 * _cd_color_normalize_XYZ:
 *
 * Return value: the Y value that was normalised
 **/
static gdouble
_cd_color_normalize_XYZ (CdColorXYZ *xyz)
{
	gdouble Y;

	g_return_val_if_fail (xyz != NULL, 0.0f);

	/* return the value we used the scale */
	Y = xyz->Y;

	/* scale by luminance */
	xyz->X /= Y;
	xyz->Z /= Y;
	xyz->Y = 1.0;
	return Y;
}
#endif

/**
 * _cd_color_scale_XYZ:
 **/
static void
_cd_color_scale_XYZ (CdColorXYZ *xyz, gdouble scale)
{
	g_return_if_fail (xyz != NULL);

	/* scale by factor */
	xyz->X /= scale;
	xyz->Y /= scale;
	xyz->Z /= scale;
}

/**
 * gcm_calibrate_native_create_profile_from_it8:
 **/
static gboolean
gcm_calibrate_native_create_profile_from_it8 (GcmProfile *profile, const gchar *filename, guint vcgt_size, GError **error)
{
	gboolean ret = FALSE;
	guint rampsize;
	cmsHANDLE it8_handle;
	cmsCIExyYTRIPLE primaries;
	cmsCIExyY whitepoint;
	CdColorXYZ primary_red;
	CdColorXYZ primary_green;
	CdColorXYZ primary_blue;
	guint i, j;
	const gdouble gamma22 = 2.2f;
	const gchar *patch_data;
	gchar *patch_name;
	gdouble div;
	gdouble temperature;
	gdouble whitepointY;
	CdColorXYZ patch_xyz;
	CdColorRGB patch_rgb;
	CdColorRGB actual_rgb;
	CdColorXYZ actual_xyz;
	CdColorRGB scale;
	CdColorRGB leakage;
	cmsHPROFILE conversion_profile = NULL;
	cmsHPROFILE xyz_profile = NULL;
	cmsToneCurve *transfer_curve[3] = { NULL, NULL, NULL };
	cmsHTRANSFORM conversion_transform = NULL;
	gdouble *data_sampled[3] = { NULL, NULL, NULL};
	gdouble *data_interpolated[3] = { NULL, NULL, NULL};
	guint16 *data_vcgt[3] = { NULL, NULL, NULL};

	/* load it8 file */
	it8_handle = cmsIT8LoadFromFile(NULL, filename);
	if (it8_handle == NULL)
		goto out;

	/* get the whitepoint */
	gcm_calibrate_native_get_it8_patch_xyY (it8_handle, "DMAX", &whitepoint);
	cmsTempFromWhitePoint (&temperature, &whitepoint);
	g_debug ("native whitepoint=%f,%f [scale:%f] (%fK)", whitepoint.x, whitepoint.y, whitepoint.Y, temperature);

	/* we need to scale each reading by this value */
	whitepointY = whitepoint.Y;
//	_cd_color_normalize_XYZ (&whitepoint);

	/* optionally use a D50 target */
//	cmsWhitePointFromTemp (&whitepoint, 5000);

	/* normalize white point for CIECAM */
	whitepoint.Y = 1.0f;

	/* set white point */
	cmsxyY2XYZ ((cmsCIEXYZ *) &actual_xyz, &whitepoint);
	ret = gcm_profile_set_whitepoint (profile, &actual_xyz, error);
	if (!ret)
		goto out;

	/* get the primaries */
	gcm_calibrate_native_get_it8_patch_xyY (it8_handle, "CRD", &primaries.Red);
	gcm_calibrate_native_get_it8_patch_xyY (it8_handle, "CGR", &primaries.Green);
	gcm_calibrate_native_get_it8_patch_xyY (it8_handle, "CBL", &primaries.Blue);

	g_debug ("red=%f,%f green=%f,%f blue=%f,%f",
		   primaries.Red.x, primaries.Red.y,
		   primaries.Green.x, primaries.Green.y,
		   primaries.Blue.x, primaries.Blue.y);

	/* get the primaries */
	gcm_calibrate_native_get_it8_patch_rgb_xyz (it8_handle, "CRD", NULL, &primary_red);
	gcm_calibrate_native_get_it8_patch_rgb_xyz (it8_handle, "CGR", NULL, &primary_green);
	gcm_calibrate_native_get_it8_patch_rgb_xyz (it8_handle, "CBL", NULL, &primary_blue);

	/* scale the values */
	_cd_color_scale_XYZ (&primary_red, whitepointY);
	_cd_color_scale_XYZ (&primary_green, whitepointY);
	_cd_color_scale_XYZ (&primary_blue, whitepointY);

	/* set the primaries */
	ret = gcm_profile_set_primaries (profile, &primary_red, &primary_green, &primary_blue, error);
	if (!ret)
		goto out;

	/* create our RGB conversion transform */
	transfer_curve[0] = transfer_curve[1] = transfer_curve[2] = cmsBuildGamma (NULL, gamma22);
	conversion_profile = cmsCreateRGBProfile (&whitepoint, &primaries, transfer_curve);
	xyz_profile = cmsCreateXYZProfile ();
	conversion_transform = cmsCreateTransform (xyz_profile, TYPE_XYZ_DBL,
						   conversion_profile, TYPE_RGB_DBL,
						   INTENT_ABSOLUTE_COLORIMETRIC, 0);

	/* get the size of the gray ramp */
	for (i=0; i<1024; i++) {
		patch_name = g_strdup_printf ("GS%i", i+1);
		patch_data = cmsIT8GetData (it8_handle, patch_name, "RGB_R");
		g_free (patch_name);
		if (patch_data == NULL)
			break;
	}
	rampsize = i;
	if (rampsize == 0) {
		g_set_error_literal (error, 1, 0, "no gray ramp found");
		goto out;
	}
	g_debug ("rampsize = %i", rampsize);

	/* create arrays for the sampled and processed data */
	for (j=0; j<3; j++) {
		data_sampled[j] = g_new0 (gdouble, rampsize);
		data_interpolated[j] = g_new0 (gdouble, vcgt_size);
		data_vcgt[j] = g_new0 (guint16, vcgt_size);
	}

	/* get the gray data */
	for (i=0; i<rampsize; i++) {
		patch_name = g_strdup_printf ("GS%i", i+1);
		gcm_calibrate_native_get_it8_patch_rgb_xyz (it8_handle, patch_name, &patch_rgb, &patch_xyz);

		/* scale the values */
		_cd_color_scale_XYZ (&patch_xyz, whitepointY);

		cmsDoTransform (conversion_transform, &patch_xyz, &actual_rgb, 1);
		g_debug ("%s = %f,%f,%f -> %f,%f,%f", patch_name,
			   patch_rgb.R, patch_rgb.G, patch_rgb.B,
			   actual_rgb.R, actual_rgb.G, actual_rgb.B);

		data_sampled[0][i] = actual_rgb.R;
		data_sampled[1][i] = actual_rgb.G;
		data_sampled[2][i] = actual_rgb.B;
		g_free (patch_name);
	}

	for (i=0; i<rampsize; i++) {
		g_debug ("%i, %f,%f,%f", i,
			   data_sampled[0][i],
			   data_sampled[1][i],
			   data_sampled[2][i]);
	}

	/* reverse the profile, as we want to correct the display, not profile it */
	div = 1.0f / (gdouble) (rampsize - 1);
	for (i=0; i<rampsize; i++) {
		gdouble value = (div*i);
		for (j=0; j<3; j++)
			data_sampled[j][i] = value + (value - data_sampled[j][i]);
	}

	/* remove the backlight leakage */
	cd_color_set_rgb (&leakage, G_MAXDOUBLE, G_MAXDOUBLE, G_MAXDOUBLE);
	for (i=0; i<rampsize; i++) {
		if (data_sampled[0][i] < leakage.R)
			leakage.R = data_sampled[0][i];
		if (data_sampled[1][i] < leakage.G)
			leakage.G = data_sampled[1][i];
		if (data_sampled[2][i] < leakage.B)
			leakage.B = data_sampled[2][i];
	}
	for (i=0; i<rampsize; i++) {
		data_sampled[0][i] -= leakage.R;
		data_sampled[1][i] -= leakage.G;
		data_sampled[2][i] -= leakage.B;
	}
	g_debug ("removed backlight leakage = %f,%f,%f", leakage.R, leakage.G, leakage.B);

	/* scale all values to 1.0 */
	cd_color_set_rgb (&scale, 0.0, 0.0, 0.0);
	for (i=0; i<rampsize; i++) {
		if (data_sampled[0][i] > scale.R)
			scale.R = data_sampled[0][i];
		if (data_sampled[1][i] > scale.G)
			scale.G = data_sampled[1][i];
		if (data_sampled[1][i] > scale.B)
			scale.B = data_sampled[2][i];
	}
	for (i=0; i<rampsize; i++) {
		data_sampled[0][i] /= scale.R;
		data_sampled[1][i] /= scale.G;
		data_sampled[2][i] /= scale.B;
	}
	g_debug ("scaled to 1.0 using = %f,%f,%f", scale.R, scale.G, scale.B);

	for (i=0; i<rampsize; i++) {
		g_debug ("%i, %f,%f,%f", i,
			   data_sampled[0][i],
			   data_sampled[1][i],
			   data_sampled[2][i]);
	}

	for (i=0; i<rampsize; i++) {
		g_debug ("%i, %f,%f,%f", i,
			   data_sampled[0][i],
			   data_sampled[1][i],
			   data_sampled[2][i]);
	}

	/* linear interpolate the values */
	for (j=0; j<3; j++)
		gcm_calibrate_native_interpolate_array (data_sampled[j], rampsize, data_interpolated[j], vcgt_size);

	/* convert to uint16 type */
	for (j=0; j<3; j++) {
		for (i=0; i<vcgt_size; i++) {
			data_vcgt[j][i] = data_interpolated[j][i] * 0xffff;
		}
	}

	/* set this in the profile */
	ret = gcm_profile_set_vcgt_from_data (profile, data_vcgt[0], data_vcgt[1], data_vcgt[2], vcgt_size, error);
	if (!ret)
		goto out;
out:
	cmsFreeToneCurve (*transfer_curve);
	cmsCloseProfile (xyz_profile);
	cmsCloseProfile (conversion_profile);
	if (conversion_transform != NULL)
		cmsDeleteTransform (conversion_transform);
	for (j=0; j<3; j++) {
		g_free (data_vcgt[j]);
		g_free (data_interpolated[j]);
		g_free (data_sampled[j]);
	}
	if (it8_handle != NULL)
		cmsIT8Free (it8_handle);
	return ret;

}

/**
 * gcm_calibrate_native_display:
 **/
static gboolean
gcm_calibrate_native_display (GcmCalibrate *calibrate, CdSensor *sensor, GtkWindow *window, GError **error)
{
	GcmCalibrateNative *calibrate_native = GCM_CALIBRATE_NATIVE(calibrate);
	GcmCalibrateNativePrivate *priv = calibrate_native->priv;
	gboolean ret = TRUE;
	GcmCalibratePrecision precision;
	guint steps = 0;
	gchar *copyright = NULL;
	gchar *description = NULL;
	gchar *manufacturer = NULL;
	gchar *model = NULL;
	const gchar *title;
	const gchar *message;
	const gchar *filename = NULL;
	const gchar *basename;
	gchar *filename_it8 = NULL;
	gchar *filename_icc = NULL;
	GcmProfile *profile;

#if 0
	cmsHPROFILE profile;
	cmsViewingConditions PCS;
	cmsViewingConditions device;

	/* take it8 file, and open */
	profile = cmsCreateProfilePlaceholder (NULL);

	cmsSetProfileVersion (profile, 4.2);
	cmsSetDeviceClass (profile, cmsSigDisplayClass);
	cmsSetColorSpace (profile, cmsSigRgbData);
	cmsSetPCS (profile, cmsSigLabData);

	PCS.whitePoint.X = cmsD50_XYZ()->X * 100.;
	PCS.whitePoint.Y = cmsD50_XYZ()->Y * 100.;
	PCS.whitePoint.Z = cmsD50_XYZ()->Z * 100.;
	PCS.La = 32;			/* Adapting field luminance */
	PCS.Yb = 20;			/* 20% of surround */
	PCS.surround = AVG_SURROUND;
	PCS.D_value  = 1.0;		/* Complete adaptation */

	device.Yb = 20;			/* sRGB VCs */
	device.La = 4;
	device.surround = DIM_SURROUND;
	device.D_value  = 1.0;		/* Complete adaptation */

	int PCSType = PT_Lab;
	g_warning ("%i", PCSType);

	cmsSaveProfileToFile (profile, "dave.icc");
#endif

	/* show window */
	gtk_window_present (priv->sample_window);

	/* TRANSLATORS: title, instrument is a hardware color calibration sensor */
	title = _("Please attach instrument");

	/* get the image, if we have one */
//	filename = cd_sensor_get_image_display (sensor);

	/* different messages with or without image */
	if (filename != NULL) {
		/* TRANSLATORS: dialog message, ask user to attach device, and there's an example image */
		message = _("Please attach the measuring instrument to the center of the screen on the gray square like the image below.");
	} else {
		/* TRANSLATORS: dialog message, ask user to attach device */
		message = _("Please attach the measuring instrument to the center of the screen on the gray square.");
	}

	/* block for a response */
	g_debug ("blocking waiting for user input: %s", title);

	/* push new messages into the UI */
	gcm_calibrate_dialog_show (priv->calibrate_dialog, GCM_CALIBRATE_DIALOG_TAB_GENERIC, title, message);
	gcm_calibrate_dialog_set_show_button_ok (priv->calibrate_dialog, TRUE);
	gcm_calibrate_dialog_set_image_filename (priv->calibrate_dialog, filename);
	gcm_calibrate_dialog_set_show_expander (priv->calibrate_dialog, FALSE);
	gcm_calibrate_dialog_set_move_window (priv->calibrate_dialog, TRUE);

	/* TRANSLATORS: button text */
	gcm_calibrate_dialog_set_button_ok_id (priv->calibrate_dialog, _("Continue"));

	/* wait for response */
//	gtk_window_present (GTK_WINDOW (priv->calibrate_dialog));
	g_main_loop_run (priv->loop);

	/* is cancelled */
	if (g_cancellable_is_cancelled (priv->cancellable))
		goto out;

	/* set modal windows up correctly */
	gcm_calibrate_dialog_set_move_window (priv->calibrate_dialog, TRUE);
	gcm_calibrate_dialog_set_window (priv->calibrate_dialog, window);

	/* TRANSLATORS: title, drawing means painting to the screen */
	title = _("Drawing the patches");

	/* TRANSLATORS: dialog message */
	message = _("Drawing the generated patches to the screen, which will then be measured by the hardware device.");

	/* push new messages into the UI */
	gcm_calibrate_dialog_show (priv->calibrate_dialog, GCM_CALIBRATE_DIALOG_TAB_GENERIC, title, message);
	gcm_calibrate_dialog_set_show_button_ok (priv->calibrate_dialog, FALSE);
	gcm_calibrate_dialog_set_show_expander (priv->calibrate_dialog, FALSE);

	/* get filenames */
	basename = gcm_calibrate_get_basename (calibrate);
	filename_it8 = g_strdup_printf ("%s.it8", basename);
	filename_icc = g_strdup_printf ("%s.icc", basename);

	/* get the number of steps to use */
	g_object_get (calibrate,
		      "precision", &precision,
		      NULL);
	if (precision == GCM_CALIBRATE_PRECISION_SHORT)
		steps = 10;
	else if (precision == GCM_CALIBRATE_PRECISION_NORMAL)
		steps = 20;
	else if (precision == GCM_CALIBRATE_PRECISION_LONG)
		steps = 30;

	g_debug ("creating %s", filename_it8);
	gcm_calibrate_native_create_it8_file (calibrate_native, sensor, filename_it8, steps);

	/* get profile text data */
	copyright = gcm_calibrate_get_profile_copyright (calibrate);
	description = gcm_calibrate_get_profile_description (calibrate);
	model = gcm_calibrate_get_profile_model (calibrate);
	manufacturer = gcm_calibrate_get_profile_manufacturer (calibrate);

	/* create basic profile */
	profile = gcm_profile_new ();
	gcm_profile_set_colorspace (profile, CD_COLORSPACE_RGB);
	gcm_profile_set_kind (profile, CD_PROFILE_KIND_DISPLAY_DEVICE);
	gcm_profile_set_description (profile, description);
	gcm_profile_set_copyright (profile, copyright);
	gcm_profile_set_model (profile, model);
	gcm_profile_set_manufacturer (profile, manufacturer);

	/* convert sampled values to profile tables */
	ret = gcm_calibrate_native_create_profile_from_it8 (profile, filename_it8, 256, error);
	if (!ret)
		goto out;

	/* save */
	g_debug ("saving %s", filename_icc);
	ret = gcm_profile_save (profile, filename_icc, error);
	if (!ret)
		goto out;

out:
	g_free (filename_it8);
	g_free (filename_icc);
	g_free (copyright);
	g_free (description);
	g_free (manufacturer);
	g_free (model);
	return ret;
}

/**
 * gcm_calibrate_native_spotread:
 **/
static gboolean
gcm_calibrate_native_spotread (GcmCalibrate *calibrate, CdSensor *sensor, GtkWindow *window, GError **error)
{
	GcmCalibrateNative *calibrate_native = GCM_CALIBRATE_NATIVE(calibrate);
	GcmCalibrateNativePrivate *priv = calibrate_native->priv;
	gboolean ret = TRUE;
	const gchar *title;
	const gchar *message;

	/* TRANSLATORS: title, setting up the photospectromiter */
	title = _("Setting up device");
	/* TRANSLATORS: dialog message */
	message = _("Setting up the device to read a spot color…");

	/* push new messages into the UI */
	gcm_calibrate_dialog_set_move_window (priv->calibrate_dialog, FALSE);
	gcm_calibrate_dialog_set_window (priv->calibrate_dialog, window);
	gcm_calibrate_dialog_set_show_button_ok (priv->calibrate_dialog, FALSE);
	gcm_calibrate_dialog_set_show_expander (priv->calibrate_dialog, TRUE);
	gcm_calibrate_dialog_show (priv->calibrate_dialog, GCM_CALIBRATE_DIALOG_TAB_GENERIC, title, message);

//out:
	return ret;
}

/**
 * gcm_calibrate_native_response_cb:
 **/
static void
gcm_calibrate_native_response_cb (GtkWidget *widget, GtkResponseType response, GcmCalibrateNative *calibrate_native)
{
	GcmCalibrateNativePrivate *priv = calibrate_native->priv;
	if (response == GTK_RESPONSE_OK) {
		if (g_main_loop_is_running (priv->loop))
			g_main_loop_quit (priv->loop);
	}
	if (response == GTK_RESPONSE_CANCEL) {
		if (g_main_loop_is_running (priv->loop))
			g_main_loop_quit (priv->loop);
		g_cancellable_cancel (priv->cancellable);
	}
}

/**
 * gcm_calibrate_native_class_init:
 **/
static void
gcm_calibrate_native_class_init (GcmCalibrateNativeClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GcmCalibrateClass *parent_class = GCM_CALIBRATE_CLASS (klass);
	object_class->finalize = gcm_calibrate_native_finalize;

	/* setup klass links */
	parent_class->calibrate_display = gcm_calibrate_native_display;
	parent_class->calibrate_spotread = gcm_calibrate_native_spotread;

	g_type_class_add_private (klass, sizeof (GcmCalibrateNativePrivate));
}

/**
 * gcm_calibrate_native_init:
 **/
static void
gcm_calibrate_native_init (GcmCalibrateNative *calibrate_native)
{
	calibrate_native->priv = GCM_CALIBRATE_NATIVE_GET_PRIVATE (calibrate_native);

	calibrate_native->priv->loop = g_main_loop_new (NULL, FALSE);
	calibrate_native->priv->cancellable = g_cancellable_new ();

	/* common dialog */
	calibrate_native->priv->calibrate_dialog = gcm_calibrate_dialog_new ();
	g_signal_connect (calibrate_native->priv->calibrate_dialog, "response",
			  G_CALLBACK (gcm_calibrate_native_response_cb), calibrate_native);

	/* sample window */
	calibrate_native->priv->sample_window = gcm_sample_window_new ();
}

/**
 * gcm_calibrate_native_finalize:
 **/
static void
gcm_calibrate_native_finalize (GObject *object)
{
	GcmCalibrateNative *calibrate_native = GCM_CALIBRATE_NATIVE (object);
	GcmCalibrateNativePrivate *priv = calibrate_native->priv;

	/* hide */
	gcm_calibrate_dialog_hide (priv->calibrate_dialog);
	g_object_unref (priv->calibrate_dialog);
//	g_object_unref (priv->sample_window);
	g_object_unref (priv->cancellable);
	g_main_loop_unref (priv->loop);

	G_OBJECT_CLASS (gcm_calibrate_native_parent_class)->finalize (object);
}

/**
 * gcm_calibrate_native_new:
 *
 * Return value: a new GcmCalibrateNative object.
 **/
GcmCalibrate *
gcm_calibrate_native_new (void)
{
	GcmCalibrateNative *calibrate_native;
	calibrate_native = g_object_new (GCM_TYPE_CALIBRATE_NATIVE, NULL);
	return GCM_CALIBRATE (calibrate_native);
}


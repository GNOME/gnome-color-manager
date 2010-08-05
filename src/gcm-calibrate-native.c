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

/**
 * SECTION:gcm-calibrate-native
 * @short_description: Calibration object
 *
 * This object allows calibration functionality using NativeCMS.
 */

#include "config.h"

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <lcms2.h>

#include "gcm-calibrate-native.h"
#include "gcm-sensor-client.h"
#include "gcm-calibrate-dialog.h"
#include "gcm-sample-window.h"

#include "egg-debug.h"

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
	GcmCalibrateDialog		*calibrate_dialog;
};

G_DEFINE_TYPE (GcmCalibrateNative, gcm_calibrate_native, GCM_TYPE_CALIBRATE)

typedef struct {
	gchar		*patch_name;
	GcmColorRGB	*source;
	GcmColorXYZ	*result;
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
gcm_calibrate_native_sample_measure (GcmSensor *sensor, GcmColorSample *sample)
{
	GError *error = NULL;
	gboolean ret;

	ret = gcm_sensor_sample (sensor, NULL, sample->result, &error);
	g_assert_no_error (error);
	g_assert (ret);
}

/**
 * gcm_calibrate_native_sample_add:
 **/
static void
gcm_calibrate_native_sample_add (GPtrArray *array, const gchar *patch_name, const GcmColorRGB *source)
{
	GcmColorSample *sample;

	sample = g_new0 (GcmColorSample, 1);
	sample->patch_name = g_strdup (patch_name);
	sample->source = g_new0 (GcmColorRGB, 1);
	sample->result = g_new0 (GcmColorXYZ, 1);
	gcm_color_copy_RGB (source, sample->source);
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

//FIXME
#include "gcm-sensor-huey.h"

/**
 * gcm_calibrate_native_create_it8_file:
 **/
static void
gcm_calibrate_native_create_it8_file (GcmCalibrateNative *calibrate_native)
{
	GcmCalibrateNativePrivate *priv = calibrate_native->priv;
	guint i;
	gchar *patch_name;
	GcmColorSample *sample;
	GPtrArray *array;
	GcmColorRGB source;
	GcmColorXYZ result;
	gdouble divisions;
	GcmSensor *sensor;

	result.X = 0.1;
	result.Y = 0.2;
	result.Z = 0.3;

	array = g_ptr_array_new_with_free_func ((GDestroyNotify) gcm_calibrate_native_sample_free_cb);

	/* blue */
	source.R = 0.0;
	source.G = 0.0;
	source.B = 1.0;
	gcm_calibrate_native_sample_add (array, "CBL", &source);

	/* green */
	source.R = 0.0;
	source.G = 1.0;
	source.B = 0.0;
	gcm_calibrate_native_sample_add (array, "CGR", &source);

	/* red */
	source.R = 1.0;
	source.G = 0.0;
	source.B = 0.0;
	gcm_calibrate_native_sample_add (array, "CRD", &source);

	/* white */
	source.R = 1.0;
	source.G = 1.0;
	source.B = 1.0;
	gcm_calibrate_native_sample_add (array, "DMIN", &source);

	/* grey ramp */
	divisions = 1.0f / 35.0f;
	for (i=1; i<35; i++) {
		patch_name = g_strdup_printf ("GS%i", i+1);
		source.R = 1.0f - (divisions * i);
		source.G = 1.0f - (divisions * i);
		source.B = 1.0f - (divisions * i);
		gcm_calibrate_native_sample_add (array, patch_name, &source);
		g_free (patch_name);
	}

	/* black */
	source.R = 0.0;
	source.G = 0.0;
	source.B = 0.0;
	gcm_calibrate_native_sample_add (array, "DMAX", &source);

	sensor = gcm_sensor_huey_new ();

	/* measure */
	divisions = 100.0f / array->len;
	for (i=0; i<array->len; i++) {
		sample = g_ptr_array_index (array, i);
		gcm_sample_window_set_color (GCM_SAMPLE_WINDOW (priv->sample_window), sample->source);
		gcm_sample_window_set_percentage (GCM_SAMPLE_WINDOW (priv->sample_window), i*divisions);
		gcm_calibrate_native_sample_measure (sensor, sample);
	}

	g_object_unref (sensor);

	/* write to disk */
	gcm_calibrate_native_sample_write (array, "./dave.it8");

	g_ptr_array_unref (array);
}

/**
 * gcm_calibrate_native_display:
 **/
static gboolean
gcm_calibrate_native_display (GcmCalibrate *calibrate, GtkWindow *window, GError **error)
{
	GcmCalibrateNative *calibrate_native = GCM_CALIBRATE_NATIVE(calibrate);
	GcmCalibrateNativePrivate *priv = calibrate_native->priv;
	gboolean ret = TRUE;
	const gchar *title;
	const gchar *message;

	/* set modal windows up correctly */
	gcm_calibrate_dialog_set_move_window (priv->calibrate_dialog, TRUE);
	gcm_calibrate_dialog_set_window (priv->calibrate_dialog, window);

	/* TRANSLATORS: title, hardware refers to a calibration device */
	title = _("Set up display");

	/* TRANSLATORS: dialog message */
	message = _("Setting up display device for use…");

	/* push new messages into the UI */
	gcm_calibrate_dialog_show (priv->calibrate_dialog, GCM_CALIBRATE_DIALOG_TAB_GENERIC, title, message);
	gcm_calibrate_dialog_set_show_button_ok (priv->calibrate_dialog, FALSE);
	gcm_calibrate_dialog_set_show_expander (priv->calibrate_dialog, FALSE);

	/* show window */
	gtk_window_present (priv->sample_window);
	gcm_calibrate_native_create_it8_file (calibrate_native);

//out:
	return ret;
}

/**
 * gcm_calibrate_native_spotread:
 **/
static gboolean
gcm_calibrate_native_spotread (GcmCalibrate *calibrate, GtkWindow *window, GError **error)
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
	calibrate_native->priv->calibrate_dialog = gcm_calibrate_dialog_new ();
	calibrate_native->priv->sample_window = gcm_sample_window_new ();
//	g_signal_connect (calibrate_native->priv->calibrate_dialog, "response",
//			  G_CALLBACK (gcm_calibrate_native_response_cb), calibrate_native);
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
	g_object_unref (priv->sample_window);

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


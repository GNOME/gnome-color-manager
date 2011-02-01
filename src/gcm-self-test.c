/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007-2010 Richard Hughes <richard@hughsie.com>
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

#include <glib-object.h>
#include <math.h>
#include <glib/gstdio.h>

#include "gcm-brightness.h"
#include "gcm-calibrate.h"
#include "gcm-calibrate-dialog.h"
#include "gcm-calibrate-manual.h"
#include "gcm-calibrate-native.h"
#include "gcm-cie-widget.h"
#include "gcm-client.h"
#include "gcm-debug.h"
#include "gcm-exif.h"
#include "gcm-gamma-widget.h"
#include "gcm-print.h"
#include "gcm-profile.h"
#include "gcm-profile-store.h"
#include "gcm-trc-widget.h"
#include "gcm-utils.h"
#include "gcm-color.h"
#include "gcm-profile.h"
#include "gcm-color.h"
#include "gcm-brightness.h"
#include "gcm-buffer.h"
#include "gcm-color.h"
#include "gcm-clut.h"
#include "gcm-math.h"
#include "gcm-dmi.h"
#include "gcm-edid.h"
#include "gcm-image.h"
#include "gcm-profile.h"
#include "gcm-profile-store.h"
#include "gcm-sample-window.h"
#include "gcm-sensor-dummy.h"
#include "gcm-tables.h"
#include "gcm-usb.h"
#include "gcm-x11-output.h"
#include "gcm-x11-screen.h"

/** ver:1.0 ***********************************************************/
static GMainLoop *_test_loop = NULL;
static guint _test_loop_timeout_id = 0;

static gboolean
_g_test_hang_check_cb (gpointer user_data)
{
	guint timeout_ms = *((guint*) user_data);
	g_main_loop_quit (_test_loop);
	g_warning ("loop not completed in %ims", timeout_ms);
	g_assert_not_reached ();
	return FALSE;
}

/**
 * _g_test_loop_run_with_timeout:
 **/
static void
_g_test_loop_run_with_timeout (guint timeout_ms)
{
	g_assert (_test_loop_timeout_id == 0);
	_test_loop = g_main_loop_new (NULL, FALSE);
	_test_loop_timeout_id = g_timeout_add (timeout_ms, _g_test_hang_check_cb, &timeout_ms);
	g_main_loop_run (_test_loop);
}

#if 0
static gboolean
_g_test_hang_wait_cb (gpointer user_data)
{
	g_main_loop_quit (_test_loop);
	_test_loop_timeout_id = 0;
	return FALSE;
}

/**
 * _g_test_loop_wait:
 **/
static void
_g_test_loop_wait (guint timeout_ms)
{
	g_assert (_test_loop_timeout_id == 0);
	_test_loop = g_main_loop_new (NULL, FALSE);
	_test_loop_timeout_id = g_timeout_add (timeout_ms, _g_test_hang_wait_cb, &timeout_ms);
	g_main_loop_run (_test_loop);
}
#endif

/**
 * _g_test_loop_quit:
 **/
static void
_g_test_loop_quit (void)
{
	if (_test_loop_timeout_id > 0) {
		g_source_remove (_test_loop_timeout_id);
		_test_loop_timeout_id = 0;
	}
	if (_test_loop != NULL) {
		g_main_loop_quit (_test_loop);
		g_main_loop_unref (_test_loop);
		_test_loop = NULL;
	}
}

/**********************************************************************/

static void
gcm_test_assert_basename (const gchar *filename1, const gchar *filename2)
{
	gchar *basename1;
	gchar *basename2;
	basename1 = g_path_get_basename (filename1);
	basename2 = g_path_get_basename (filename2);
	g_assert_cmpstr (basename1, ==, basename2);
	g_free (basename1);
	g_free (basename2);
}


#define TEST_MAIN_OUTPUT	"LVDS-1"

static void
gcm_test_math_func (void)
{
	GcmColorRGBint rgb_int;
	GcmColorRGB rgb;
	GcmColorYxy Yxy;
	GcmColorXYZ XYZ;
	GcmMat3x3 mat;
	GcmMat3x3 matsrc;

	/* black */
	rgb_int.R = 0x00; rgb_int.G = 0x00; rgb_int.B = 0x00;
	gcm_color_convert_RGBint_to_RGB (&rgb_int, &rgb);
	g_assert_cmpfloat (rgb.R, <, 0.01);
	g_assert_cmpfloat (rgb.G, <, 0.01);
	g_assert_cmpfloat (rgb.B, <, 0.01);
	g_assert_cmpfloat (rgb.R, >, -0.01);
	g_assert_cmpfloat (rgb.G, >, -0.01);
	g_assert_cmpfloat (rgb.B, >, -0.01);

	/* white */
	rgb_int.R = 0xff; rgb_int.G = 0xff; rgb_int.B = 0xff;
	gcm_color_convert_RGBint_to_RGB (&rgb_int, &rgb);
	g_assert_cmpfloat (rgb.R, <, 1.01);
	g_assert_cmpfloat (rgb.G, <, 1.01);
	g_assert_cmpfloat (rgb.B, <, 1.01);
	g_assert_cmpfloat (rgb.R, >, 0.99);
	g_assert_cmpfloat (rgb.G, >, 0.99);
	g_assert_cmpfloat (rgb.B, >, 0.99);

	/* and back */
	gcm_color_convert_RGB_to_RGBint (&rgb, &rgb_int);
	g_assert_cmpint (rgb_int.R, ==, 0xff);
	g_assert_cmpint (rgb_int.G, ==, 0xff);
	g_assert_cmpint (rgb_int.B, ==, 0xff);

	/* black */
	rgb.R = 0.0f; rgb.G = 0.0f; rgb.B = 0.0f;
	gcm_color_convert_RGB_to_RGBint (&rgb, &rgb_int);
	g_assert_cmpint (rgb_int.R, ==, 0x00);
	g_assert_cmpint (rgb_int.G, ==, 0x00);
	g_assert_cmpint (rgb_int.B, ==, 0x00);

	/* Yxy -> XYZ */
	Yxy.Y = 21.5;
	Yxy.x = 0.31;
	Yxy.y = 0.32;
	gcm_color_convert_Yxy_to_XYZ (&Yxy, &XYZ);
	g_assert_cmpfloat (XYZ.X, <, 21.0);
	g_assert_cmpfloat (XYZ.X, >, 20.5);
	g_assert_cmpfloat (XYZ.Y, <, 22.0);
	g_assert_cmpfloat (XYZ.Y, >, 21.0);
	g_assert_cmpfloat (XYZ.Z, <, 25.0);
	g_assert_cmpfloat (XYZ.Z, >, 24.5);

	/* and back */
	gcm_color_convert_XYZ_to_Yxy (&XYZ, &Yxy);
	g_assert_cmpfloat (Yxy.Y, <, 22.0);
	g_assert_cmpfloat (Yxy.Y, >, 21.0);
	g_assert_cmpfloat (Yxy.x, <, 0.35);
	g_assert_cmpfloat (Yxy.x, >, 0.25);
	g_assert_cmpfloat (Yxy.y, <, 0.35);
	g_assert_cmpfloat (Yxy.y, >, 0.25);

	/* matrix */
	mat.m00 = 1.00f;
	gcm_mat33_clear (&mat);
	g_assert_cmpfloat (mat.m00, <, 0.001f);
	g_assert_cmpfloat (mat.m00, >, -0.001f);
	g_assert_cmpfloat (mat.m22, <, 0.001f);
	g_assert_cmpfloat (mat.m22, >, -0.001f);

	/* multiply two matrices */
	gcm_mat33_clear (&matsrc);
	matsrc.m01 = matsrc.m10 = 2.0f;
	gcm_mat33_matrix_multiply (&matsrc, &matsrc, &mat);
	g_assert_cmpfloat (mat.m00, <, 4.1f);
	g_assert_cmpfloat (mat.m00, >, 3.9f);
	g_assert_cmpfloat (mat.m11, <, 4.1f);
	g_assert_cmpfloat (mat.m11, >, 3.9f);
	g_assert_cmpfloat (mat.m22, <, 0.001f);
	g_assert_cmpfloat (mat.m22, >, -0.001f);
}

static void
gcm_test_sensor_button_pressed_cb (GcmSensor *sensor, gint *signal_count)
{
	(*signal_count)++;
}

static void
gcm_test_sensor_func (void)
{
	gboolean ret;
	GError *error = NULL;
	GcmSensor *sensor;
	gdouble value;
	GcmColorXYZ values;
	gboolean signal_count = 0;

	/* start sensor */
	sensor = gcm_sensor_dummy_new ();
	g_signal_connect (sensor, "button-pressed", G_CALLBACK (gcm_test_sensor_button_pressed_cb), &signal_count);

	/* set LEDs */
	ret = gcm_sensor_set_leds (sensor, 0x0f, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* set mode */
	gcm_sensor_set_output_type (sensor, GCM_SENSOR_OUTPUT_TYPE_LCD);
	g_assert_cmpint (gcm_sensor_get_output_type (sensor), ==, GCM_SENSOR_OUTPUT_TYPE_LCD);

	/* get ambient */
	ret = gcm_sensor_get_ambient (sensor, NULL, &value, &error);
	g_assert_cmpint (signal_count, ==, 0);
	g_assert_no_error (error);
	g_assert (ret);
	g_debug ("ambient = %.1lf Lux", value);

	/* sample color */
	ret = gcm_sensor_sample (sensor, NULL, &values, &error);
	g_assert_cmpint (signal_count, ==, 1);
	g_assert_no_error (error);
	g_assert (ret);
	g_debug ("X=%0.4lf, Y=%0.4lf, Z=%0.4lf", values.X, values.Y, values.Z);

	/* set LEDs */
	ret = gcm_sensor_set_leds (sensor, 0x00, &error);
	g_assert_no_error (error);
	g_assert (ret);

	g_object_unref (sensor);
}

typedef struct {
	const gchar *monitor_name;
	const gchar *vendor_name;
	const gchar *serial_number;
	const gchar *eisa_id;
	const gchar *checksum;
	const gchar *pnp_id;
	guint width;
	guint height;
	gfloat gamma;
} GcmEdidTestData;

static void
gcm_test_edid_test_parse_edid_file (GcmEdid *edid, const gchar *filename, GcmEdidTestData *test_data)
{
	gchar *data;
	gfloat mygamma;
	gboolean ret;
	GError *error = NULL;
	gsize length = 0;

	ret = g_file_get_contents (filename, &data, &length, &error);
	g_assert_no_error (error);
	g_assert (ret);

	ret = gcm_edid_parse (edid, (const guint8 *) data, length, &error);
	g_assert_no_error (error);
	g_assert (ret);

	g_assert_cmpstr (gcm_edid_get_monitor_name (edid), ==, test_data->monitor_name);
	g_assert_cmpstr (gcm_edid_get_vendor_name (edid), ==, test_data->vendor_name);
	g_assert_cmpstr (gcm_edid_get_serial_number (edid), ==, test_data->serial_number);
	g_assert_cmpstr (gcm_edid_get_eisa_id (edid), ==, test_data->eisa_id);
	g_assert_cmpstr (gcm_edid_get_checksum (edid), ==, test_data->checksum);
	g_assert_cmpstr (gcm_edid_get_pnp_id (edid), ==, test_data->pnp_id);
	g_assert_cmpint (gcm_edid_get_height (edid), ==, test_data->height);
	g_assert_cmpint (gcm_edid_get_width (edid), ==, test_data->width);
	mygamma = gcm_edid_get_gamma (edid);
	g_assert_cmpfloat (mygamma, >=, test_data->gamma - 0.01);
	g_assert_cmpfloat (mygamma, <, test_data->gamma + 0.01);

	g_free (data);
}

static void
gcm_test_edid_func (void)
{
	GcmEdid *edid;
	GcmEdidTestData test_data;

	edid = gcm_edid_new ();
	g_assert (edid != NULL);

	/* LG 21" LCD panel */
	test_data.monitor_name = "L225W";
	test_data.vendor_name = "Goldstar Company Ltd";
	test_data.serial_number = "34398";
	test_data.eisa_id = NULL;
	test_data.checksum = "80b7dda4c74b06366abb8fa23e71d645";
	test_data.pnp_id = "GSM";
	test_data.height = 30;
	test_data.width = 47;
	test_data.gamma = 2.2f;
	gcm_test_edid_test_parse_edid_file (edid, TESTDATADIR "/LG-L225W-External.bin", &test_data);

	/* Lenovo T61 Intel Panel */
	test_data.monitor_name = NULL;
	test_data.vendor_name = "IBM France";
	test_data.serial_number = NULL;
	test_data.eisa_id = "LTN154P2-L05";
	test_data.checksum = "c585d9e80adc65c54f0a52597e850f83";
	test_data.pnp_id = "IBM";
	test_data.height = 21;
	test_data.width = 33;
	test_data.gamma = 2.2f;
	gcm_test_edid_test_parse_edid_file (edid, TESTDATADIR "/Lenovo-T61-Internal.bin", &test_data);

	g_object_unref (edid);
}

static void
gcm_test_tables_func (void)
{
	GcmTables *tables;
	GError *error = NULL;
	gchar *vendor;

	tables = gcm_tables_new ();
	g_assert (tables != NULL);

	vendor = gcm_tables_get_pnp_id (tables, "IBM", &error);
	g_assert_no_error (error);
	g_assert (vendor != NULL);
	g_assert_cmpstr (vendor, ==, "IBM France");
	g_free (vendor);

	vendor = gcm_tables_get_pnp_id (tables, "MIL", &error);
	g_assert_no_error (error);
	g_assert (vendor != NULL);
	g_assert_cmpstr (vendor, ==, "Marconi Instruments Ltd");
	g_free (vendor);

	vendor = gcm_tables_get_pnp_id (tables, "XXX", &error);
	g_assert_error (error, 1, 0);
	g_assert_cmpstr (vendor, ==, NULL);
	g_free (vendor);

	g_object_unref (tables);
}

static void
gcm_test_profile_func (void)
{
	GcmProfile *profile;
	GFile *file;
	GcmClut *clut;
	gboolean ret;
	GError *error = NULL;
	GcmColorXYZ *xyz;
	GcmColorYxy yxy;
	GcmColorYxy red;
	GcmColorYxy green;
	GcmColorYxy blue;
	GcmColorYxy white;

	/* bluish test */
	profile = gcm_profile_new ();
	file = g_file_new_for_path (TESTDATADIR "/bluish.icc");
	ret = gcm_profile_parse (profile, file, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_object_unref (file);

	/* get CLUT */
	clut = gcm_profile_generate_vcgt (profile, 256);
	g_assert (clut != NULL);
	g_assert_cmpint (gcm_clut_get_size (clut), ==, 256);

	g_assert_cmpstr (gcm_profile_get_copyright (profile), ==, "Copyright (c) 1998 Hewlett-Packard Company");
	g_assert_cmpstr (gcm_profile_get_manufacturer (profile), ==, "IEC http://www.iec.ch");
	g_assert_cmpstr (gcm_profile_get_model (profile), ==, "IEC 61966-2.1 Default RGB colour space - sRGB");
	g_assert_cmpstr (gcm_profile_get_datetime (profile), ==, "February  9 1998, 06:49:00 AM");
	g_assert_cmpstr (gcm_profile_get_description (profile), ==, "Blueish Test");
	g_assert_cmpstr (gcm_profile_get_checksum (profile), ==, "8e2aed5dac6f8b5d8da75610a65b7f27");
	g_assert_cmpint (gcm_profile_get_kind (profile), ==, CD_PROFILE_KIND_DISPLAY_DEVICE);
	g_assert_cmpint (gcm_profile_get_colorspace (profile), ==, CD_COLORSPACE_RGB);
	g_assert_cmpint (gcm_profile_get_temperature (profile), ==, 6500);

	/* get extra data */
	g_object_get (profile,
		      "red", &xyz,
		      NULL);
	g_assert (xyz != NULL);
	gcm_color_convert_XYZ_to_Yxy (xyz, &yxy);
	g_assert_cmpfloat (fabs (yxy.x - 0.648454), <, 0.01);

	gcm_color_free_XYZ (xyz);
	g_object_unref (clut);
	g_object_unref (profile);

	/* Adobe test */
	profile = gcm_profile_new ();
	file = g_file_new_for_path (TESTDATADIR "/AdobeGammaTest.icm");
	ret = gcm_profile_parse (profile, file, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_object_unref (file);

	g_assert_cmpstr (gcm_profile_get_copyright (profile), ==, "Copyright (c) 1998 Hewlett-Packard Company Modified using Adobe Gamma");
	g_assert_cmpstr (gcm_profile_get_manufacturer (profile), ==, "IEC http://www.iec.ch");
	g_assert_cmpstr (gcm_profile_get_model (profile), ==, "IEC 61966-2.1 Default RGB colour space - sRGB");
	g_assert_cmpstr (gcm_profile_get_datetime (profile), ==, "August 16 2005, 09:49:54 PM");
	g_assert_cmpstr (gcm_profile_get_description (profile), ==, "ADOBEGAMMA-Test");
	g_assert_cmpstr (gcm_profile_get_checksum (profile), ==, "bd847723f676e2b846daaf6759330624");
	g_assert_cmpint (gcm_profile_get_kind (profile), ==, CD_PROFILE_KIND_DISPLAY_DEVICE);
	g_assert_cmpint (gcm_profile_get_colorspace (profile), ==, CD_COLORSPACE_RGB);
	g_assert_cmpint (gcm_profile_get_temperature (profile), ==, 6500);

	g_object_unref (profile);

	/* create test */
	profile = gcm_profile_new ();

	/* from my T61 */
	gcm_color_set_Yxy (&red, 1.0f, 0.569336f, 0.332031f);
	gcm_color_set_Yxy (&green, 1.0f, 0.311523f, 0.543945f);
	gcm_color_set_Yxy (&blue, 1.0f, 0.149414f, 0.131836f);
	gcm_color_set_Yxy (&white, 1.0f, 0.313477f, 0.329102f);

	/* create from chroma */
	ret = gcm_profile_create_from_chroma (profile, 2.2f, &red, &green, &blue, &white, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* add vcgt */
	ret = gcm_profile_guess_and_add_vcgt (profile, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* save */
	gcm_profile_save (profile, "dave.icc", &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_object_unref (profile);

	/* verify values */
	profile = gcm_profile_new ();
	file = g_file_new_for_path ("dave.icc");
	ret = gcm_profile_parse (profile, file, &error);
	g_assert_no_error (error);
	g_assert (ret);

	g_assert_cmpstr (gcm_profile_get_copyright (profile), ==, "No copyright, use freely");
	g_assert_cmpstr (gcm_profile_get_manufacturer (profile), ==, NULL);
	g_assert_cmpstr (gcm_profile_get_model (profile), ==, NULL);
	g_assert_cmpstr (gcm_profile_get_description (profile), ==, "RGB built-in");
	g_assert_cmpint (gcm_profile_get_kind (profile), ==, CD_PROFILE_KIND_DISPLAY_DEVICE);
	g_assert_cmpint (gcm_profile_get_colorspace (profile), ==, CD_COLORSPACE_RGB);
	g_assert_cmpint (gcm_profile_get_temperature (profile), ==, 5000);

	/* delete temp file */
	ret = g_file_delete (file, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	g_object_unref (file);
	g_object_unref (profile);
}

static void
gcm_test_clut_func (void)
{
	GcmClut *clut;
	GPtrArray *array;
	const GcmClutData *data;

	clut = gcm_clut_new ();
	g_assert (clut != NULL);

	/* set some initial properties */
	g_object_set (clut,
		      "size", 3,
		      "contrast", 100.0f,
		      "brightness", 0.0f,
		      NULL);

	array = gcm_clut_get_array (clut);
	g_assert_cmpint (array->len, ==, 3);

	data = g_ptr_array_index (array, 0);
	g_assert_cmpint (data->red, ==, 0);
	g_assert_cmpint (data->green, ==, 0);
	g_assert_cmpint (data->blue, ==, 0);

	data = g_ptr_array_index (array, 1);
	g_assert_cmpint (data->red, ==, 32767);
	g_assert_cmpint (data->green, ==, 32767);
	g_assert_cmpint (data->blue, ==, 32767);

	data = g_ptr_array_index (array, 2);
	g_assert_cmpint (data->red, ==, 65535);
	g_assert_cmpint (data->green, ==, 65535);
	g_assert_cmpint (data->blue, ==, 65535);

	g_ptr_array_unref (array);

	/* set some initial properties */
	g_object_set (clut,
		      "contrast", 99.0f,
		      "brightness", 0.0f,
		      NULL);

	array = gcm_clut_get_array (clut);
	g_assert_cmpint (array->len, ==, 3);

	data = g_ptr_array_index (array, 0);
	g_assert_cmpint (data->red, ==, 0);
	g_assert_cmpint (data->green, ==, 0);
	g_assert_cmpint (data->blue, ==, 0);
	data = g_ptr_array_index (array, 1);
	g_assert_cmpint (data->red, ==, 32439);
	g_assert_cmpint (data->green, ==, 32439);
	g_assert_cmpint (data->blue, ==, 32439);
	data = g_ptr_array_index (array, 2);
	g_assert_cmpint (data->red, ==, 64879);
	g_assert_cmpint (data->green, ==, 64879);
	g_assert_cmpint (data->blue, ==, 64879);

	g_ptr_array_unref (array);

	/* set some initial properties */
	g_object_set (clut,
		      "contrast", 100.0f,
		      "brightness", 1.0f,
		      NULL);

	array = gcm_clut_get_array (clut);
	g_assert_cmpint (array->len, ==, 3);

	data = g_ptr_array_index (array, 0);
	g_assert_cmpint (data->red, ==, 655);
	g_assert_cmpint (data->green, ==, 655);
	g_assert_cmpint (data->blue, ==, 655);
	data = g_ptr_array_index (array, 1);
	g_assert_cmpint (data->red, ==, 33094);
	g_assert_cmpint (data->green, ==, 33094);
	g_assert_cmpint (data->blue, ==, 33094);
	data = g_ptr_array_index (array, 2);
	g_assert_cmpint (data->red, ==, 65535);
	g_assert_cmpint (data->green, ==, 65535);
	g_assert_cmpint (data->blue, ==, 65535);

	g_ptr_array_unref (array);

	g_object_unref (clut);
}

static void
gcm_test_dmi_func (void)
{
	GcmDmi *dmi;

	dmi = gcm_dmi_new ();
	g_assert (dmi != NULL);
	g_assert (gcm_dmi_get_name (dmi) != NULL);
//	g_assert (gcm_dmi_get_version (dmi) != NULL);
	g_assert (gcm_dmi_get_vendor (dmi) != NULL);
	g_object_unref (dmi);
}

static void
gcm_test_xyz_func (void)
{
	GcmColorXYZ *xyz;
	GcmColorYxy yxy;

	xyz = gcm_color_new_XYZ ();
	g_assert (xyz != NULL);

	/* nothing set */
	gcm_color_convert_XYZ_to_Yxy (xyz, &yxy);
	g_assert_cmpfloat (fabs (yxy.x - 0.0f), <, 0.001f);

	/* set dummy values */
	gcm_color_set_XYZ (xyz, 0.125, 0.25, 0.5);
	gcm_color_convert_XYZ_to_Yxy (xyz, &yxy);

	g_assert_cmpfloat (fabs (yxy.x - 0.142857143f), <, 0.001f);
	g_assert_cmpfloat (fabs (yxy.y - 0.285714286f), <, 0.001f);

	gcm_color_free_XYZ (xyz);
}

static void
gcm_test_profile_store_func (void)
{
	GcmProfileStore *store;
	GPtrArray *array;
	GcmProfile *profile;
	gboolean ret;

	store = gcm_profile_store_new ();
	g_assert (store != NULL);

	/* add test files */
	ret = gcm_profile_store_search_path (store, TESTDATADIR "/.");
	g_assert (ret);

	/* profile does not exist */
	profile = gcm_profile_store_get_by_filename (store, "xxxxxxxxx");
	g_assert (profile == NULL);

	/* profile does exist */
	profile = gcm_profile_store_get_by_checksum (store, "8e2aed5dac6f8b5d8da75610a65b7f27");
	g_assert (profile != NULL);
	g_assert_cmpstr (gcm_profile_get_checksum (profile), ==, "8e2aed5dac6f8b5d8da75610a65b7f27");
	g_object_unref (profile);

	/* get array of profiles */
	array = gcm_profile_store_get_array (store);
	g_assert (array != NULL);
	g_assert_cmpint (array->len, ==, 3);
	g_ptr_array_unref (array);

	g_object_unref (store);
}

static void
gcm_test_brightness_func (void)
{
	GcmBrightness *brightness;
	gboolean ret;
	GError *error = NULL;
	guint orig_percentage;
	guint percentage;

	brightness = gcm_brightness_new ();
	g_assert (brightness != NULL);

	ret = gcm_brightness_get_percentage (brightness, &orig_percentage, &error);
	g_assert_no_error (error);
	g_assert (ret);

	ret = gcm_brightness_set_percentage (brightness, 10, &error);
	g_assert_no_error (error);
	g_assert (ret);

	ret = gcm_brightness_get_percentage (brightness, &percentage, &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert_cmpint (percentage, >, 5);
	g_assert_cmpint (percentage, <, 15);

	ret = gcm_brightness_set_percentage (brightness, orig_percentage, &error);
	g_assert_no_error (error);
	g_assert (ret);

	g_object_unref (brightness);
}

static void
gcm_test_image_func (void)
{
	GcmImage *image;
	GtkWidget *image_test;
	GtkWidget *dialog;
	GtkWidget *vbox;
	gint response;
	gboolean ret;
	GcmProfile *profile;
	GFile *file;

	image = gcm_image_new ();
	g_assert (image != NULL);

	gtk_image_set_from_file (GTK_IMAGE(image), TESTDATADIR "/image-widget.png");

	/* show in a dialog as an example */
	dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, "Does color-corrected image match\nthe picture below?");
	image_test = gtk_image_new_from_file (TESTDATADIR "/image-widget-good.png");
	vbox = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	gtk_box_pack_end (GTK_BOX(vbox), GTK_WIDGET(image), TRUE, TRUE, 12);
	gtk_box_pack_end (GTK_BOX(vbox), image_test, TRUE, TRUE, 12);
	gtk_widget_set_size_request (GTK_WIDGET(image), 300, 300);
	gtk_window_set_resizable (GTK_WINDOW (dialog), TRUE);
	gtk_widget_show (GTK_WIDGET(image));
	gtk_widget_show (image_test);

	g_object_set (image,
		      "use-embedded-profile", TRUE,
		      "output-profile", NULL,
		      NULL);

	response = gtk_dialog_run (GTK_DIALOG (dialog));
	g_assert ((response == GTK_RESPONSE_YES));

	gtk_image_set_from_file (GTK_IMAGE(image_test), TESTDATADIR "/image-widget-nonembed.png");
	g_object_set (image,
		      "use-embedded-profile", FALSE,
		      NULL);

	response = gtk_dialog_run (GTK_DIALOG (dialog));
	g_assert ((response == GTK_RESPONSE_YES));

	gtk_image_set_from_file (GTK_IMAGE(image_test), TESTDATADIR "/image-widget-output.png");
	g_object_set (image,
		      "use-embedded-profile", TRUE,
		      NULL);

	/* get test file */
	profile = gcm_profile_new ();
	file = g_file_new_for_path (TESTDATADIR "/ibm-t61.icc");
	ret = gcm_profile_parse (profile, file, NULL);
	g_object_unref (file);
	g_assert (ret);
	gcm_image_set_output_profile (image, profile);
	g_object_unref (profile);

	response = gtk_dialog_run (GTK_DIALOG (dialog));
	g_assert ((response == GTK_RESPONSE_YES));

	gtk_widget_destroy (dialog);
}

static void
gcm_test_usb_func (void)
{
	GcmUsb *usb;
	gboolean ret;
	GError *error = NULL;

	usb = gcm_usb_new ();
	g_assert (usb != NULL);
	g_assert (!gcm_usb_get_connected (usb));
	g_assert (gcm_usb_get_device_handle (usb) == NULL);

	/* try to load */
	ret = gcm_usb_load (usb, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* attach to the default mainloop */
	gcm_usb_attach_to_context (usb, NULL);

	/* connect to a non-existant device */
	ret = gcm_usb_connect (usb, 0xffff, 0xffff, 0x1, 0x1, &error);
	g_assert (!ret);
	g_assert_error (error, GCM_USB_ERROR, GCM_USB_ERROR_INTERNAL);
	g_error_free (error);

	g_object_unref (usb);
}

static void
gcm_test_buffer_func (void)
{
	guchar buffer[4];

	gcm_buffer_write_uint16_be (buffer, 255);
	g_assert_cmpint (buffer[0], ==, 0x00);
	g_assert_cmpint (buffer[1], ==, 0xff);
	g_assert_cmpint (gcm_buffer_read_uint16_be (buffer), ==, 255);

	gcm_buffer_write_uint16_le (buffer, 8192);
	g_assert_cmpint (buffer[0], ==, 0x00);
	g_assert_cmpint (buffer[1], ==, 0x20);
	g_assert_cmpint (gcm_buffer_read_uint16_le (buffer), ==, 8192);
}

static void
gcm_test_x11_func (void)
{
	GcmX11Screen *screen;
	GcmX11Output *output;
	guint x, y;
	guint width, height;
	GError *error = NULL;
	gboolean ret;

	/* new object */
	screen = gcm_x11_screen_new ();
	ret = gcm_x11_screen_assign (screen, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* get object */
	output = gcm_x11_screen_get_output_by_name (screen, TEST_MAIN_OUTPUT, &error);
	g_assert_no_error (error);
	g_assert (output != NULL);

	/* check parameters */
	gcm_x11_output_get_position (output, &x, &y);
//	g_assert_cmpint (x, ==, 0);
	g_assert_cmpint (y, ==, 0);
	gcm_x11_output_get_size (output, &width, &height);
	g_assert_cmpint (width, >, 0);
	g_assert_cmpint (height, >, 0);
	g_assert (gcm_x11_output_get_connected (output));

	g_object_unref (output);
	g_object_unref (screen);
}

static gboolean
gcm_test_sample_window_loop_cb (GMainLoop *loop)
{
	g_main_loop_quit (loop);
	return FALSE;
}

static void
gcm_test_sample_window_move_window (GtkWindow *window, const gchar *output_name)
{
	GcmX11Screen *screen;
	GcmX11Output *output;
	guint x, y;
	guint width, height;
	gint window_width, window_height;
	GError *error = NULL;
	gboolean ret;

	/* get new screen */
	screen = gcm_x11_screen_new ();
	ret = gcm_x11_screen_assign (screen, NULL, &error);
	if (!ret) {
		g_warning ("failed to assign screen: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* get output */
	output = gcm_x11_screen_get_output_by_name (screen, output_name, &error);
	if (output == NULL) {
		g_warning ("failed to get output: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* center the window on this output */
	gcm_x11_output_get_position (output, &x, &y);
	gcm_x11_output_get_size (output, &width, &height);
	gtk_window_get_size (window, &window_width, &window_height);
	gtk_window_move (window, x + ((width - window_width) / 2), y + ((height - window_height) / 2));
out:
	if (output != NULL)
		g_object_unref (output);
	g_object_unref (screen);
}

static void
gcm_test_sample_window_func (void)
{
	GtkWindow *window;
	GMainLoop *loop;
	GcmColorRGB source;

	window = gcm_sample_window_new ();
	g_assert (window != NULL);
	source.R = 1.0f;
	source.G = 1.0f;
	source.B = 0.0f;
	gcm_sample_window_set_color (GCM_SAMPLE_WINDOW (window), &source);
	gcm_sample_window_set_percentage (GCM_SAMPLE_WINDOW (window), GCM_SAMPLE_WINDOW_PERCENTAGE_PULSE);

	/* move to the center of device lvds1 */
	gcm_test_sample_window_move_window (window, TEST_MAIN_OUTPUT);
	gtk_window_present (window);

	loop = g_main_loop_new (NULL, FALSE);
	g_timeout_add_seconds (2, (GSourceFunc) gcm_test_sample_window_loop_cb, loop);
	g_main_loop_run (loop);

	g_main_loop_unref (loop);
	gtk_widget_destroy (GTK_WIDGET (window));
}

static void
gcm_test_calibrate_func (void)
{
	GcmCalibrate *calibrate;
	gboolean ret;
	GError *error = NULL;
	gchar *model;
	gchar *manufacturer;

	calibrate = gcm_calibrate_new ();
	g_assert (calibrate != NULL);

	/* calibrate display manually */
	ret = gcm_calibrate_set_from_exif (GCM_CALIBRATE(calibrate), TESTDATADIR "/test.tif", &error);
	g_assert_no_error (error);
	g_assert (ret);
	model = gcm_calibrate_get_profile_model (calibrate);
	manufacturer = gcm_calibrate_get_profile_manufacturer (calibrate);
	g_assert_cmpstr (model, ==, "NIKON D60");
	g_assert_cmpstr (manufacturer, ==, "NIKON CORPORATION");
	g_free (model);
	g_free (manufacturer);
	g_object_unref (calibrate);
}

static void
gcm_test_calibrate_dialog_func (void)
{
	GcmCalibrateDialog *calibrate_dialog;
	calibrate_dialog = gcm_calibrate_dialog_new ();
	g_assert (calibrate_dialog != NULL);
	g_object_unref (calibrate_dialog);
}

static void
gcm_test_calibrate_native_func (void)
{
	gboolean ret;
	GError *error = NULL;
	GcmCalibrate *calibrate;
	GcmX11Screen *screen;
	gchar *contents;
	gchar *filename = NULL;

	calibrate = gcm_calibrate_native_new ();
	g_assert (calibrate != NULL);

	g_object_set (calibrate,
		      "output-name", "LVDS1",
		      NULL);


	/* set device */
	g_object_set (device,
		      "native-device", "LVDS1",
		      NULL);
	ret = gcm_calibrate_set_from_device (calibrate, device, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* use a screen */
	screen = gcm_x11_screen_new ();
	ret = gcm_x11_screen_assign (screen, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* clear any VCGT */
	ret = cd_device_xrandr_reset (device, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* do the calibration */
	ret = gcm_calibrate_display (calibrate, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* be good and restore the settings if they changed */
	ret = g_spawn_command_line_sync (BINDIR "/gcm-apply", NULL, NULL, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	g_unlink (filename);
	g_free (contents);
	g_free (filename);
	g_object_unref (screen);
	g_object_unref (calibrate);
}

static void
gcm_test_calibrate_manual_func (void)
{
	GcmCalibrate *calibrate;
	gboolean ret;
	GError *error = NULL;

	calibrate = gcm_calibrate_manual_new ();
	g_assert (calibrate != NULL);

	/* set to avoid a critical warning */
	g_object_set (calibrate,
		      "output-name", "lvds1",
		      NULL);

	/* calibrate display manually */
	ret = gcm_calibrate_display (calibrate, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	g_object_unref (calibrate);
}

static void
gcm_test_cie_widget_func (void)
{
	GtkWidget *widget;
	GtkWidget *image;
	GtkWidget *dialog;
	GtkWidget *vbox;
	GcmProfile *profile;
	GcmColorXYZ *white;
	GcmColorXYZ *red;
	GcmColorXYZ *green;
	GcmColorXYZ *blue;
	gint response;
	GFile *file = NULL;
	GcmColorYxy white_Yxy;
	GcmColorYxy red_Yxy;
	GcmColorYxy green_Yxy;
	GcmColorYxy blue_Yxy;

	widget = gcm_cie_widget_new ();
	g_assert (widget != NULL);

	profile = gcm_profile_new ();
	file = g_file_new_for_path (TESTDATADIR "/bluish.icc");
	gcm_profile_parse (profile, file, NULL);
	g_object_get (profile,
		      "white", &white,
		      "red", &red,
		      "green", &green,
		      "blue", &blue,
		      NULL);
	g_object_unref (file);

	gcm_color_convert_XYZ_to_Yxy (white, &white_Yxy);
	gcm_color_convert_XYZ_to_Yxy (red, &red_Yxy);
	gcm_color_convert_XYZ_to_Yxy (green, &green_Yxy);
	gcm_color_convert_XYZ_to_Yxy (blue, &blue_Yxy);

	g_object_set (widget,
		      "red", &red_Yxy,
		      "green", &green_Yxy,
		      "blue", &blue_Yxy,
		      "white", &white_Yxy,
		      NULL);

	/* show in a dialog as an example */
	dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, "Does CIE widget match\nthe picture below?");
	image = gtk_image_new_from_file (TESTDATADIR "/cie-widget.png");
	vbox = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	gtk_box_pack_end (GTK_BOX(vbox), widget, TRUE, TRUE, 12);
	gtk_box_pack_end (GTK_BOX(vbox), image, TRUE, TRUE, 12);
	gtk_widget_set_size_request (widget, 300, 300);
	gtk_window_set_resizable (GTK_WINDOW (dialog), TRUE);
	gtk_widget_show (widget);
	gtk_widget_show (image);

	response = gtk_dialog_run (GTK_DIALOG (dialog));

	g_assert ((response == GTK_RESPONSE_YES));

	gtk_widget_destroy (dialog);

	g_object_unref (profile);
	gcm_color_free_XYZ (white);
	gcm_color_free_XYZ (red);
	gcm_color_free_XYZ (green);
	gcm_color_free_XYZ (blue);
}

static void
gcm_test_exif_func (void)
{
	GcmExif *exif;
	gboolean ret;
	GError *error = NULL;
	GFile *file;

	exif = gcm_exif_new ();
	g_assert (exif != NULL);

	/* TIFF */
	file = g_file_new_for_path (TESTDATADIR "/test.tif");
	ret = gcm_exif_parse (exif, file, &error);
	g_object_unref (file);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert_cmpstr (gcm_exif_get_model (exif), ==, "NIKON D60");
	g_assert_cmpstr (gcm_exif_get_manufacturer (exif), ==, "NIKON CORPORATION");
	g_assert_cmpstr (gcm_exif_get_serial (exif), ==, NULL);
	g_assert_cmpint (gcm_exif_get_device_kind (exif), ==, CD_DEVICE_KIND_CAMERA);

	/* JPG */
	file = g_file_new_for_path (TESTDATADIR "/test.jpg");
	ret = gcm_exif_parse (exif, file, &error);
	g_object_unref (file);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert_cmpstr (gcm_exif_get_model (exif), ==, "NIKON D60");
	g_assert_cmpstr (gcm_exif_get_manufacturer (exif), ==, "NIKON CORPORATION");
	g_assert_cmpstr (gcm_exif_get_serial (exif), ==, NULL);
	g_assert_cmpint (gcm_exif_get_device_kind (exif), ==, CD_DEVICE_KIND_CAMERA);

	/* RAW */
	file = g_file_new_for_path (TESTDATADIR "/test.kdc");
	ret = gcm_exif_parse (exif, file, &error);
	g_object_unref (file);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert_cmpstr (gcm_exif_get_model (exif), ==, "Eastman Kodak Company");
	g_assert_cmpstr (gcm_exif_get_manufacturer (exif), ==, "Kodak Digital Science DC50 Zoom Camera");
	g_assert_cmpstr (gcm_exif_get_serial (exif), ==, NULL);
	g_assert_cmpint (gcm_exif_get_device_kind (exif), ==, CD_DEVICE_KIND_CAMERA);

	/* PNG */
	file = g_file_new_for_path (TESTDATADIR "/test.png");
	ret = gcm_exif_parse (exif, file, &error);
	g_object_unref (file);
	g_assert_error (error, GCM_EXIF_ERROR, GCM_EXIF_ERROR_NO_SUPPORT);
	g_assert (!ret);

	g_object_unref (exif);
}

static void
gcm_test_gamma_widget_func (void)
{
	GtkWidget *widget;
	GtkWidget *image;
	GtkWidget *dialog;
	GtkWidget *vbox;
	gint response;

	widget = gcm_gamma_widget_new ();
	g_assert (widget != NULL);

	g_object_set (widget,
		      "color-light", 0.5f,
		      "color-dark", 0.0f,
		      "color-red", 0.25f,
		      "color-green", 0.25f,
		      "color-blue", 0.25f,
		      NULL);

	/* show in a dialog as an example */
	dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, "Does GAMMA widget match\nthe picture below?");
	image = gtk_image_new_from_file (TESTDATADIR "/gamma-widget.png");
	vbox = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	gtk_box_pack_end (GTK_BOX(vbox), widget, TRUE, TRUE, 12);
	gtk_box_pack_end (GTK_BOX(vbox), image, TRUE, TRUE, 12);
	gtk_widget_set_size_request (widget, 300, 300);
	gtk_window_set_resizable (GTK_WINDOW (dialog), TRUE);
	gtk_widget_show (widget);
	gtk_widget_show (image);

	response = gtk_dialog_run (GTK_DIALOG (dialog));

	g_assert ((response == GTK_RESPONSE_YES));

	gtk_widget_destroy (dialog);
}

static GPtrArray *
gcm_print_test_render_cb (GcmPrint *print,  GtkPageSetup *page_setup, gpointer user_data, GError **error)
{
	GPtrArray *filenames;
	filenames = g_ptr_array_new_with_free_func (g_free);
	g_ptr_array_add (filenames, g_strdup (TESTDATADIR "/image-widget-nonembed.png"));
	g_ptr_array_add (filenames, g_strdup (TESTDATADIR "/image-widget-good.png"));
	return filenames;
}

static void
gcm_test_print_func (void)
{
	gboolean ret;
	GError *error = NULL;
	GcmPrint *print;

	print = gcm_print_new ();

	ret = gcm_print_with_render_callback (print, NULL, gcm_print_test_render_cb, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	g_object_unref (print);
}

static void
gcm_test_trc_widget_func (void)
{
	GtkWidget *widget;
	GtkWidget *image;
	GtkWidget *dialog;
	GtkWidget *vbox;
	GcmClut *clut;
	GcmProfile *profile;
	gint response;
	GFile *file;

	widget = gcm_trc_widget_new ();
	g_assert (widget != NULL);

	profile = gcm_profile_new ();
	file = g_file_new_for_path (TESTDATADIR "/AdobeGammaTest.icm");
	gcm_profile_parse (profile, file, NULL);
	clut = gcm_profile_generate_vcgt (profile, 256);
	g_object_set (widget,
		      "clut", clut,
		      NULL);
	g_object_unref (file);

	/* show in a dialog as an example */
	dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, "Does TRC widget match\nthe picture below?");
	image = gtk_image_new_from_file (TESTDATADIR "/trc-widget.png");
	vbox = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	gtk_box_pack_end (GTK_BOX(vbox), widget, TRUE, TRUE, 12);
	gtk_box_pack_end (GTK_BOX(vbox), image, TRUE, TRUE, 12);
	gtk_widget_set_size_request (widget, 300, 300);
	gtk_window_set_resizable (GTK_WINDOW (dialog), TRUE);
	gtk_widget_show (widget);
	gtk_widget_show (image);

	response = gtk_dialog_run (GTK_DIALOG (dialog));

	g_assert ((response == GTK_RESPONSE_YES));

	gtk_widget_destroy (dialog);

	g_object_unref (clut);
	g_object_unref (profile);
}

static void
gcm_test_utils_func (void)
{
	gboolean ret;
	gchar *text;
	gchar *filename;
	GFile *file;
	GFile *dest;

	text = gcm_utils_linkify ("http://www.dave.org is text http://www.hughsie.com that needs to be linked to http://www.bbc.co.uk really");
	g_assert_cmpstr (text, ==, "<a href=\"http://www.dave.org\">http://www.dave.org</a> is text "
			     "<a href=\"http://www.hughsie.com\">http://www.hughsie.com</a> that needs to be linked to "
			     "<a href=\"http://www.bbc.co.uk\">http://www.bbc.co.uk</a> really");
	g_free (text);

	file = g_file_new_for_path ("dave.icc");
	dest = gcm_utils_get_profile_destination (file);
	filename = g_file_get_path (dest);
	g_assert (g_str_has_suffix (filename, "/.local/share/icc/dave.icc"));
	g_free (filename);
	g_object_unref (file);
	g_object_unref (dest);

	file = g_file_new_for_path (TESTDATADIR "/bluish.icc");
	ret = gcm_utils_is_icc_profile (file);
	g_assert (ret);
	g_object_unref (file);

	ret = gcm_utils_output_is_lcd_internal ("LVDS1");
	g_assert (ret);

	ret = gcm_utils_output_is_lcd_internal ("DVI1");
	g_assert (!ret);

	ret = gcm_utils_output_is_lcd ("LVDS1");
	g_assert (ret);

	ret = gcm_utils_output_is_lcd ("DVI1");
	g_assert (ret);

	filename = g_strdup ("Hello\n\rWorld!");
	gcm_utils_alphanum_lcase (filename);
	g_assert_cmpstr (filename, ==, "hello__world_");
	g_free (filename);

	filename = g_strdup ("Hel lo\n\rWo-(r)ld!");
	gcm_utils_ensure_sensible_filename (filename);
	g_assert_cmpstr (filename, ==, "Hel lo__Wo-(r)ld_");
	g_free (filename);

	g_assert (gcm_utils_device_kind_to_profile_kind (CD_DEVICE_KIND_SCANNER) == CD_PROFILE_KIND_INPUT_DEVICE);
	g_assert (gcm_utils_device_kind_to_profile_kind (CD_DEVICE_KIND_UNKNOWN) == CD_PROFILE_KIND_UNKNOWN);
}

int
main (int argc, char **argv)
{
	if (! g_thread_supported ())
		g_thread_init (NULL);
	gtk_init (&argc, &argv);
	g_test_init (&argc, &argv, NULL);

	/* setup manually as we have no GMainContext */
	gcm_debug_setup (g_getenv ("VERBOSE") != NULL);

	g_test_add_func ("/color/calibrate", gcm_test_calibrate_func);
	g_test_add_func ("/color/exif", gcm_test_exif_func);
	g_test_add_func ("/color/utils", gcm_test_utils_func);
	g_test_add_func ("/color/calibrate_dialog", gcm_test_calibrate_dialog_func);
	g_test_add_func ("/color/math", gcm_test_math_func);
	g_test_add_func ("/color/sensor", gcm_test_sensor_func);
	g_test_add_func ("/color/edid", gcm_test_edid_func);
	g_test_add_func ("/color/tables", gcm_test_tables_func);
	g_test_add_func ("/color/profile", gcm_test_profile_func);
	g_test_add_func ("/color/clut", gcm_test_clut_func);
	g_test_add_func ("/color/xyz", gcm_test_xyz_func);
	g_test_add_func ("/color/dmi", gcm_test_dmi_func);
	g_test_add_func ("/color/x11", gcm_test_x11_func);
	g_test_add_func ("/color/profile_store", gcm_test_profile_store_func);
	g_test_add_func ("/color/buffer", gcm_test_buffer_func);
	g_test_add_func ("/color/sample-window", gcm_test_sample_window_func);
	g_test_add_func ("/color/usb", gcm_test_usb_func);
	if (g_test_thorough ()) {
		g_test_add_func ("/color/brightness", gcm_test_brightness_func);
		g_test_add_func ("/color/image", gcm_test_image_func);
		g_test_add_func ("/color/calibrate_native", gcm_test_calibrate_native_func);
		g_test_add_func ("/color/trc", gcm_test_trc_widget_func);
		g_test_add_func ("/color/cie", gcm_test_cie_widget_func);
		g_test_add_func ("/color/gamma_widget", gcm_test_gamma_widget_func);
	}
	if (g_test_slow ()) {
		g_test_add_func ("/color/print", gcm_test_print_func);
		g_test_add_func ("/color/calibrate_manual", gcm_test_calibrate_manual_func);
	}

	return g_test_run ();
}


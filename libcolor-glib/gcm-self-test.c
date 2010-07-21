/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007-2010 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU Lesser General Public License Version 2.1
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include "config.h"

#include <glib-object.h>
#include <math.h>

#include "gcm-common.h"
#include "gcm-sensor-dummy.h"
#include "gcm-ddc-client.h"
#include "gcm-ddc-device.h"
#include "gcm-edid.h"
#include "gcm-brightness.h"
#include "gcm-tables.h"
#include "gcm-profile.h"
#include "gcm-profile-store.h"
#include "gcm-clut.h"
#include "gcm-xyz.h"
#include "gcm-dmi.h"
#include "gcm-image.h"
#include "gcm-usb.h"

static void
gcm_test_common_func (void)
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
gcm_test_ddc_device_func (void)
{
	GcmDdcDevice *device;

	device = gcm_ddc_device_new ();
	g_assert (device != NULL);

	g_object_unref (device);
}

static void
gcm_test_ddc_client_func (void)
{
	GcmDdcClient *client;

	client = gcm_ddc_client_new ();
	g_assert (client != NULL);

	g_object_unref (client);
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

	ret = gcm_sensor_startup (sensor, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* set LEDs */
	ret = gcm_sensor_set_leds (sensor, 0x0f, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* set mode */
	gcm_sensor_set_output_type (sensor, GCM_SENSOR_OUTPUT_TYPE_LCD);
	g_assert_cmpint (gcm_sensor_get_output_type (sensor), ==, GCM_SENSOR_OUTPUT_TYPE_LCD);

	/* get ambient */
	ret = gcm_sensor_get_ambient (sensor, &value, &error);
	g_assert_cmpint (signal_count, ==, 0);
	g_assert_no_error (error);
	g_assert (ret);
	g_debug ("ambient = %.1lf Lux", value);

	/* sample color */
	ret = gcm_sensor_sample (sensor, &values, &error);
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

	ret = g_file_get_contents (filename, &data, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	ret = gcm_edid_parse (edid, (const guint8 *) data, &error);
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
	GcmXyz *xyz;

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
	g_assert_cmpint (gcm_profile_get_kind (profile), ==, GCM_PROFILE_KIND_DISPLAY_DEVICE);
	g_assert_cmpint (gcm_profile_get_colorspace (profile), ==, GCM_COLORSPACE_RGB);
	g_assert (gcm_profile_get_has_vcgt (profile));

	/* get extra data */
	g_object_get (profile,
		      "red", &xyz,
		      NULL);
	g_assert_cmpfloat (fabs (gcm_xyz_get_x (xyz) - 0.648454), <, 0.01);

	g_object_unref (xyz);
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
	g_assert_cmpint (gcm_profile_get_kind (profile), ==, GCM_PROFILE_KIND_DISPLAY_DEVICE);
	g_assert_cmpint (gcm_profile_get_colorspace (profile), ==, GCM_COLORSPACE_RGB);
	g_assert (gcm_profile_get_has_vcgt (profile));

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
	g_assert (gcm_dmi_get_version (dmi) != NULL);
	g_assert (gcm_dmi_get_vendor (dmi) != NULL);
	g_object_unref (dmi);
}

static void
gcm_test_xyz_func (void)
{
	GcmXyz *xyz;
	gdouble value;

	xyz = gcm_xyz_new ();
	g_assert (xyz != NULL);

	/* nothing set */
	value = gcm_xyz_get_x (xyz);
	g_assert_cmpfloat (fabs (value - 0.0f), <, 0.001f);

	/* set dummy values */
	g_object_set (xyz,
		      "cie-x", 0.125,
		      "cie-y", 0.25,
		      "cie-z", 0.5,
		      NULL);

	value = gcm_xyz_get_x (xyz);
	g_assert_cmpfloat (fabs (value - 0.142857143f), <, 0.001f);

	value = gcm_xyz_get_y (xyz);
	g_assert_cmpfloat (fabs (value - 0.285714286f), <, 0.001f);

	value = gcm_xyz_get_z (xyz);
	g_assert_cmpfloat (fabs (value - 0.571428571f), <, 0.001f);

	g_object_unref (xyz);
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

int
main (int argc, char **argv)
{
	g_type_init ();

	g_test_init (&argc, &argv, NULL);

	/* tests go here */
	g_test_add_func ("/libcolor-glib/common", gcm_test_common_func);
	g_test_add_func ("/libcolor-glib/ddc-device", gcm_test_ddc_device_func);
	g_test_add_func ("/libcolor-glib/ddc-client", gcm_test_ddc_client_func);
	g_test_add_func ("/libcolor-glib/sensor", gcm_test_sensor_func);
	g_test_add_func ("/libcolor-glib/edid", gcm_test_edid_func);
	g_test_add_func ("/libcolor-glib/tables", gcm_test_tables_func);
	g_test_add_func ("/libcolor-glib/profile", gcm_test_profile_func);
	g_test_add_func ("/libcolor-glib/clut", gcm_test_clut_func);
	g_test_add_func ("/libcolor-glib/xyz", gcm_test_xyz_func);
	g_test_add_func ("/libcolor-glib/dmi", gcm_test_dmi_func);
	g_test_add_func ("/libcolor-glib/profile_store", gcm_test_profile_store_func);
	g_test_add_func ("/libcolor-glib/usb", gcm_test_usb_func);
	if (g_test_thorough ()) {
		g_test_add_func ("/libcolor-glib/brightness", gcm_test_brightness_func);
		g_test_add_func ("/libcolor-glib/image", gcm_test_image_func);
	}

	return g_test_run ();
}


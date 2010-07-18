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

#include "gcm-common.h"
#include "gcm-sensor-dummy.h"
#include "gcm-ddc-client.h"
#include "gcm-ddc-device.h"
#include "gcm-edid.h"
#include "gcm-tables.h"

static void
gcm_test_common_func (void)
{
	GcmColorRgbInt rgb_int;
	GcmColorRgb rgb;
	GcmColorYxy Yxy;
	GcmColorXYZ XYZ;
	GcmMat3x3 mat;
	GcmMat3x3 matsrc;

	/* black */
	rgb_int.red = 0x00; rgb_int.green = 0x00; rgb_int.blue = 0x00;
	gcm_convert_rgb_int_to_rgb (&rgb_int, &rgb);
	g_assert_cmpfloat (rgb.red, <, 0.01);
	g_assert_cmpfloat (rgb.green, <, 0.01);
	g_assert_cmpfloat (rgb.blue, <, 0.01);
	g_assert_cmpfloat (rgb.red, >, -0.01);
	g_assert_cmpfloat (rgb.green, >, -0.01);
	g_assert_cmpfloat (rgb.blue, >, -0.01);

	/* white */
	rgb_int.red = 0xff; rgb_int.green = 0xff; rgb_int.blue = 0xff;
	gcm_convert_rgb_int_to_rgb (&rgb_int, &rgb);
	g_assert_cmpfloat (rgb.red, <, 1.01);
	g_assert_cmpfloat (rgb.green, <, 1.01);
	g_assert_cmpfloat (rgb.blue, <, 1.01);
	g_assert_cmpfloat (rgb.red, >, 0.99);
	g_assert_cmpfloat (rgb.green, >, 0.99);
	g_assert_cmpfloat (rgb.blue, >, 0.99);

	/* and back */
	gcm_convert_rgb_to_rgb_int (&rgb, &rgb_int);
	g_assert_cmpint (rgb_int.red, ==, 0xff);
	g_assert_cmpint (rgb_int.green, ==, 0xff);
	g_assert_cmpint (rgb_int.blue, ==, 0xff);

	/* black */
	rgb.red = 0.0f; rgb.green = 0.0f; rgb.blue = 0.0f;
	gcm_convert_rgb_to_rgb_int (&rgb, &rgb_int);
	g_assert_cmpint (rgb_int.red, ==, 0x00);
	g_assert_cmpint (rgb_int.green, ==, 0x00);
	g_assert_cmpint (rgb_int.blue, ==, 0x00);

	/* Yxy -> XYZ */
	Yxy.Y = 21.5;
	Yxy.x = 0.31;
	Yxy.y = 0.32;
	gcm_convert_Yxy_to_XYZ (&Yxy, &XYZ);
	g_assert_cmpfloat (XYZ.X, <, 21.0);
	g_assert_cmpfloat (XYZ.X, >, 20.5);
	g_assert_cmpfloat (XYZ.Y, <, 22.0);
	g_assert_cmpfloat (XYZ.Y, >, 21.0);
	g_assert_cmpfloat (XYZ.Z, <, 25.0);
	g_assert_cmpfloat (XYZ.Z, >, 24.5);

	/* and back */
	gcm_convert_XYZ_to_Yxy (&XYZ, &Yxy);
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
gcm_test_sensor_func (void)
{
	gboolean ret;
	GError *error = NULL;
	GcmSensor *sensor;
	gdouble value;
	GcmColorXYZ values;

	/* start sensor */
	sensor = gcm_sensor_dummy_new ();
	ret = gcm_sensor_startup (sensor, &error);
	g_assert (ret);
	g_assert_no_error (error);

	/* set LEDs */
	ret = gcm_sensor_set_leds (sensor, 0x0f, &error);
	g_assert (ret);
	g_assert_no_error (error);

	/* set mode */
	gcm_sensor_set_output_type (sensor, GCM_SENSOR_OUTPUT_TYPE_LCD);
	g_assert_cmpint (gcm_sensor_get_output_type (sensor), ==, GCM_SENSOR_OUTPUT_TYPE_LCD);

	/* get ambient */
	ret = gcm_sensor_get_ambient (sensor, &value, &error);
	g_assert (ret);
	g_assert_no_error (error);
	g_debug ("ambient = %.1lf Lux", value);

	/* sample color */
	ret = gcm_sensor_sample (sensor, &values, &error);
	g_assert (ret);
	g_assert_no_error (error);
	g_debug ("X=%0.4lf, Y=%0.4lf, Z=%0.4lf", values.X, values.Y, values.Z);

	/* set LEDs */
	ret = gcm_sensor_set_leds (sensor, 0x00, &error);
	g_assert (ret);
	g_assert_no_error (error);

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
	g_test_add_func ("/color/edid", gcm_test_edid_func);
	g_test_add_func ("/color/tables", gcm_test_tables_func);

	return g_test_run ();
}


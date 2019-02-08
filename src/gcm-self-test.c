/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007-2015 Richard Hughes <richard@hughsie.com>
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

#include <glib-object.h>
#include <math.h>
#include <glib/gstdio.h>
#include <stdlib.h>

#include "gcm-brightness.h"
#include "gcm-calibrate.h"
#include "gcm-debug.h"
#include "gcm-exif.h"
#include "gcm-print.h"
#include "gcm-utils.h"

static void
gcm_test_brightness_func (void)
{
	g_autoptr(GcmBrightness) brightness = NULL;
	gboolean ret;
	g_autoptr(GError) error = NULL;
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
}

static void
gcm_test_calibrate_func (void)
{
	gboolean ret;
	g_autoptr(GError) error = NULL;
	g_autoptr(GcmCalibrate) calibrate = NULL;
	g_autofree gchar *model = NULL;
	g_autofree gchar *manufacturer = NULL;

	calibrate = gcm_calibrate_new ();
	g_assert (calibrate != NULL);

	/* calibrate display manually */
	ret = gcm_calibrate_set_from_exif (GCM_CALIBRATE(calibrate), TESTDATADIR "/test.tif", &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* get device properties */
	g_object_get (calibrate,
		      "manufacturer", &manufacturer,
		      "model", &model,
		      NULL);
	g_assert_cmpstr (model, ==, "NIKON D60");
	g_assert_cmpstr (manufacturer, ==, "NIKON CORPORATION");
}

static void
gcm_test_exif_func (void)
{
	gboolean ret;
	g_autoptr(GError) error = NULL;
	GFile *file;
	g_autoptr(GcmExif) exif = NULL;

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
#if 0
	file = g_file_new_for_path (TESTDATADIR "/test.kdc");
	ret = gcm_exif_parse (exif, file, &error);
	g_object_unref (file);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert_cmpstr (gcm_exif_get_model (exif), ==, "Eastman Kodak Company");
	g_assert_cmpstr (gcm_exif_get_manufacturer (exif), ==, "Kodak Digital Science DC50 Zoom Camera");
	g_assert_cmpstr (gcm_exif_get_serial (exif), ==, NULL);
	g_assert_cmpint (gcm_exif_get_device_kind (exif), ==, CD_DEVICE_KIND_CAMERA);
#endif

	/* PNG */
	file = g_file_new_for_path (TESTDATADIR "/test.png");
	ret = gcm_exif_parse (exif, file, &error);
	g_object_unref (file);
	g_assert_error (error, GCM_EXIF_ERROR, GCM_EXIF_ERROR_NO_SUPPORT);
	g_assert (!ret);
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
	g_autoptr(GError) error = NULL;
	g_autoptr(GcmPrint) print = NULL;

	print = gcm_print_new ();

	ret = gcm_print_with_render_callback (print, NULL, gcm_print_test_render_cb, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);
}

static void
gcm_test_utils_func (void)
{
	gboolean ret;
	gchar *text;
	gchar *filename;
	g_autoptr(GFile) file = NULL;
	g_autoptr(GFile) dest = NULL;

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
}

int
main (int argc, char **argv)
{
	gtk_init (&argc, &argv);
	g_test_init (&argc, &argv, NULL);

	/* setup manually as we have no GMainContext */
	gcm_debug_setup (g_getenv ("VERBOSE") != NULL);

	g_test_add_func ("/color/calibrate", gcm_test_calibrate_func);
	g_test_add_func ("/color/exif", gcm_test_exif_func);
	g_test_add_func ("/color/utils", gcm_test_utils_func);
	if (g_test_thorough ())
		g_test_add_func ("/color/brightness", gcm_test_brightness_func);
	if (g_test_slow ())
		g_test_add_func ("/color/print", gcm_test_print_func);

	return g_test_run ();
}


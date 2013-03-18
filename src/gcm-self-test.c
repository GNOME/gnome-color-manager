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

#include "config.h"

#include <glib-object.h>
#include <math.h>
#include <glib/gstdio.h>
#include <stdlib.h>

#include "gcm-brightness.h"
#include "gcm-calibrate.h"
#include "gcm-cie-widget.h"
#include "gcm-clut.h"
#include "gcm-debug.h"
#include "gcm-exif.h"
#include "gcm-gamma-widget.h"
#include "gcm-hull.h"
#include "gcm-image.h"
#include "gcm-named-color.h"
#include "gcm-print.h"
#include "gcm-profile.h"
#include "gcm-trc-widget.h"
#include "gcm-utils.h"

static void
gcm_test_hull_func (void)
{
	GcmHull *hull;
	CdColorXYZ xyz;
	CdColorRGB color;
	guint faces[3];
	gchar *data;

	hull = gcm_hull_new ();
	g_assert (hull != NULL);

	gcm_hull_set_flags (hull, 8);
	g_assert_cmpint (gcm_hull_get_flags (hull), ==, 8);

	/* add a point */
	xyz.X = 1.0;
	xyz.Y = 2.0;
	xyz.Z = 3.0;
	color.R = 0.25;
	color.G = 0.5;
	color.B = 1.0;
	gcm_hull_add_vertex (hull, &xyz, &color);

	/* add another two */
	xyz.Z = 2.0;
	gcm_hull_add_vertex (hull, &xyz, &color);
	xyz.X = 2.0;
	gcm_hull_add_vertex (hull, &xyz, &color);

	/* add a face */
	faces[0] = 0;
	faces[1] = 1;
	faces[2] = 2;
	gcm_hull_add_face (hull, faces, 3);

	/* export to a PLY file */
	data = gcm_hull_export_to_ply (hull);
	g_assert_cmpstr (data, ==, "ply\n"
				   "format ascii 1.0\n"
				   "element vertex 3\n"
				   "property float x\n"
				   "property float y\n"
				   "property float z\n"
				   "property uchar red\n"
				   "property uchar green\n"
				   "property uchar blue\n"
				   "element face 1\n"
				   "property list uchar uint vertex_indices\n"
				   "end_header\n"
				   "1.000000 2.000000 3.000000 63 127 255\n"
				   "1.000000 2.000000 2.000000 63 127 255\n"
				   "2.000000 2.000000 2.000000 63 127 255\n"
				   "3 0 1 2\n");
	g_free (data);

	g_object_unref (hull);
}

static void
gcm_test_profile_func (void)
{
	GcmProfile *profile;
	GFile *file;
	GcmClut *clut;
	gboolean ret;
	GError *error = NULL;
	CdColorXYZ *xyz;
	CdColorYxy yxy;
	CdColorYxy red;
	CdColorYxy green;
	CdColorYxy blue;
	CdColorYxy white;
	GcmHull *hull;
	gchar *data;

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
	cd_color_xyz_to_yxy (xyz, &yxy);
	g_assert_cmpfloat (fabs (yxy.x - 0.648454), <, 0.01);

	cd_color_xyz_free (xyz);
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
	cd_color_yxy_set (&red, 1.0f, 0.569336f, 0.332031f);
	cd_color_yxy_set (&green, 1.0f, 0.311523f, 0.543945f);
	cd_color_yxy_set (&blue, 1.0f, 0.149414f, 0.131836f);
	cd_color_yxy_set (&white, 1.0f, 0.313477f, 0.329102f);

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
	g_assert_cmpint (gcm_profile_get_temperature (profile), ==, 6400);

	/* delete temp file */
	ret = g_file_delete (file, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	g_object_unref (file);
	g_object_unref (profile);

	/* get gamut hull */
	profile = gcm_profile_new ();
	file = g_file_new_for_path (TESTDATADIR "/ibm-t61.icc");
	ret = gcm_profile_parse (profile, file, &error);
	g_assert_no_error (error);
	g_assert (ret);
	hull = gcm_profile_generate_gamut_hull (profile, 12);
	g_assert (hull != NULL);

	/* save as PLY file */
	data = gcm_hull_export_to_ply (hull);
	ret = g_file_set_contents ("/tmp/gamut.ply", data, -1, NULL);
	g_assert (ret);

	g_free (data);
	g_object_unref (hull);
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

	g_object_unref (clut);
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

	/* get device properties */
	g_object_get (calibrate,
		      "manufacturer", &manufacturer,
		      "model", &model,
		      NULL);
	g_assert_cmpstr (model, ==, "NIKON D60");
	g_assert_cmpstr (manufacturer, ==, "NIKON CORPORATION");
	g_free (model);
	g_free (manufacturer);
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
	CdColorXYZ *white;
	CdColorXYZ *red;
	CdColorXYZ *green;
	CdColorXYZ *blue;
	gint response;
	GFile *file = NULL;
	CdColorYxy white_Yxy;
	CdColorYxy red_Yxy;
	CdColorYxy green_Yxy;
	CdColorYxy blue_Yxy;

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

	cd_color_xyz_to_yxy (white, &white_Yxy);
	cd_color_xyz_to_yxy (red, &red_Yxy);
	cd_color_xyz_to_yxy (green, &green_Yxy);
	cd_color_xyz_to_yxy (blue, &blue_Yxy);

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
	cd_color_xyz_free (white);
	cd_color_xyz_free (red);
	cd_color_xyz_free (green);
	cd_color_xyz_free (blue);
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

static void
gcm_test_named_color_func (void)
{
	GcmNamedColor *nc;
	CdColorXYZ *xyz;
	CdColorXYZ *xyz2;
	const CdColorXYZ *xyz_new;
	gchar *tmp = NULL;

	nc = gcm_named_color_new ();

	gcm_named_color_set_title (nc, "Hello world");

	xyz = cd_color_xyz_new ();

	/* use setters */
	cd_color_xyz_set (xyz, 0.1, 0.2, 0.3);
	gcm_named_color_set_value (nc, xyz);

	/* test getters */
	g_assert_cmpstr (gcm_named_color_get_title (nc), ==, "Hello world");
	xyz_new = gcm_named_color_get_value (nc);
	g_assert_cmpfloat (abs (xyz_new->X - 0.1), <, 0.01);
	g_assert_cmpfloat (abs (xyz_new->Y - 0.2), <, 0.01);
	g_assert_cmpfloat (abs (xyz_new->Z - 0.3), <, 0.01);

	/* overwrite using properties */
	cd_color_xyz_set (xyz, 0.4, 0.5, 0.6);
	g_object_set (nc,
		      "title", "dave",
		      "value", xyz,
		      NULL);

	/* test property getters */
	g_object_get (nc,
		      "title", &tmp,
		      "value", &xyz2,
		      NULL);
	g_assert_cmpstr (gcm_named_color_get_title (nc), ==, "dave");
	g_assert_cmpfloat (abs (xyz2->X - 0.4), <, 0.01);
	g_assert_cmpfloat (abs (xyz2->Y - 0.5), <, 0.01);
	g_assert_cmpfloat (abs (xyz2->Z - 0.6), <, 0.01);

	g_free (tmp);
	cd_color_xyz_free (xyz);
	cd_color_xyz_free (xyz2);
}

int
main (int argc, char **argv)
{
	gtk_init (&argc, &argv);
	g_test_init (&argc, &argv, NULL);

	/* setup manually as we have no GMainContext */
	gcm_debug_setup (g_getenv ("VERBOSE") != NULL);

	g_test_add_func ("/color/named-color", gcm_test_named_color_func);
	g_test_add_func ("/color/calibrate", gcm_test_calibrate_func);
	g_test_add_func ("/color/exif", gcm_test_exif_func);
	g_test_add_func ("/color/utils", gcm_test_utils_func);
	g_test_add_func ("/color/hull", gcm_test_hull_func);
	g_test_add_func ("/color/profile", gcm_test_profile_func);
	g_test_add_func ("/color/clut", gcm_test_clut_func);
	if (g_test_thorough ()) {
		g_test_add_func ("/color/brightness", gcm_test_brightness_func);
		g_test_add_func ("/color/image", gcm_test_image_func);
		g_test_add_func ("/color/trc", gcm_test_trc_widget_func);
		g_test_add_func ("/color/cie", gcm_test_cie_widget_func);
		g_test_add_func ("/color/gamma_widget", gcm_test_gamma_widget_func);
	}
	if (g_test_slow ()) {
		g_test_add_func ("/color/print", gcm_test_print_func);
	}

	return g_test_run ();
}


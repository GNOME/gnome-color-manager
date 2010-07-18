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
#include "gcm-cie-widget.h"
#include "gcm-client.h"
#include "gcm-device.h"
#include "gcm-device-udev.h"
#include "gcm-device-xrandr.h"
#include "gcm-exif.h"
#include "gcm-gamma-widget.h"
#include "gcm-image.h"
#include "gcm-print.h"
#include "gcm-profile.h"
#include "gcm-profile-store.h"
#include "gcm-trc-widget.h"
#include "gcm-utils.h"
#include "gcm-xyz.h"
#include "gcm-profile.h"
#include "gcm-xyz.h"

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

static void
gcm_test_calibrate_func (void)
{
	GcmCalibrate *calibrate;
	gboolean ret;
	GError *error = NULL;

	calibrate = gcm_calibrate_new ();
	g_assert (calibrate != NULL);

	/* calibrate display manually */
	ret = gcm_calibrate_set_from_exif (GCM_CALIBRATE(calibrate), TESTDATADIR "/test.tif", &error);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert_cmpstr (gcm_calibrate_get_model_fallback (calibrate), ==, "NIKON D60");
	g_assert_cmpstr (gcm_calibrate_get_manufacturer_fallback (calibrate), ==, "NIKON CORPORATION");

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
gcm_test_calibrate_manual_func (void)
{
	GcmCalibrateManual *calibrate;
	gboolean ret;
	GError *error = NULL;

	calibrate = gcm_calibrate_manual_new ();
	g_assert (calibrate != NULL);

	/* set to avoid a critical warning */
	g_object_set (calibrate,
		      "output-name", "lvds1",
		      NULL);

	/* calibrate display manually */
	ret = gcm_calibrate_display (GCM_CALIBRATE(calibrate), NULL, &error);
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
	GcmXyz *white;
	GcmXyz *red;
	GcmXyz *green;
	GcmXyz *blue;
	gint response;
	GFile *file = NULL;

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

	g_object_set (widget,
		      "red", red,
		      "green", green,
		      "blue", blue,
		      "white", white,
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
	g_object_unref (white);
	g_object_unref (red);
	g_object_unref (green);
	g_object_unref (blue);
}

static guint _changes = 0;

static void
gcm_device_test_changed_cb (GcmDevice *device)
{
	_changes++;
	_g_test_loop_quit ();
}

static void
gcm_test_device_func (void)
{
	GcmDevice *device;
	gboolean ret;
	GError *error = NULL;
	gchar *filename;
	GcmProfile *profile;
	const gchar *profile_filename;
	GPtrArray *profiles;
	gchar *data;
	gchar **split;
	gchar *contents;

	device = gcm_device_udev_new ();
	g_assert (device != NULL);

	/* connect to the changed signal */
	g_signal_connect (device, "changed", G_CALLBACK (gcm_device_test_changed_cb), NULL);

	g_assert_cmpint (_changes, ==, 0);

	g_assert (gcm_device_kind_from_string ("scanner") == GCM_DEVICE_KIND_SCANNER);
	g_assert (gcm_device_kind_from_string ("xxx") == GCM_DEVICE_KIND_UNKNOWN);

	g_assert_cmpstr (gcm_device_kind_to_string (GCM_DEVICE_KIND_SCANNER), ==, "scanner");
	g_assert_cmpstr (gcm_device_kind_to_string (GCM_DEVICE_KIND_UNKNOWN), ==, "unknown");

	/* set some properties */
	g_object_set (device,
		      "kind", GCM_DEVICE_KIND_SCANNER,
		      "id", "sysfs_dummy_device",
		      "connected", FALSE,
		      "virtual", FALSE,
		      "serial", "0123456789",
		      "colorspace", GCM_COLORSPACE_RGB,
		      NULL);

	_g_test_loop_run_with_timeout (1000);

	g_assert_cmpint (_changes, ==, 1);

	gcm_device_set_connected (device, TRUE);

	_g_test_loop_run_with_timeout (1000);

	g_assert_cmpint (_changes, ==, 2);

	g_assert_cmpstr (gcm_device_get_id (device), ==, "sysfs_dummy_device");

	/* ensure the file is nuked */
	filename = gcm_utils_get_default_config_location ();
	g_unlink (filename);

	/* missing file */
	ret = gcm_device_load (device, &error);
	g_assert_no_error (error);
	g_assert (ret);

	g_assert_cmpstr (gcm_device_get_default_profile_filename (device), ==, NULL);

	/* empty file that exists */
	g_file_set_contents (filename, "", -1, NULL);

	/* load from empty file */
	ret = gcm_device_load (device, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* set default file */
	contents = g_strdup_printf ("[sysfs_dummy_device]\n"
				    "title=Canon - CanoScan\n"
				    "type=scanner\n"
				    "profile=%s;%s\n",
				    TESTDATADIR "/bluish.icc",
				    TESTDATADIR "/AdobeGammaTest.icm");
	g_file_set_contents (filename, contents, -1, NULL);

	ret = gcm_device_load (device, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* time out of loop */
	_g_test_loop_run_with_timeout (1000);

	g_assert_cmpint (_changes, ==, 3);

	/* get some properties */
	profile_filename = gcm_device_get_default_profile_filename (device);
	gcm_test_assert_basename (profile_filename, TESTDATADIR "/bluish.icc");
	profiles = gcm_device_get_profiles (device);
	g_assert_cmpint (profiles->len, ==, 2);

	profile = g_ptr_array_index (profiles, 0);
	g_assert (profile != NULL);
	gcm_test_assert_basename (gcm_profile_get_filename (profile), TESTDATADIR "/bluish.icc");

	profile = g_ptr_array_index (profiles, 1);
	g_assert (profile != NULL);
	gcm_test_assert_basename (gcm_profile_get_filename (profile), TESTDATADIR "/AdobeGammaTest.icm");

	/* set some properties */
	gcm_device_set_default_profile_filename (device, TESTDATADIR "/bluish.icc");

	/* ensure the file is nuked, again */
	g_unlink (filename);

	/* save to empty file */
	ret = gcm_device_save (device, &error);
	g_assert_no_error (error);
	g_assert (ret);

	ret = g_file_get_contents (filename, &data, NULL, NULL);
	g_assert (ret);

	split = g_strsplit (data, "\n", -1);
	g_assert_cmpstr (split[1], ==, "[sysfs_dummy_device]");
	g_assert_cmpstr (split[5], ==, "serial=0123456789");
	g_assert_cmpstr (split[6], ==, "type=scanner");
	g_assert_cmpstr (split[7], ==, "colorspace=rgb");
	g_free (data);
	g_strfreev (split);

	/* ensure the file is nuked, in case we are running in distcheck */
	g_unlink (filename);
	g_free (filename);
	g_free (contents);
	g_ptr_array_unref (profiles);

	g_object_unref (device);
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
	g_assert_cmpint (gcm_exif_get_device_kind (exif), ==, GCM_DEVICE_KIND_CAMERA);

	/* JPG */
	file = g_file_new_for_path (TESTDATADIR "/test.jpg");
	ret = gcm_exif_parse (exif, file, &error);
	g_object_unref (file);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert_cmpstr (gcm_exif_get_model (exif), ==, "NIKON D60");
	g_assert_cmpstr (gcm_exif_get_manufacturer (exif), ==, "NIKON CORPORATION");
	g_assert_cmpstr (gcm_exif_get_serial (exif), ==, NULL);
	g_assert_cmpint (gcm_exif_get_device_kind (exif), ==, GCM_DEVICE_KIND_CAMERA);

	/* RAW */
	file = g_file_new_for_path (TESTDATADIR "/test.kdc");
	ret = gcm_exif_parse (exif, file, &error);
	g_object_unref (file);
	g_assert_no_error (error);
	g_assert (ret);
	g_assert_cmpstr (gcm_exif_get_model (exif), ==, "Eastman Kodak Company");
	g_assert_cmpstr (gcm_exif_get_manufacturer (exif), ==, "Kodak Digital Science DC50 Zoom Camera");
	g_assert_cmpstr (gcm_exif_get_serial (exif), ==, NULL);
	g_assert_cmpint (gcm_exif_get_device_kind (exif), ==, GCM_DEVICE_KIND_CAMERA);

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

	/* get default config location (when in make check) */
	g_setenv ("GCM_TEST", "1", TRUE);
	filename = gcm_utils_get_default_config_location ();
	g_assert_cmpstr (filename, ==, "/tmp/device-profiles.conf");
	g_free (filename);

	g_assert (gcm_utils_device_kind_to_profile_kind (GCM_DEVICE_KIND_SCANNER) == GCM_PROFILE_KIND_INPUT_DEVICE);
	g_assert (gcm_utils_device_kind_to_profile_kind (GCM_DEVICE_KIND_UNKNOWN) == GCM_PROFILE_KIND_UNKNOWN);
}

static void
gcm_test_client_func (void)
{
	GcmClient *client;
	GError *error = NULL;
	gboolean ret;
	GPtrArray *array;
	GcmDevice *device;
	gchar *contents;
	gchar *filename;
	gchar *data = NULL;

	client = gcm_client_new ();
	g_assert (client != NULL);

	/* ensure file is gone */
	g_setenv ("GCM_TEST", "1", TRUE);
	filename = gcm_utils_get_default_config_location ();
	g_unlink (filename);

	/* ensure we don't fail with no config file */
	ret = gcm_client_coldplug (client, GCM_CLIENT_COLDPLUG_SAVED, &error);
	g_assert_no_error (error);
	g_assert (ret);

	array = gcm_client_get_devices (client);
	g_assert (array != NULL);
	g_assert_cmpint (array->len, ==, 0);
	g_ptr_array_unref (array);

	/* ensure we get one device */
	contents = g_strdup_printf ("[xrandr_goldstar]\n"
				    "profile=%s\n"
				    "title=Goldstar\n"
				    "type=display\n"
				    "colorspace=rgb\n",
				    TESTDATADIR "/bluish.icc");
	ret = g_file_set_contents (filename, contents, -1, &error);
	g_assert_no_error (error);
	g_assert (ret);

	ret = gcm_client_coldplug (client, GCM_CLIENT_COLDPLUG_SAVED, &error);
	g_assert_no_error (error);
	g_assert (ret);

	array = gcm_client_get_devices (client);
	g_assert (array != NULL);
	g_assert_cmpint (array->len, ==, 1);
	device = g_ptr_array_index (array, 0);
	g_assert_cmpstr (gcm_device_get_id (device), ==, "xrandr_goldstar");
	g_assert_cmpstr (gcm_device_get_title (device), ==, "Goldstar");
	gcm_test_assert_basename (gcm_device_get_default_profile_filename (device), TESTDATADIR "/bluish.icc");
	g_assert (gcm_device_get_saved (device));
	g_assert (!gcm_device_get_connected (device));
	g_assert (GCM_IS_DEVICE_XRANDR (device));
	g_ptr_array_unref (array);

	device = gcm_device_udev_new ();
	gcm_device_set_id (device, "xrandr_goldstar");
	gcm_device_set_title (device, "Slightly different");
	gcm_device_set_connected (device, TRUE);
	ret = gcm_client_add_device (client, device, &error);
	g_assert_no_error (error);
	g_assert (ret);

	/* ensure we merge saved properties into current devices */
	array = gcm_client_get_devices (client);
	g_assert_cmpint (array->len, ==, 1);
	device = g_ptr_array_index (array, 0);
	g_assert_cmpstr (gcm_device_get_id (device), ==, "xrandr_goldstar");
	g_assert_cmpstr (gcm_device_get_title (device), ==, "Slightly different");
	gcm_test_assert_basename (gcm_device_get_default_profile_filename (device), TESTDATADIR "/bluish.icc");
	g_assert (gcm_device_get_saved (device));
	g_assert (gcm_device_get_connected (device));
	g_assert (GCM_IS_DEVICE_UDEV (device));
	g_ptr_array_unref (array);

	/* delete */
	gcm_device_set_connected (device, FALSE);
	ret = gcm_client_delete_device (client, device, &error);
	g_assert_no_error (error);
	g_assert (ret);

	array = gcm_client_get_devices (client);
	g_assert_cmpint (array->len, ==, 0);
	g_ptr_array_unref (array);

	ret = g_file_get_contents (filename, &data, NULL, &error);
	g_assert_no_error (error);
	g_assert (ret);

	g_assert_cmpstr (data, ==, "");

	g_object_unref (client);
	g_object_unref (device);
	g_unlink (filename);
	g_free (contents);
	g_free (filename);
	g_free (data);
}

int
main (int argc, char **argv)
{
	if (! g_thread_supported ())
		g_thread_init (NULL);
	gtk_init (&argc, &argv);
	g_test_init (&argc, &argv, NULL);

	g_test_add_func ("/color/client", gcm_test_client_func);
	g_test_add_func ("/color/calibrate", gcm_test_calibrate_func);
	g_test_add_func ("/color/exif", gcm_test_exif_func);
	g_test_add_func ("/color/utils", gcm_test_utils_func);
	g_test_add_func ("/color/device", gcm_test_device_func);
	g_test_add_func ("/color/calibrate_dialog", gcm_test_calibrate_dialog_func);
	if (g_test_thorough ()) {
		g_test_add_func ("/color/trc", gcm_test_trc_widget_func);
		g_test_add_func ("/color/cie", gcm_test_cie_widget_func);
		g_test_add_func ("/color/gamma_widget", gcm_test_gamma_widget_func);
		g_test_add_func ("/color/image", gcm_test_image_func);
	}
	if (g_test_slow ()) {
		g_test_add_func ("/color/print", gcm_test_print_func);
		g_test_add_func ("/color/calibrate_manual", gcm_test_calibrate_manual_func);
	}

	return g_test_run ();
}


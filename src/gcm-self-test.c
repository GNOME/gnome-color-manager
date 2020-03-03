/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007-2015 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include "config.h"

#include <glib-object.h>
#include <math.h>
#include <glib/gstdio.h>
#include <stdlib.h>

#include "gcm-cie-widget.h"
#include "gcm-debug.h"
#include "gcm-gamma-widget.h"
#include "gcm-trc-widget.h"
#include "gcm-utils.h"

static void
gcm_test_cie_widget_func (void)
{
	GtkWidget *widget;
	GtkWidget *image;
	GtkWidget *dialog;
	GtkWidget *vbox;
	gboolean ret;
	gint response;
	CdColorYxy white_Yxy;
	CdColorYxy red_Yxy;
	CdColorYxy green_Yxy;
	CdColorYxy blue_Yxy;
	g_autoptr(CdColorXYZ) blue = NULL;
	g_autoptr(CdColorXYZ) green = NULL;
	g_autoptr(CdColorXYZ) red = NULL;
	g_autoptr(CdColorXYZ) white = NULL;
	g_autoptr(CdIcc) profile = NULL;
	g_autoptr(GFile) file = NULL;

	widget = gcm_cie_widget_new ();
	g_assert (widget != NULL);

	profile = cd_icc_new ();
	file = g_file_new_for_path (TESTDATADIR "/bluish.icc");
	ret = cd_icc_load_file (profile, file, CD_ICC_LOAD_FLAGS_NONE, NULL, NULL);
	g_assert (ret);
	g_object_get (profile,
		      "white", &white,
		      "red", &red,
		      "green", &green,
		      "blue", &blue,
		      NULL);

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
gcm_test_trc_widget_func (void)
{
	GtkWidget *widget;
	GtkWidget *image;
	GtkWidget *dialog;
	GtkWidget *vbox;
	gboolean ret;
	gint response;
	g_autoptr(CdIcc) profile = NULL;
    g_autoptr(GFile) file = NULL;
	g_autoptr(GPtrArray) clut = NULL;

	widget = gcm_trc_widget_new ();
	g_assert (widget != NULL);

	profile = cd_icc_new ();
	file = g_file_new_for_path (TESTDATADIR "/AdobeGammaTest.icm");
	ret = cd_icc_load_file (profile, file, CD_ICC_LOAD_FLAGS_NONE, NULL, NULL);
	g_assert (ret);
	clut = cd_icc_get_vcgt (profile, 256, NULL);
	g_object_set (widget,
		      "data", clut,
		      NULL);

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
}

static void
gcm_test_utils_func (void)
{
	g_autofree gchar *text = NULL;
	text = gcm_utils_linkify ("http://www.dave.org is text http://www.hughsie.com that needs to be linked to http://www.bbc.co.uk really");
	g_assert_cmpstr (text, ==, "<a href=\"http://www.dave.org\">http://www.dave.org</a> is text "
			     "<a href=\"http://www.hughsie.com\">http://www.hughsie.com</a> that needs to be linked to "
			     "<a href=\"http://www.bbc.co.uk\">http://www.bbc.co.uk</a> really");
}

int
main (int argc, char **argv)
{
	gtk_init (&argc, &argv);
	g_test_init (&argc, &argv, NULL);

	/* setup manually as we have no GMainContext */
	gcm_debug_setup (g_getenv ("VERBOSE") != NULL);

	g_test_add_func ("/color/utils", gcm_test_utils_func);
	if (g_test_thorough ()) {
		g_test_add_func ("/color/trc", gcm_test_trc_widget_func);
		g_test_add_func ("/color/cie", gcm_test_cie_widget_func);
		g_test_add_func ("/color/gamma_widget", gcm_test_gamma_widget_func);
	}

	return g_test_run ();
}


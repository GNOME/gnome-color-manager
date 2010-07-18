/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
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

#include <sys/types.h>
#include <unistd.h>
#include <glib-object.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <locale.h>
#include <gtk/gtk.h>
#include <lcms2.h>

#include "egg-debug.h"

#include "gcm-calibrate-argyll.h"
#include "gcm-colorimeter.h"
#include "gcm-profile-store.h"
#include "gcm-utils.h"
#include "gcm-xyz.h"

static GtkBuilder *builder = NULL;
static GtkWidget *info_bar_hardware = NULL;
static GtkWidget *info_bar_hardware_label = NULL;
static GcmCalibrate *calibrate = NULL;
static GcmProfileStore *profile_store = NULL;
static const gchar *profile_filename = NULL;
static gboolean done_measure = FALSE;

enum {
	GCM_PREFS_COMBO_COLUMN_TEXT,
	GCM_PREFS_COMBO_COLUMN_PROFILE,
	GCM_PREFS_COMBO_COLUMN_LAST
};

/**
 * gcm_picker_set_pixbuf_color:
 **/
static void
gcm_picker_set_pixbuf_color (GdkPixbuf *pixbuf, gchar red, gchar green, gchar blue)
{
	gint x, y;
	gint width, height, rowstride, n_channels;
	guchar *pixels, *p;

	n_channels = gdk_pixbuf_get_n_channels (pixbuf);
	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);
	rowstride = gdk_pixbuf_get_rowstride (pixbuf);
	pixels = gdk_pixbuf_get_pixels (pixbuf);

	/* set to all the correct colors */
	for (y=0; y<height; y++) {
		for (x=0; x<width; x++) {
			p = pixels + y * rowstride + x * n_channels;
			p[0] = red;
			p[1] = green;
			p[2] = blue;
		}
	}
}

/**
 * gcm_picker_measure_cb:
 **/
static void
gcm_picker_measure_cb (GtkWidget *widget, gpointer data)
{
	GtkWindow *window;
	gboolean ret;
	GError *error = NULL;

	/* reset the image */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "image_preview"));
	gtk_image_set_from_file (GTK_IMAGE (widget), DATADIR "/icons/hicolor/64x64/apps/gnome-color-manager.png");

	/* get value */
	window = GTK_WINDOW (gtk_builder_get_object (builder, "dialog_picker"));
	ret = gcm_calibrate_spotread (calibrate, window, &error);
	if (!ret) {
		egg_warning ("failed to get spot color: %s", error->message);
		g_error_free (error);
	}
}

/**
 * gcm_picker_refresh_results:
 **/
static void
gcm_picker_refresh_results (void)
{
	GcmXyz *xyz = NULL;
	GtkImage *image;
	GtkLabel *label;
	GdkPixbuf *pixbuf = NULL;
	gdouble color_xyz[3];
	guint8 color_rgb[3];
	gdouble color_lab[3];
	gdouble color_error[3];
	gchar *text_xyz = NULL;
	gchar *text_lab = NULL;
	gchar *text_rgb = NULL;
	gchar *text_error = NULL;
	cmsHPROFILE profile_xyz;
	cmsHPROFILE profile_rgb;
	cmsHPROFILE profile_lab;
	cmsHTRANSFORM transform_rgb;
	cmsHTRANSFORM transform_lab;
	cmsHTRANSFORM transform_error;

	/* get new value */
	g_object_get (calibrate, "xyz", &xyz, NULL);

	/* create new pixbuf of the right size */
	pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, 200, 200);

	/* get values */
	g_object_get (xyz,
		      "cie-x", &color_xyz[0],
		      "cie-y", &color_xyz[1],
		      "cie-z", &color_xyz[2],
		      NULL);

	/* lcms scales these for some reason */
	color_xyz[0] /= 100.0f;
	color_xyz[1] /= 100.0f;
	color_xyz[2] /= 100.0f;

	/* get profiles */
	profile_xyz = cmsCreateXYZProfile ();
	profile_rgb = cmsOpenProfileFromFile (profile_filename, "r");
	profile_lab = cmsCreateLab4Profile (cmsD50_xyY ());

	/* create transforms */
	transform_rgb = cmsCreateTransform (profile_xyz, TYPE_XYZ_DBL, profile_rgb, TYPE_RGB_8, INTENT_PERCEPTUAL, 0);
	transform_lab = cmsCreateTransform (profile_xyz, TYPE_XYZ_DBL, profile_lab, TYPE_Lab_DBL, INTENT_PERCEPTUAL, 0);
	transform_error = cmsCreateTransform (profile_rgb, TYPE_RGB_8, profile_xyz, TYPE_XYZ_DBL, INTENT_PERCEPTUAL, 0);

	cmsDoTransform (transform_rgb, color_xyz, color_rgb, 1);
	cmsDoTransform (transform_lab, color_xyz, color_lab, 1);
	cmsDoTransform (transform_error, color_rgb, color_error, 1);

	/* destroy lcms state */
	cmsDeleteTransform (transform_rgb);
	cmsDeleteTransform (transform_lab);
	cmsDeleteTransform (transform_error);
	cmsCloseProfile (profile_xyz);
	cmsCloseProfile (profile_rgb);
	cmsCloseProfile (profile_lab);

	/* set XYZ */
	label = GTK_LABEL (gtk_builder_get_object (builder, "label_xyz"));
	text_xyz = g_strdup_printf ("%.3f, %.3f, %.3f", color_xyz[0], color_xyz[1], color_xyz[2]);
	gtk_label_set_label (label, text_xyz);

	/* set LAB */
	label = GTK_LABEL (gtk_builder_get_object (builder, "label_lab"));
	text_lab = g_strdup_printf ("%.3f, %.3f, %.3f", color_lab[0], color_lab[1], color_lab[2]);
	gtk_label_set_label (label, text_lab);

	/* set RGB */
	label = GTK_LABEL (gtk_builder_get_object (builder, "label_rgb"));
	text_rgb = g_strdup_printf ("%i, %i, %i (#%02X%02X%02X)",
				    color_rgb[0], color_rgb[1], color_rgb[2],
				    color_rgb[0], color_rgb[1], color_rgb[2]);
	gtk_label_set_label (label, text_rgb);
	gcm_picker_set_pixbuf_color (pixbuf, color_rgb[0], color_rgb[1], color_rgb[2]);

	/* set error */
	label = GTK_LABEL (gtk_builder_get_object (builder, "label_error"));
	text_error = g_strdup_printf ("%.1f%%, %.1f%%, %.1f%%",
				      ABS ((color_error[0] - color_xyz[0]) / color_xyz[0] * 100),
				      ABS ((color_error[1] - color_xyz[1]) / color_xyz[1] * 100),
				      ABS ((color_error[2] - color_xyz[2]) / color_xyz[2] * 100));
	gtk_label_set_label (label, text_error);

	/* set image */
	image = GTK_IMAGE (gtk_builder_get_object (builder, "image_preview"));
	gtk_image_set_from_pixbuf (image, pixbuf);

	g_free (text_xyz);
	g_free (text_lab);
	g_free (text_rgb);
	g_free (text_error);
	if (xyz != NULL)
		g_object_unref (xyz);
	if (pixbuf != NULL)
		g_object_unref (pixbuf);
}

/**
 * gcm_picker_xyz_notify_cb:
 **/
static void
gcm_picker_xyz_notify_cb (GcmCalibrate *calibrate_, GParamSpec *pspec, gpointer user_data)
{
	GtkWidget *widget;

	/* set expanded */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "expander_results"));
	gtk_expander_set_expanded (GTK_EXPANDER (widget), TRUE);
	gtk_widget_set_sensitive (widget, TRUE);

	/* we've got results so make sure it's sensitive */
	done_measure = TRUE;

	gcm_picker_refresh_results ();
}

/**
 * gcm_picker_close_cb:
 **/
static void
gcm_picker_close_cb (GtkWidget *widget, gpointer data)
{
	GtkApplication *application = (GtkApplication *) data;
	gtk_application_quit (application);
}

/**
 * gcm_picker_help_cb:
 **/
static void
gcm_picker_help_cb (GtkWidget *widget, gpointer data)
{
	gcm_gnome_help ("picker");
}

/**
 * gcm_picker_delete_event_cb:
 **/
static gboolean
gcm_picker_delete_event_cb (GtkWidget *widget, GdkEvent *event, gpointer data)
{
	gcm_picker_close_cb (widget, data);
	return FALSE;
}

/**
 * gcm_picker_colorimeter_setup_ui:
 **/
static void
gcm_picker_colorimeter_setup_ui (GcmColorimeter *colorimeter)
{
	gboolean present;
	gboolean supports_spot;
	gboolean ret = FALSE;
	GtkWidget *widget;

	/* no present */
	present = gcm_colorimeter_get_present (colorimeter);
	if (!present) {
		/* TRANSLATORS: this is displayed the user has not got suitable hardware */
		gtk_label_set_label (GTK_LABEL (info_bar_hardware_label), _("No colorimeter is attached."));
		goto out;
	}

	/* no support */
	supports_spot = gcm_sensor_supports_spot (gcm_colorimeter_get_sensor (colorimeter));
	ret = (present && supports_spot);

	/* change the label */
	if (present && !supports_spot) {
		/* TRANSLATORS: this is displayed the user has not got suitable hardware */
		gtk_label_set_label (GTK_LABEL (info_bar_hardware_label), _("The attached colorimeter is not capable of reading a spot color."));
	} else if (!present) {
	}

out:
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "button_measure"));
	gtk_widget_set_sensitive (widget, ret);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "expander_results"));
	gtk_widget_set_sensitive (widget, ret && done_measure);
	gtk_widget_set_visible (info_bar_hardware, !ret);
}

/**
 * gcm_picker_colorimeter_changed_cb:
 **/
static void
gcm_picker_colorimeter_changed_cb (GcmColorimeter *colorimeter, gpointer user_data)
{
	gcm_picker_colorimeter_setup_ui (colorimeter);
}

/**
 * gcm_window_set_parent_xid:
 **/
static void
gcm_window_set_parent_xid (GtkWindow *window, guint32 xid)
{
	GdkDisplay *display;
	GdkWindow *parent_window;
	GdkWindow *our_window;

	display = gdk_display_get_default ();
	parent_window = gdk_window_foreign_new_for_display (display, xid);
	our_window = gtk_widget_get_window (GTK_WIDGET (window));

	/* set this above our parent */
	gtk_window_set_modal (window, TRUE);
	gdk_window_set_transient_for (our_window, parent_window);
}

/**
 * gcm_picker_error_cb:
 **/
static void
gcm_picker_error_cb (cmsContext ContextID, cmsUInt32Number errorcode, const char *text)
{
	egg_warning ("LCMS error %i: %s", errorcode, text);
}

/**
 * gcm_prefs_space_combo_changed_cb:
 **/
static void
gcm_prefs_space_combo_changed_cb (GtkWidget *widget, gpointer data)
{
	gboolean ret;
	GtkTreeIter iter;
	GtkTreeModel *model;
	GcmProfile *profile = NULL;

	/* no selection */
	ret = gtk_combo_box_get_active_iter (GTK_COMBO_BOX(widget), &iter);
	if (!ret)
		goto out;

	/* get profile */
	model = gtk_combo_box_get_model (GTK_COMBO_BOX(widget));
	gtk_tree_model_get (model, &iter,
			    GCM_PREFS_COMBO_COLUMN_PROFILE, &profile,
			    -1);
	if (profile == NULL)
		goto out;

	profile_filename = gcm_profile_get_filename (profile);
	egg_debug ("changed picker space %s", profile_filename);
	gcm_picker_refresh_results ();
out:
	if (profile != NULL)
		g_object_unref (profile);
}

/**
 * gcm_prefs_set_combo_simple_text:
 **/
static void
gcm_prefs_set_combo_simple_text (GtkWidget *combo_box)
{
	GtkCellRenderer *renderer;
	GtkListStore *store;

	store = gtk_list_store_new (2, G_TYPE_STRING, GCM_TYPE_PROFILE);
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store), GCM_PREFS_COMBO_COLUMN_TEXT, GTK_SORT_ASCENDING);
	gtk_combo_box_set_model (GTK_COMBO_BOX (combo_box), GTK_TREE_MODEL (store));
	g_object_unref (store);

	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer,
		      "ellipsize", PANGO_ELLIPSIZE_END,
		      "wrap-mode", PANGO_WRAP_WORD_CHAR,
		      "width-chars", 60,
		      NULL);
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo_box), renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo_box), renderer,
					"text", GCM_PREFS_COMBO_COLUMN_TEXT,
					NULL);
}

/**
 * gcm_prefs_combobox_add_profile:
 **/
static void
gcm_prefs_combobox_add_profile (GtkWidget *widget, GcmProfile *profile, GtkTreeIter *iter)
{
	GtkTreeModel *model;
	GtkTreeIter iter_tmp;
	const gchar *description;

	/* iter is optional */
	if (iter == NULL)
		iter = &iter_tmp;


	/* also add profile */
	model = gtk_combo_box_get_model (GTK_COMBO_BOX(widget));
	description = gcm_profile_get_description (profile);
	gtk_list_store_append (GTK_LIST_STORE(model), iter);
	gtk_list_store_set (GTK_LIST_STORE(model), iter,
			    GCM_PREFS_COMBO_COLUMN_TEXT, description,
			    GCM_PREFS_COMBO_COLUMN_PROFILE, profile,
			    -1);
}

/**
 * gcm_prefs_setup_space_combobox:
 **/
static void
gcm_prefs_setup_space_combobox (GtkWidget *widget)
{
	GcmProfile *profile;
	guint i;
	const gchar *filename;
	GcmColorspace colorspace;
	gboolean has_profile = FALSE;
	gboolean has_vcgt;
	gboolean has_colorspace_description;
	gchar *text = NULL;
	GPtrArray *profile_array = NULL;
	GtkTreeIter iter;

	/* get new list */
	profile_array = gcm_profile_store_get_array (profile_store);

	/* update each list */
	for (i=0; i<profile_array->len; i++) {
		profile = g_ptr_array_index (profile_array, i);

		/* only for correct kind */
		has_vcgt = gcm_profile_get_has_vcgt (profile);
		has_colorspace_description = gcm_profile_has_colorspace_description (profile);
		colorspace = gcm_profile_get_colorspace (profile);
		if (!has_vcgt && has_colorspace_description &&
		    colorspace == GCM_COLORSPACE_RGB) {
			gcm_prefs_combobox_add_profile (widget, profile, &iter);

			/* set active option */
			filename = gcm_profile_get_filename (profile);
			if (g_strcmp0 (filename, profile_filename) == 0)
				gtk_combo_box_set_active_iter (GTK_COMBO_BOX (widget), &iter);
			has_profile = TRUE;
		}
	}
	if (!has_profile) {
		/* TRANSLATORS: this is when there are no profiles that can be used; the search term is either "RGB" or "CMYK" */
		text = g_strdup_printf (_("No %s color spaces available"),
					gcm_colorspace_to_localised_string (GCM_COLORSPACE_RGB));
		gtk_combo_box_append_text (GTK_COMBO_BOX(widget), text);
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
		gtk_widget_set_sensitive (widget, FALSE);
	}
	if (profile_array != NULL)
		g_ptr_array_unref (profile_array);
	g_free (text);
}

/**
 * main:
 **/
int
main (int argc, char *argv[])
{
	GOptionContext *context;
	guint retval = 0;
	GError *error = NULL;
	GtkApplication *application;
	GtkWidget *main_window;
	GtkWidget *widget;
	guint xid = 0;
	GcmColorimeter *colorimeter = NULL;

	const GOptionEntry options[] = {
		{ "parent-window", 'p', 0, G_OPTION_ARG_INT, &xid,
		  /* TRANSLATORS: we can make this modal (stay on top of) another window */
		  _("Set the parent window to make this modal"), NULL },
		{ NULL}
	};

	/* setup translations */
	setlocale (LC_ALL, "");
	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	/* setup type system */
	g_type_init ();

	/* setup LCMS */
	cmsSetLogErrorHandler (gcm_picker_error_cb);

	context = g_option_context_new (NULL);
	/* TRANSLATORS: tool that is used to pick colors */
	g_option_context_set_summary (context, _("GNOME Color Manager Color Picker"));
	g_option_context_add_group (context, egg_debug_get_option_group ());
	g_option_context_add_group (context, gtk_get_option_group (TRUE));
	g_option_context_add_main_entries (context, options, NULL);
	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);

	/* ensure single instance */
	application = gtk_application_new ("org.gnome.ColorManager.Picker", &argc, &argv);

	/* get UI */
	builder = gtk_builder_new ();
	retval = gtk_builder_add_from_file (builder, GCM_DATA "/gcm-picker.ui", &error);
	if (retval == 0) {
		egg_warning ("failed to load ui: %s", error->message);
		g_error_free (error);
		goto out;
	}

	main_window = GTK_WIDGET (gtk_builder_get_object (builder, "dialog_picker"));
	gtk_application_add_window (application, GTK_WINDOW (main_window));
	gtk_window_set_icon_name (GTK_WINDOW (main_window), GCM_STOCK_ICON);
	g_signal_connect (main_window, "delete_event",
			  G_CALLBACK (gcm_picker_delete_event_cb), application);

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "button_close"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gcm_picker_close_cb), application);

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "button_help"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gcm_picker_help_cb), NULL);

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "button_measure"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gcm_picker_measure_cb), NULL);

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "image_preview"));
	gtk_widget_set_size_request (widget, 200, 200);

	/* add application specific icons to search path */
	gtk_icon_theme_append_search_path (gtk_icon_theme_get_default (),
	                                   GCM_DATA G_DIR_SEPARATOR_S "icons");

	/* use the color device */
	colorimeter = gcm_colorimeter_new ();
	g_signal_connect (colorimeter, "changed", G_CALLBACK (gcm_picker_colorimeter_changed_cb), NULL);

	/* set the parent window if it is specified */
	if (xid != 0) {
		egg_debug ("Setting xid %i", xid);
		gcm_window_set_parent_xid (GTK_WINDOW (main_window), xid);
	}

	/* use argyll */
	calibrate = GCM_CALIBRATE (gcm_calibrate_argyll_new ());
	g_signal_connect (calibrate, "notify::xyz",
			  G_CALLBACK (gcm_picker_xyz_notify_cb), NULL);

	/* use an info bar if there is no device, or the wrong device */
	info_bar_hardware = gtk_info_bar_new ();
	info_bar_hardware_label = gtk_label_new (NULL);
	gtk_info_bar_set_message_type (GTK_INFO_BAR(info_bar_hardware), GTK_MESSAGE_INFO);
	widget = gtk_info_bar_get_content_area (GTK_INFO_BAR(info_bar_hardware));
	gtk_container_add (GTK_CONTAINER(widget), info_bar_hardware_label);
	gtk_widget_show (info_bar_hardware_label);

	/* add infobar to devices pane */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "vbox1"));
	gtk_box_pack_start (GTK_BOX(widget), info_bar_hardware, FALSE, FALSE, 0);

	/* disable some ui if no hardware */
	gcm_picker_colorimeter_setup_ui (colorimeter);

	/* maintain a list of profiles */
	profile_store = gcm_profile_store_new ();

	/* default to AdobeRGB */
	profile_filename = "/usr/share/color/icc/Argyll/ClayRGB1998.icm";

	/* setup RGB combobox */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "combobox_colorspace"));
	gcm_prefs_set_combo_simple_text (widget);
	gcm_prefs_setup_space_combobox (widget);
	g_signal_connect (G_OBJECT (widget), "changed",
			  G_CALLBACK (gcm_prefs_space_combo_changed_cb), NULL);

	/* setup results expander */
	gcm_picker_refresh_results ();

	/* setup initial preview window */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "image_preview"));
	gtk_image_set_from_file (GTK_IMAGE (widget), DATADIR "/icons/hicolor/64x64/apps/gnome-color-manager.png");

	/* wait */
	gtk_widget_show (main_window);
	gtk_application_run (application);

out:
	g_object_unref (application);
	if (profile_store != NULL)
		g_object_unref (profile_store);
	if (colorimeter != NULL)
		g_object_unref (colorimeter);
	if (calibrate != NULL)
		g_object_unref (calibrate);
	if (builder != NULL)
		g_object_unref (builder);
	return retval;
}


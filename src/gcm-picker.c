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
#include <gdk/gdkx.h>
#include <lcms2.h>
#include <colord.h>

#include "gcm-utils.h"
#include "gcm-debug.h"

static CdClient *client = NULL;
static const gchar *profile_filename = NULL;
static gboolean done_measure = FALSE;
static CdColorXYZ last_sample;
static CdSensor *sensor = NULL;
static gdouble last_ambient = -1.0f;
static GtkBuilder *builder = NULL;
static GtkWidget *info_bar_hardware_label = NULL;
static GtkWidget *info_bar_hardware = NULL;
static guint xid = 0;
static guint unlock_timer = 0;

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
 * gcm_picker_refresh_results:
 **/
static void
gcm_picker_refresh_results (void)
{
	cmsCIExyY xyY;
	cmsHPROFILE profile_lab;
	cmsHPROFILE profile_rgb;
	cmsHPROFILE profile_xyz;
	cmsHTRANSFORM transform_error;
	cmsHTRANSFORM transform_lab;
	cmsHTRANSFORM transform_rgb;
	gboolean ret;
	gchar *text_ambient = NULL;
	gchar *text_error = NULL;
	gchar *text_lab = NULL;
	gchar *text_rgb = NULL;
	gchar *text_temperature = NULL;
	gchar *text_whitepoint = NULL;
	gchar *text_xyz = NULL;
	CdColorLab color_lab;
	CdColorRGB8 color_rgb;
	CdColorXYZ color_error;
	CdColorXYZ color_xyz;
	GdkPixbuf *pixbuf = NULL;
	gdouble temperature = 0.0f;
	GtkImage *image;
	GtkLabel *label;

	/* nothing set yet */
	if (profile_filename == NULL)
		goto out;

	/* copy as we're modifying the value */
	cd_color_xyz_copy (&last_sample, &color_xyz);

	/* create new pixbuf of the right size */
	image = GTK_IMAGE (gtk_builder_get_object (builder, "image_preview"));
	pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8,
				 gtk_widget_get_allocated_width (GTK_WIDGET (image)),
				 gtk_widget_get_allocated_height (GTK_WIDGET (image)));

	/* lcms scales these for some reason */
	color_xyz.X /= 100.0f;
	color_xyz.Y /= 100.0f;
	color_xyz.Z /= 100.0f;

	/* get profiles */
	profile_xyz = cmsCreateXYZProfile ();
	profile_rgb = cmsOpenProfileFromFile (profile_filename, "r");
	profile_lab = cmsCreateLab4Profile (cmsD50_xyY ());

	/* create transforms */
	transform_rgb = cmsCreateTransform (profile_xyz, TYPE_XYZ_DBL,
					    profile_rgb, TYPE_RGB_8,
					    INTENT_PERCEPTUAL, 0);
	if (transform_rgb == NULL)
		goto out;
	transform_lab = cmsCreateTransform (profile_xyz, TYPE_XYZ_DBL,
					    profile_lab, TYPE_Lab_DBL,
					    INTENT_PERCEPTUAL, 0);
	if (transform_lab == NULL)
		goto out;
	transform_error = cmsCreateTransform (profile_rgb, TYPE_RGB_8,
					      profile_xyz, TYPE_XYZ_DBL,
					      INTENT_PERCEPTUAL, 0);
	if (transform_error == NULL)
		goto out;

	cmsDoTransform (transform_rgb, &color_xyz, &color_rgb, 1);
	cmsDoTransform (transform_lab, &color_xyz, &color_lab, 1);
	cmsDoTransform (transform_error, &color_rgb, &color_error, 1);

	/* destroy lcms state */
	cmsDeleteTransform (transform_rgb);
	cmsDeleteTransform (transform_lab);
	cmsDeleteTransform (transform_error);
	cmsCloseProfile (profile_xyz);
	cmsCloseProfile (profile_rgb);
	cmsCloseProfile (profile_lab);

	/* set XYZ */
	label = GTK_LABEL (gtk_builder_get_object (builder, "label_xyz"));
	text_xyz = g_strdup_printf ("%.3f, %.3f, %.3f",
				    last_sample.X,
				    last_sample.Y,
				    last_sample.Z);
	gtk_label_set_label (label, text_xyz);

	/* set LAB */
	label = GTK_LABEL (gtk_builder_get_object (builder, "label_lab"));
	text_lab = g_strdup_printf ("%.3f, %.3f, %.3f",
				    color_lab.L,
				    color_lab.a,
				    color_lab.b);
	gtk_label_set_label (label, text_lab);

	/* set whitepoint */
	cmsXYZ2xyY (&xyY, (cmsCIEXYZ *)&last_sample);
	label = GTK_LABEL (gtk_builder_get_object (builder, "label_whitepoint"));
	text_whitepoint = g_strdup_printf ("%.3f,%.3f [%.3f]",
					   xyY.x, xyY.y, xyY.Y);
	gtk_label_set_label (label, text_whitepoint);

	/* set temperature */
	ret = cmsTempFromWhitePoint (&temperature, &xyY);
	if (ret) {
		/* round to nearest 10K */
		temperature = (((guint) temperature) / 10) * 10;
	}
	label = GTK_LABEL (gtk_builder_get_object (builder, "label_temperature"));
	text_temperature = g_strdup_printf ("%.0fK", temperature);
	gtk_label_set_label (label, text_temperature);

	/* set RGB */
	label = GTK_LABEL (gtk_builder_get_object (builder, "label_rgb"));
	text_rgb = g_strdup_printf ("%i, %i, %i (#%02X%02X%02X)",
				    color_rgb.R, color_rgb.G, color_rgb.B,
				    color_rgb.R, color_rgb.G, color_rgb.B);
	gtk_label_set_label (label, text_rgb);
	gcm_picker_set_pixbuf_color (pixbuf, color_rgb.R, color_rgb.G, color_rgb.B);

	/* set error */
	label = GTK_LABEL (gtk_builder_get_object (builder, "label_error"));
	if (color_xyz.X > 0.01f &&
	    color_xyz.Y > 0.01f &&
	    color_xyz.Z > 0.01f) {
		text_error = g_strdup_printf ("%.1f%%, %.1f%%, %.1f%%",
					      ABS ((color_error.X - color_xyz.X) / color_xyz.X * 100),
					      ABS ((color_error.Y - color_xyz.Y) / color_xyz.Y * 100),
					      ABS ((color_error.Z - color_xyz.Z) / color_xyz.Z * 100));
		gtk_label_set_label (label, text_error);
	} else {
		/* TRANSLATORS: this is when the error is invalid */
		gtk_label_set_label (label, _("Unknown"));
	}

	/* set ambient */
	label = GTK_LABEL (gtk_builder_get_object (builder, "label_ambient"));
	if (last_ambient < 0) {
		/* TRANSLATORS: this is when the ambient light level is unknown */
		gtk_label_set_label (label, _("Unknown"));
	} else {
		text_ambient = g_strdup_printf ("%.1f Lux", last_ambient);
		gtk_label_set_label (label, text_ambient);
	}

	/* set image */
	gtk_image_set_from_pixbuf (image, pixbuf);
out:
	g_free (text_ambient);
	g_free (text_error);
	g_free (text_lab);
	g_free (text_rgb);
	g_free (text_temperature);
	g_free (text_whitepoint);
	g_free (text_xyz);
	if (pixbuf != NULL)
		g_object_unref (pixbuf);
}

/**
 * gcm_picker_got_results:
 **/
static void
gcm_picker_got_results (void)
{
	GtkWidget *widget;

	/* set expanded */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "expander_results"));
	gtk_expander_set_expanded (GTK_EXPANDER (widget), TRUE);
	gtk_widget_set_sensitive (widget, TRUE);

	/* we've got results so make sure it's sensitive */
	done_measure = TRUE;
}

/**
 * gcm_picker_unlock_timeout_cb:
 **/
static gboolean
gcm_picker_unlock_timeout_cb (gpointer user_data)
{
	gboolean ret;
	GError *error = NULL;

	/* unlock */
	ret = cd_sensor_unlock_sync (sensor, NULL, &error);
	if (!ret) {
		g_warning ("failed to unlock: %s", error->message);
		g_error_free (error);
	}
	unlock_timer = 0;
	return FALSE;
}

/**
 * gcm_picker_measure_cb:
 **/
static void
gcm_picker_measure_cb (GtkWidget *widget, gpointer data)
{
	gboolean ret;
	CdColorXYZ *tmp = NULL;
	GError *error = NULL;

	/* reset the image */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "image_preview"));
	gtk_image_set_from_file (GTK_IMAGE (widget), DATADIR "/icons/hicolor/64x64/apps/gnome-color-manager.png");

	/* lock */
	if (!cd_sensor_get_locked (sensor)) {
		ret = cd_sensor_lock_sync (sensor,
					   NULL,
					   &error);
		if (!ret) {
			g_warning ("failed to lock: %s", error->message);
			g_error_free (error);
			goto out;
		}
	}

	/* cancel pending unlock */
	if (unlock_timer != 0) {
		g_source_remove (unlock_timer);
		unlock_timer = 0;
	}

	/* get color */
	tmp = cd_sensor_get_sample_sync (sensor,
					 CD_SENSOR_CAP_LCD,
					 NULL,
					 &error);
	if (tmp == NULL) {
		g_warning ("failed to get sample: %s", error->message);
		g_clear_error (&error);
		goto out_unlock;
	}
	cd_color_xyz_copy (tmp, &last_sample);
#if 0
	/* get ambient */
	ret = cd_sensor_get_sample_sync (sensor,
					 CD_SENSOR_CAP_AMBIENT,
					 NULL,
					 &last_ambient,
					 NULL,
					 &error);
	if (!ret) {
		g_warning ("failed to get ambient: %s", error->message);
		g_clear_error (&error);
		goto out_unlock;
	}
#endif
out_unlock:
	/* unlock after a small delay */
	unlock_timer = g_timeout_add_seconds (30, gcm_picker_unlock_timeout_cb, data);
	gcm_picker_refresh_results ();
	gcm_picker_got_results ();
out:
	if (tmp != NULL)
		cd_color_xyz_free (tmp);
}

/**
 * gcm_picker_close_cb:
 **/
static void
gcm_picker_close_cb (GtkWidget *widget, gpointer data)
{
	GtkApplication *application = (GtkApplication *) data;
	g_application_release (G_APPLICATION (application));
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
 * gcm_picker_sensor_client_setup_ui:
 **/
static void
gcm_picker_sensor_client_setup_ui (void)
{
	gboolean ret = FALSE;
	GtkWidget *widget;
	GPtrArray *sensors;
	GError *error = NULL;

	/* no present */
	sensors = cd_client_get_sensors_sync (client, NULL, &error);
	if (sensors == NULL) {
		g_warning ("%s", error->message);
		g_error_free (error);
		goto out;
	}
	if (sensors->len == 0) {
		/* TRANSLATORS: this is displayed the user has not got suitable hardware */
		gtk_label_set_label (GTK_LABEL (info_bar_hardware_label),
				    _("No colorimeter is attached."));
		goto out;
	}
	sensor = g_object_ref (g_ptr_array_index (sensors, 0));

	/* connect to the profile */
	ret = cd_sensor_connect_sync (sensor, NULL, &error);
	if (!ret) {
		g_warning ("failed to connect to sensor: %s",
			   error->message);
		g_error_free (error);
		goto out;
	}

	if (!cd_sensor_get_native (sensor)) {
		 /* TRANSLATORS: this is displayed if VTE support is not enabled */
		gtk_label_set_label (GTK_LABEL (info_bar_hardware_label),
				     _("The sensor has no native driver."));
		goto out;
	}

#if 0
	/* no support */
	ret = cd_sensor_supports_spot (sensor);
	if (!ret) {
		/* TRANSLATORS: this is displayed the user has not got suitable hardware */
		gtk_label_set_label (GTK_LABEL (info_bar_hardware_label), _("The attached colorimeter is not capable of reading a spot color."));
		goto out;
	}
#else
	ret = TRUE;
#endif

out:
	if (sensors != NULL)
		g_ptr_array_unref (sensors);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "button_measure"));
	gtk_widget_set_sensitive (widget, ret);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "expander_results"));
	gtk_widget_set_sensitive (widget, ret && done_measure);
	gtk_widget_set_visible (info_bar_hardware, !ret);
}

/**
 * gcm_picker_sensor_client_changed_cb:
 **/
static void
gcm_picker_sensor_client_changed_cb (CdClient *_client,
				     CdSensor *_sensor,
				     gpointer user_data)
{
	gcm_picker_sensor_client_setup_ui ();
}

/**
 * gcm_window_set_parent_xid:
 **/
static void
gcm_window_set_parent_xid (GtkWindow *window, guint32 _xid)
{
	GdkDisplay *display;
	GdkWindow *parent_window;
	GdkWindow *our_window;

	display = gdk_display_get_default ();
	parent_window = gdk_x11_window_foreign_new_for_display (display, _xid);
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
	g_warning ("LCMS error %i: %s", errorcode, text);
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
	CdProfile *profile = NULL;

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

	profile_filename = cd_profile_get_filename (profile);
	g_debug ("changed picker space %s", profile_filename);

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

	store = gtk_list_store_new (2, G_TYPE_STRING, CD_TYPE_PROFILE);
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store),
					      GCM_PREFS_COMBO_COLUMN_TEXT, GTK_SORT_ASCENDING);
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
gcm_prefs_combobox_add_profile (GtkWidget *widget, CdProfile *profile, GtkTreeIter *iter)
{
	GtkTreeModel *model;
	GtkTreeIter iter_tmp;
	const gchar *description;

	/* iter is optional */
	if (iter == NULL)
		iter = &iter_tmp;

	/* also add profile */
	model = gtk_combo_box_get_model (GTK_COMBO_BOX(widget));
	description = cd_profile_get_title (profile);
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
	CdColorspace colorspace;
	CdDevice *device_tmp;
	CdProfile *profile;
	const gchar *filename;
	const gchar *tmp;
	gboolean has_profile = FALSE;
	gboolean has_vcgt;
	gchar *text = NULL;
	gboolean ret;
	GError *error = NULL;
	GPtrArray *devices = NULL;
	GPtrArray *profile_array = NULL;
	GtkTreeIter iter;
	GtkTreeModel *model;
	guint i;

	/* get new list */
	profile_array = cd_client_get_profiles_sync (client,
						     NULL,
						     &error);
	if (profile_array == NULL) {
		g_warning ("failed to get profiles: %s",
			   error->message);
		g_error_free (error);
		goto out;
	}

	/* update each list */
	for (i=0; i<profile_array->len; i++) {
		profile = g_ptr_array_index (profile_array, i);

		/* connect to the profile */
		ret = cd_profile_connect_sync (profile, NULL, &error);
		if (!ret) {
			g_warning ("failed to connect to profile: %s",
				   error->message);
			g_error_free (error);
			goto out;
		}

		/* ignore profiles from other user accounts */
		if (!cd_profile_has_access (profile))
			continue;

		/* is a printer profile */
		filename = cd_profile_get_filename (profile);
		if (filename == NULL)
			continue;

		/* only for correct kind */
		has_vcgt = cd_profile_get_has_vcgt (profile);
		tmp = cd_profile_get_metadata_item (profile, CD_PROFILE_METADATA_STANDARD_SPACE);
		colorspace = cd_profile_get_colorspace (profile);
		if (!has_vcgt && tmp != NULL &&
		    colorspace == CD_COLORSPACE_RGB) {
			gcm_prefs_combobox_add_profile (widget, profile, &iter);

			/* set active option */
			if (g_strcmp0 (tmp, "adobe-rgb") == 0) {
				profile_filename = filename;
				gtk_combo_box_set_active_iter (GTK_COMBO_BOX (widget), &iter);
			}
			has_profile = TRUE;
		}
	}

	/* add device profiles */
	devices = cd_client_get_devices_by_kind_sync (client,
						      CD_DEVICE_KIND_DISPLAY,
						      NULL,
						      &error);
	for (i=0; i<devices->len; i++) {
		device_tmp = g_ptr_array_index (devices, i);

		/* connect to the device */
		ret = cd_device_connect_sync (device_tmp, NULL, &error);
		if (!ret) {
			g_warning ("failed to connect to device: %s",
				   error->message);
			g_error_free (error);
			goto out;
		}

		profile = cd_device_get_default_profile (device_tmp);
		if (profile == NULL)
			continue;

		/* connect to the profile */
		ret = cd_profile_connect_sync (profile, NULL, &error);
		if (!ret) {
			g_warning ("failed to connect to profile: %s",
				   error->message);
			g_error_free (error);
			goto out;
		}

		/* add device profile */
		gcm_prefs_combobox_add_profile (widget, profile, NULL);
		g_object_unref (profile);
		has_profile = TRUE;
	}

	if (!has_profile) {
		/* TRANSLATORS: this is when there are no profiles that can be used;
		 * the search term is either "RGB" or "CMYK" */
		text = g_strdup_printf (_("No %s color spaces available"),
					cd_colorspace_to_localised_string (CD_COLORSPACE_RGB));
		model = gtk_combo_box_get_model (GTK_COMBO_BOX (widget));
		gtk_list_store_append (GTK_LIST_STORE(model), &iter);
		gtk_list_store_set (GTK_LIST_STORE(model), &iter,
				    GCM_PREFS_COMBO_COLUMN_TEXT, text,
				    -1);
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
		gtk_widget_set_sensitive (widget, FALSE);
	}
out:
	if (devices != NULL)
		g_ptr_array_unref (devices);
	if (profile_array != NULL)
		g_ptr_array_unref (profile_array);
	g_free (text);
}

/**
 * gcm_picker_activate_cb:
 **/
static void
gcm_picker_activate_cb (GApplication *application, gpointer user_data)
{
	GtkWindow *window;
	window = GTK_WINDOW (gtk_builder_get_object (builder, "dialog_picker"));
	gtk_window_present (window);
}

/**
 * gcm_picker_startup_cb:
 **/
static void
gcm_picker_startup_cb (GApplication *application, gpointer user_data)
{
	gboolean ret;
	GError *error = NULL;
	GtkWidget *main_window;
	GtkWidget *widget;
	guint retval = 0;

	/* get UI */
	builder = gtk_builder_new ();
	retval = gtk_builder_add_from_resource (builder,
						"/org/gnome/color-manager/gcm-picker.ui",
						&error);
	if (retval == 0) {
		g_warning ("failed to load ui: %s", error->message);
		g_error_free (error);
		goto out;
	}

	main_window = GTK_WIDGET (gtk_builder_get_object (builder, "dialog_picker"));
	gtk_application_add_window (GTK_APPLICATION (application), GTK_WINDOW (main_window));
	gtk_window_set_icon_name (GTK_WINDOW (main_window), GCM_STOCK_ICON);
	g_signal_connect (main_window, "delete_event",
			  G_CALLBACK (gcm_picker_delete_event_cb), application);

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "button_close"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gcm_picker_close_cb), application);


	widget = GTK_WIDGET (gtk_builder_get_object (builder, "button_measure"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gcm_picker_measure_cb), NULL);

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "image_preview"));
	gtk_widget_set_size_request (widget, 200, 200);

	/* add application specific icons to search path */
	gtk_icon_theme_append_search_path (gtk_icon_theme_get_default (),
	                                   GCM_DATA G_DIR_SEPARATOR_S "icons");

	/* create a last sample */
	//cd_color_xyz_clear (&last_sample);

	/* set the parent window if it is specified */
	if (xid != 0) {
		g_debug ("Setting xid %i", xid);
		gcm_window_set_parent_xid (GTK_WINDOW (main_window), xid);
	}

	/* use an info bar if there is no device, or the wrong device */
	info_bar_hardware = gtk_info_bar_new ();
	info_bar_hardware_label = gtk_label_new (NULL);
	gtk_info_bar_set_message_type (GTK_INFO_BAR(info_bar_hardware), GTK_MESSAGE_INFO);
	widget = gtk_info_bar_get_content_area (GTK_INFO_BAR(info_bar_hardware));
	gtk_container_add (GTK_CONTAINER(widget), info_bar_hardware_label);
	gtk_widget_show (info_bar_hardware_label);

	/* add infobar to devices pane */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "box1"));
	gtk_box_pack_start (GTK_BOX(widget), info_bar_hardware, FALSE, FALSE, 0);

	/* maintain a list of profiles */
	client = cd_client_new ();
	ret = cd_client_connect_sync (client,
				      NULL,
				      &error);
	if (!ret) {
		g_warning ("failed to connect to colord: %s",
			   error->message);
		g_error_free (error);
		goto out;
	}
	g_signal_connect (client, "sensor-added",
			  G_CALLBACK (gcm_picker_sensor_client_changed_cb), NULL);
	g_signal_connect (client, "sensor-removed",
			  G_CALLBACK (gcm_picker_sensor_client_changed_cb), NULL);

	/* disable some ui if no hardware */
	gcm_picker_sensor_client_setup_ui ();

	/* setup RGB combobox */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "combobox_colorspace"));
	gcm_prefs_set_combo_simple_text (widget);
	gcm_prefs_setup_space_combobox (widget);
	g_signal_connect (G_OBJECT (widget), "changed",
			  G_CALLBACK (gcm_prefs_space_combo_changed_cb), NULL);

	/* setup results expander */
//	gcm_picker_refresh_results (NULL);

	/* setup initial preview window */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "image_preview"));
	gtk_image_set_from_file (GTK_IMAGE (widget), DATADIR "/icons/hicolor/64x64/apps/gnome-color-manager.png");

	/* wait */
	gtk_widget_show (main_window);
out:
	return;
}

/**
 * main:
 **/
int
main (int argc, char *argv[])
{
	GOptionContext *context;
	GtkApplication *application;
	int status = 0;

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

	/* setup LCMS */
	cmsSetLogErrorHandler (gcm_picker_error_cb);

	context = g_option_context_new (NULL);
	/* TRANSLATORS: tool that is used to pick colors */
	g_option_context_set_summary (context, _("GNOME Color Manager Color Picker"));
	g_option_context_add_group (context, gcm_debug_get_option_group ());
	g_option_context_add_group (context, gtk_get_option_group (TRUE));
	g_option_context_add_main_entries (context, options, NULL);
	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);

	/* ensure single instance */
	application = gtk_application_new ("org.gnome.ColorManager.Picker", 0);
	g_signal_connect (application, "startup",
			  G_CALLBACK (gcm_picker_startup_cb), NULL);
	g_signal_connect (application, "activate",
			  G_CALLBACK (gcm_picker_activate_cb), NULL);

	status = g_application_run (G_APPLICATION (application), argc, argv);

	g_object_unref (application);
	if (client != NULL)
		g_object_unref (client);
	if (builder != NULL)
		g_object_unref (builder);
	return status;
}


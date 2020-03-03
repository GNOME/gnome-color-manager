/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010-2015 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: GPL-2.0+
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

typedef struct {
	CdClient	*client;
	const gchar	*profile_filename;
	gboolean	 done_measure;
	CdColorXYZ	 last_sample;
	CdSensor	*sensor;
	gdouble		 last_ambient;
	GtkBuilder	*builder;
	GtkWidget	*info_bar_hardware_label;
	GtkWidget	*info_bar_hardware;
	guint		 xid;
	guint		 unlock_timer;
} GcmPickerPrivate;

enum {
	GCM_PREFS_COMBO_COLUMN_TEXT,
	GCM_PREFS_COMBO_COLUMN_PROFILE,
	GCM_PREFS_COMBO_COLUMN_LAST
};

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
		for (x = 0; x < width; x++) {
			p = pixels + y * rowstride + x * n_channels;
			p[0] = red;
			p[1] = green;
			p[2] = blue;
		}
	}
}

static void
gcm_picker_refresh_results (GcmPickerPrivate *priv)
{
	cmsCIExyY xyY;
	cmsHPROFILE profile_lab;
	cmsHPROFILE profile_rgb;
	cmsHPROFILE profile_xyz;
	cmsHTRANSFORM transform_error;
	cmsHTRANSFORM transform_lab;
	cmsHTRANSFORM transform_rgb;
	gboolean ret;
	CdColorLab color_lab;
	CdColorRGB8 color_rgb;
	CdColorXYZ color_error;
	CdColorXYZ color_xyz;
	gdouble temperature = 0.0f;
	GtkImage *image;
	GtkLabel *label;
	g_autoptr(GdkPixbuf) pixbuf = NULL;
	g_autofree gchar *text_ambient = NULL;
	g_autofree gchar *text_error = NULL;
	g_autofree gchar *text_lab = NULL;
	g_autofree gchar *text_rgb = NULL;
	g_autofree gchar *text_temperature = NULL;
	g_autofree gchar *text_whitepoint = NULL;
	g_autofree gchar *text_xyz = NULL;

	/* nothing set yet */
	if (priv->profile_filename == NULL)
		return;

	/* copy as we're modifying the value */
	cd_color_xyz_copy (&priv->last_sample, &color_xyz);

	/* create new pixbuf of the right size */
	image = GTK_IMAGE (gtk_builder_get_object (priv->builder, "image_preview"));
	pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8,
				 gtk_widget_get_allocated_width (GTK_WIDGET (image)),
				 gtk_widget_get_allocated_height (GTK_WIDGET (image)));

	/* lcms scales these for some reason */
	color_xyz.X /= 100.0f;
	color_xyz.Y /= 100.0f;
	color_xyz.Z /= 100.0f;

	/* get profiles */
	profile_xyz = cmsCreateXYZProfile ();
	profile_rgb = cmsOpenProfileFromFile (priv->profile_filename, "r");
	profile_lab = cmsCreateLab4Profile (cmsD50_xyY ());

	/* create transforms */
	transform_rgb = cmsCreateTransform (profile_xyz, TYPE_XYZ_DBL,
					    profile_rgb, TYPE_RGB_8,
					    INTENT_PERCEPTUAL, 0);
	if (transform_rgb == NULL)
		return;
	transform_lab = cmsCreateTransform (profile_xyz, TYPE_XYZ_DBL,
					    profile_lab, TYPE_Lab_DBL,
					    INTENT_PERCEPTUAL, 0);
	if (transform_lab == NULL)
		return;
	transform_error = cmsCreateTransform (profile_rgb, TYPE_RGB_8,
					      profile_xyz, TYPE_XYZ_DBL,
					      INTENT_PERCEPTUAL, 0);
	if (transform_error == NULL)
		return;

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
	label = GTK_LABEL (gtk_builder_get_object (priv->builder, "label_xyz"));
	text_xyz = g_strdup_printf ("%.3f, %.3f, %.3f",
				    priv->last_sample.X,
				    priv->last_sample.Y,
				    priv->last_sample.Z);
	gtk_label_set_label (label, text_xyz);

	/* set LAB */
	label = GTK_LABEL (gtk_builder_get_object (priv->builder, "label_lab"));
	text_lab = g_strdup_printf ("%.3f, %.3f, %.3f",
				    color_lab.L,
				    color_lab.a,
				    color_lab.b);
	gtk_label_set_label (label, text_lab);

	/* set whitepoint */
	cmsXYZ2xyY (&xyY, (cmsCIEXYZ *)&priv->last_sample);
	label = GTK_LABEL (gtk_builder_get_object (priv->builder, "label_whitepoint"));
	text_whitepoint = g_strdup_printf ("%.3f,%.3f [%.3f]",
					   xyY.x, xyY.y, xyY.Y);
	gtk_label_set_label (label, text_whitepoint);

	/* set temperature */
	ret = cmsTempFromWhitePoint (&temperature, &xyY);
	if (ret) {
		/* round to nearest 10K */
		temperature = (((guint) temperature) / 10) * 10;
	}
	label = GTK_LABEL (gtk_builder_get_object (priv->builder, "label_temperature"));
	text_temperature = g_strdup_printf ("%.0fK", temperature);
	gtk_label_set_label (label, text_temperature);

	/* set RGB */
	label = GTK_LABEL (gtk_builder_get_object (priv->builder, "label_rgb"));
	text_rgb = g_strdup_printf ("%i, %i, %i (#%02X%02X%02X)",
				    color_rgb.R, color_rgb.G, color_rgb.B,
				    color_rgb.R, color_rgb.G, color_rgb.B);
	gtk_label_set_label (label, text_rgb);
	gcm_picker_set_pixbuf_color (pixbuf, color_rgb.R, color_rgb.G, color_rgb.B);

	/* set error */
	label = GTK_LABEL (gtk_builder_get_object (priv->builder, "label_error"));
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
	label = GTK_LABEL (gtk_builder_get_object (priv->builder, "label_ambient"));
	if (priv->last_ambient < 0) {
		/* TRANSLATORS: this is when the ambient light level is unknown */
		gtk_label_set_label (label, _("Unknown"));
	} else {
		text_ambient = g_strdup_printf ("%.1f Lux", priv->last_ambient);
		gtk_label_set_label (label, text_ambient);
	}

	/* set image */
	gtk_image_set_from_pixbuf (image, pixbuf);
}

static void
gcm_picker_got_results (GcmPickerPrivate *priv)
{
	GtkWidget *widget;

	/* set expanded */
	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "expander_results"));
	gtk_expander_set_expanded (GTK_EXPANDER (widget), TRUE);
	gtk_widget_set_sensitive (widget, TRUE);

	/* we've got results so make sure it's sensitive */
	priv->done_measure = TRUE;
}

static gboolean
gcm_picker_unlock_timeout_cb (gpointer user_data)
{
	g_autoptr(GError) error = NULL;
	GcmPickerPrivate *priv = (GcmPickerPrivate *) user_data;

	/* unlock */
	if (!cd_sensor_unlock_sync (priv->sensor, NULL, &error))
		g_warning ("failed to unlock: %s", error->message);
	priv->unlock_timer = 0;
	return FALSE;
}

static void
gcm_picker_measure_cb (GtkWidget *widget, gpointer data)
{
	GcmPickerPrivate *priv = (GcmPickerPrivate *) data;
	gboolean ret;
	g_autoptr(CdColorXYZ) tmp = NULL;
	g_autoptr(GError) error = NULL;

	/* reset the image */
	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "image_preview"));
	gtk_image_set_from_file (GTK_IMAGE (widget), DATADIR "/icons/hicolor/64x64/apps/gnome-color-manager.png");

	/* lock */
	if (!cd_sensor_get_locked (priv->sensor)) {
		ret = cd_sensor_lock_sync (priv->sensor,
					   NULL,
					   &error);
		if (!ret) {
			g_warning ("failed to lock: %s", error->message);
			return;
		}
	}

	/* cancel pending unlock */
	if (priv->unlock_timer != 0) {
		g_source_remove (priv->unlock_timer);
		priv->unlock_timer = 0;
	}

	/* get color */
	tmp = cd_sensor_get_sample_sync (priv->sensor,
					 CD_SENSOR_CAP_LCD,
					 NULL,
					 &error);
	if (tmp == NULL) {
		g_warning ("failed to get sample: %s", error->message);
		goto out_unlock;
	}
	cd_color_xyz_copy (tmp, &priv->last_sample);
#if 0
	/* get ambient */
	ret = cd_sensor_get_sample_sync (priv->sensor,
					 CD_SENSOR_CAP_AMBIENT,
					 NULL,
					 &last_ambient,
					 NULL,
					 &error);
	if (!ret) {
		g_warning ("failed to get ambient: %s", error->message);
		goto out_unlock;
	}
#endif
out_unlock:
	/* unlock after a small delay */
	priv->unlock_timer = g_timeout_add_seconds (30, gcm_picker_unlock_timeout_cb, data);
	gcm_picker_refresh_results (priv);
	gcm_picker_got_results (priv);
}

static void
gcm_picker_sensor_client_setup_ui (GcmPickerPrivate *priv)
{
	gboolean ret = FALSE;
	GtkWidget *widget;
	g_autoptr(GError) error = NULL;
	g_autoptr(GPtrArray) sensors;

	/* no present */
	sensors = cd_client_get_sensors_sync (priv->client, NULL, &error);
	if (sensors == NULL) {
		g_warning ("%s", error->message);
		goto out;
	}
	if (sensors->len == 0) {
		gtk_label_set_label (GTK_LABEL (priv->info_bar_hardware_label),
				    /* TRANSLATORS: this is displayed the user has not got suitable hardware */
				    _("No colorimeter is attached."));
		goto out;
	}
	priv->sensor = g_object_ref (g_ptr_array_index (sensors, 0));

	/* connect to the profile */
	ret = cd_sensor_connect_sync (priv->sensor, NULL, &error);
	if (!ret) {
		g_warning ("failed to connect to sensor: %s",
			   error->message);
		goto out;
	}

	if (!cd_sensor_get_native (priv->sensor)) {
		gtk_label_set_label (GTK_LABEL (priv->info_bar_hardware_label),
				     /* TRANSLATORS: this is displayed if VTE support is not enabled */
				     _("The sensor has no native driver."));
		goto out;
	}

#if 0
	/* no support */
	ret = cd_sensor_supports_spot (priv->sensor);
	if (!ret) {
		/* TRANSLATORS: this is displayed the user has not got suitable hardware */
		gtk_label_set_label (GTK_LABEL (priv->info_bar_hardware_label), _("The attached colorimeter is not capable of reading a spot color."));
		goto out;
	}
#else
	ret = TRUE;
#endif

out:
	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "button_measure"));
	gtk_widget_set_sensitive (widget, ret);
	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "expander_results"));
	gtk_widget_set_sensitive (widget, ret && priv->done_measure);
	gtk_widget_set_visible (priv->info_bar_hardware, !ret);
}

static void
gcm_picker_sensor_client_changed_cb (CdClient *_client,
				     CdSensor *_sensor,
				     GcmPickerPrivate *priv)
{
	gcm_picker_sensor_client_setup_ui (priv);
}

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

static void
gcm_picker_error_cb (cmsContext ContextID, cmsUInt32Number errorcode, const char *text)
{
	g_warning ("LCMS error %u: %s", errorcode, text);
}

static void
gcm_prefs_space_combo_changed_cb (GtkWidget *widget, GcmPickerPrivate *priv)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	g_autoptr(CdProfile) profile = NULL;

	/* no selection */
	if (!gtk_combo_box_get_active_iter (GTK_COMBO_BOX(widget), &iter))
		return;

	/* get profile */
	model = gtk_combo_box_get_model (GTK_COMBO_BOX(widget));
	gtk_tree_model_get (model, &iter,
			    GCM_PREFS_COMBO_COLUMN_PROFILE, &profile,
			    -1);
	if (profile == NULL)
		return;

	priv->profile_filename = cd_profile_get_filename (profile);
	g_debug ("changed picker space %s", priv->profile_filename);

	gcm_picker_refresh_results (priv);
}

static void
gcm_prefs_set_combo_simple_text (GtkWidget *combo_box)
{
	GtkCellRenderer *renderer;
	g_autoptr(GtkListStore) store = NULL;

	store = gtk_list_store_new (2, G_TYPE_STRING, CD_TYPE_PROFILE);
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store),
					      GCM_PREFS_COMBO_COLUMN_TEXT, GTK_SORT_ASCENDING);
	gtk_combo_box_set_model (GTK_COMBO_BOX (combo_box), GTK_TREE_MODEL (store));

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

static void
gcm_prefs_setup_space_combobox (GcmPickerPrivate *priv, GtkWidget *widget)
{
	CdColorspace colorspace;
	CdDevice *device_tmp;
	CdProfile *profile;
	const gchar *filename;
	const gchar *tmp;
	gboolean has_profile = FALSE;
	gboolean has_vcgt;
	gboolean ret;
	GtkTreeIter iter;
	GtkTreeModel *model;
	guint i;
	g_autofree gchar *text = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(GPtrArray) devices = NULL;
	g_autoptr(GPtrArray) profile_array = NULL;

	/* get new list */
	profile_array = cd_client_get_profiles_sync (priv->client,
						     NULL,
						     &error);
	if (profile_array == NULL) {
		g_warning ("failed to get profiles: %s",
			   error->message);
		return;
	}

	/* update each list */
	for (i = 0; i < profile_array->len; i++) {
		profile = g_ptr_array_index (profile_array, i);

		/* connect to the profile */
		ret = cd_profile_connect_sync (profile, NULL, &error);
		if (!ret) {
			g_warning ("failed to connect to profile: %s",
				   error->message);
			return;
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
				priv->profile_filename = filename;
				gtk_combo_box_set_active_iter (GTK_COMBO_BOX (widget), &iter);
			}
			has_profile = TRUE;
		}
	}

	/* add device profiles */
	devices = cd_client_get_devices_by_kind_sync (priv->client,
						      CD_DEVICE_KIND_DISPLAY,
						      NULL,
						      &error);
	for (i = 0; i < devices->len; i++) {
		device_tmp = g_ptr_array_index (devices, i);

		/* connect to the device */
		ret = cd_device_connect_sync (device_tmp, NULL, &error);
		if (!ret) {
			g_warning ("failed to connect to device: %s",
				   error->message);
			return;
		}

		profile = cd_device_get_default_profile (device_tmp);
		if (profile == NULL)
			continue;

		/* connect to the profile */
		ret = cd_profile_connect_sync (profile, NULL, &error);
		if (!ret) {
			g_warning ("failed to connect to profile: %s",
				   error->message);
			return;
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
}

static void
gcm_picker_activate_cb (GApplication *application, GcmPickerPrivate *priv)
{
	GtkWindow *window;
	window = GTK_WINDOW (gtk_builder_get_object (priv->builder, "dialog_picker"));
	gtk_window_present (window);
}

static void
gcm_picker_startup_cb (GApplication *application, GcmPickerPrivate *priv)
{
	gboolean ret;
	GtkWidget *main_window;
	GtkWidget *widget;
	guint retval = 0;
	g_autoptr(GError) error = NULL;

	/* get UI */
	priv->builder = gtk_builder_new ();
	retval = gtk_builder_add_from_resource (priv->builder,
						"/org/gnome/color-manager/gcm-picker.ui",
						&error);
	if (retval == 0) {
		g_warning ("failed to load ui: %s", error->message);
		return;
	}

	main_window = GTK_WIDGET (gtk_builder_get_object (priv->builder, "dialog_picker"));
	gtk_application_add_window (GTK_APPLICATION (application), GTK_WINDOW (main_window));
	gtk_window_set_icon_name (GTK_WINDOW (main_window), GCM_STOCK_ICON);

	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "button_measure"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gcm_picker_measure_cb), priv);

	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "image_preview"));
	gtk_widget_set_size_request (widget, 200, 200);

	/* add application specific icons to search path */
	gtk_icon_theme_append_search_path (gtk_icon_theme_get_default (),
	                                   PKGDATADIR G_DIR_SEPARATOR_S "icons");

	/* create a last sample */
	//cd_color_xyz_clear (&last_sample);

	/* set the parent window if it is specified */
	if (priv->xid != 0) {
		g_debug ("Setting xid %u", priv->xid);
		gcm_window_set_parent_xid (GTK_WINDOW (main_window), priv->xid);
	}

	/* use an info bar if there is no device, or the wrong device */
	priv->info_bar_hardware = gtk_info_bar_new ();
	priv->info_bar_hardware_label = gtk_label_new (NULL);
	gtk_info_bar_set_message_type (GTK_INFO_BAR(priv->info_bar_hardware), GTK_MESSAGE_INFO);
	widget = gtk_info_bar_get_content_area (GTK_INFO_BAR(priv->info_bar_hardware));
	gtk_container_add (GTK_CONTAINER(widget), priv->info_bar_hardware_label);
	gtk_widget_show (priv->info_bar_hardware_label);

	/* add infobar to devices pane */
	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "box1"));
	gtk_box_pack_start (GTK_BOX(widget), priv->info_bar_hardware, FALSE, FALSE, 0);

	/* maintain a list of profiles */
	priv->client = cd_client_new ();
	ret = cd_client_connect_sync (priv->client, NULL, &error);
	if (!ret) {
		g_warning ("failed to connect to colord: %s",
			   error->message);
		return;
	}
	g_signal_connect (priv->client, "sensor-added",
			  G_CALLBACK (gcm_picker_sensor_client_changed_cb), priv);
	g_signal_connect (priv->client, "sensor-removed",
			  G_CALLBACK (gcm_picker_sensor_client_changed_cb), priv);

	/* disable some ui if no hardware */
	gcm_picker_sensor_client_setup_ui (priv);

	/* setup RGB combobox */
	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "combobox_colorspace"));
	gcm_prefs_set_combo_simple_text (widget);
	gcm_prefs_setup_space_combobox (priv, widget);
	g_signal_connect (G_OBJECT (widget), "changed",
			  G_CALLBACK (gcm_prefs_space_combo_changed_cb), priv);

	/* setup initial preview window */
	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "image_preview"));
	gtk_image_set_from_file (GTK_IMAGE (widget), DATADIR "/icons/hicolor/64x64/apps/gnome-color-manager.png");

	/* wait */
	gtk_widget_show (main_window);
}

int
main (int argc, char *argv[])
{
	GcmPickerPrivate *priv;
	GOptionContext *context;
	GtkApplication *application;
	guint xid = 0;
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

	/* create private */
	priv = g_new0 (GcmPickerPrivate, 1);
	priv->last_ambient = -1.0f;
	priv->xid = xid;

	/* ensure single instance */
	application = gtk_application_new ("org.gnome.ColorManager.Picker", 0);
	g_signal_connect (application, "startup",
			  G_CALLBACK (gcm_picker_startup_cb), priv);
	g_signal_connect (application, "activate",
			  G_CALLBACK (gcm_picker_activate_cb), priv);

	status = g_application_run (G_APPLICATION (application), argc, argv);

	g_object_unref (application);
	if (priv->client != NULL)
		g_object_unref (priv->client);
	if (priv->builder != NULL)
		g_object_unref (priv->builder);
	g_free (priv);
	return status;
}

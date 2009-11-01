/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009 Richard Hughes <richard@hughsie.com>
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
 * GNU General Public License for more profile.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <math.h>
#include <unique/unique.h>
#include <glib/gstdio.h>

#include "egg-debug.h"

#include "gcm-utils.h"
#include "gcm-profile.h"
#include "gcm-calibrate.h"

static GtkBuilder *builder = NULL;
static GtkListStore *list_store_devices = NULL;
static guint current_device = 0;
static GcmClut *current_clut = NULL;
static GnomeRRScreen *rr_screen = NULL;
static GPtrArray *profiles_array = NULL;

enum {
	GPM_DEVICES_COLUMN_ID,
	GPM_DEVICES_COLUMN_ICON,
	GPM_DEVICES_COLUMN_TEXT,
	GPM_DEVICES_COLUMN_CLUT,
	GPM_DEVICES_COLUMN_LAST
};

/**
 * gcm_prefs_close_cb:
 **/
static void
gcm_prefs_close_cb (GtkWidget *widget, gpointer data)
{
	GMainLoop *loop = (GMainLoop *) data;
	g_main_loop_quit (loop);
}

/**
 * gcm_prefs_delete_event_cb:
 **/
static gboolean
gcm_prefs_delete_event_cb (GtkWidget *widget, GdkEvent *event, gpointer data)
{
	gcm_prefs_close_cb (widget, data);
	return FALSE;
}

/**
 * gcm_prefs_help_cb:
 **/
static void
gcm_prefs_help_cb (GtkWidget *widget, gpointer data)
{
	egg_warning ("help for prefs");
}

/**
 * gcm_prefs_calibrate_cb:
 **/
static void
gcm_prefs_calibrate_cb (GtkWidget *widget, gpointer data)
{
	GcmCalibrate *calib = NULL;
	gboolean ret;
	GError *error = NULL;
	GtkWindow *window;
	GnomeRROutput *output;
	const gchar *output_name;
	gchar *filename = NULL;
	gchar *destination = NULL;

	/* get the device */
	output = gnome_rr_screen_get_output_by_id (rr_screen, current_device);
	if (output == NULL) {
		egg_warning ("failed to get output");
		goto out;
	}

	/* create new calibration object */
	calib = gcm_calibrate_new ();

	/* set the proper output name */
	output_name = gnome_rr_output_get_name (output);
	g_object_set (calib,
		      "output-name", output_name,
		      NULL);

	/* run each task in order */
	window = GTK_WINDOW(gtk_builder_get_object (builder, "dialog_prefs"));
	ret = gcm_calibrate_setup (calib, window, &error);
	if (!ret) {
		egg_warning ("failed to setup: %s", error->message);
		g_error_free (error);
		goto finish_calibrate;
	}

	/* step 1 */
	ret = gcm_calibrate_task (calib, GCM_CALIBRATE_TASK_NEUTRALISE, &error);
	if (!ret) {
		egg_warning ("failed to calibrate: %s", error->message);
		g_error_free (error);
		goto finish_calibrate;
	}

	/* step 2 */
	ret = gcm_calibrate_task (calib, GCM_CALIBRATE_TASK_GENERATE_PATCHES, &error);
	if (!ret) {
		egg_warning ("failed to calibrate: %s", error->message);
		g_error_free (error);
		goto finish_calibrate;
	}

	/* step 3 */
	ret = gcm_calibrate_task (calib, GCM_CALIBRATE_TASK_DRAW_AND_MEASURE, &error);
	if (!ret) {
		egg_warning ("failed to calibrate: %s", error->message);
		g_error_free (error);
		goto finish_calibrate;
	}

	/* step 4 */
	ret = gcm_calibrate_task (calib, GCM_CALIBRATE_TASK_GENERATE_PROFILE, &error);
	if (!ret) {
		egg_warning ("failed to calibrate: %s", error->message);
		g_error_free (error);
		goto finish_calibrate;
	}

finish_calibrate:
	/* step 4 */
	filename = gcm_calibrate_finish (calib, &error);
	if (filename == NULL) {
		egg_warning ("failed to finish calibrate: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* copy the ICC file to the proper location */
	destination = gcm_utils_get_profile_destination (filename);
	ret = gcm_utils_mkdir_and_copy (filename, destination, &error);
	if (!ret) {
		egg_warning ("failed to calibrate: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* set the new profile and save config */
	g_ptr_array_add (profiles_array, g_strdup (destination));
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "combobox_profile"));
	gtk_combo_box_set_active (GTK_COMBO_BOX (widget), profiles_array->len - 1);

	/* remove temporary file */
	g_unlink (filename);

out:
	if (calib != NULL)
		g_object_unref (calib);

	/* need to set the gamma back to the default after calibration */
	ret = gcm_utils_set_output_gamma (output, &error);
	if (!ret) {
		egg_warning ("failed to set output gamma: %s", error->message);
		g_error_free (error);
	}
	g_free (filename);
	g_free (destination);
}

/**
 * gcm_prefs_reset_cb:
 **/
static void
gcm_prefs_reset_cb (GtkWidget *widget, gpointer data)
{
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hscale_gamma"));
	gtk_range_set_value (GTK_RANGE (widget), 1.0f);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hscale_brightness"));
	gtk_range_set_value (GTK_RANGE (widget), 0.0f);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hscale_contrast"));
	gtk_range_set_value (GTK_RANGE (widget), 100.0f);
}

/**
 * gcm_prefs_message_received_cb
 **/
static UniqueResponse
gcm_prefs_message_received_cb (UniqueApp *app, UniqueCommand command, UniqueMessageData *message_data, guint time_ms, gpointer data)
{
	GtkWindow *window;
	if (command == UNIQUE_ACTIVATE) {
		window = GTK_WINDOW (gtk_builder_get_object (builder, "dialog_prefs"));
		gtk_window_present (window);
	}
	return UNIQUE_RESPONSE_OK;
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
 * gcm_prefs_add_devices_columns:
 **/
static void
gcm_prefs_add_devices_columns (GtkTreeView *treeview)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	/* image */
	renderer = gtk_cell_renderer_pixbuf_new ();
	g_object_set (renderer, "stock-size", GTK_ICON_SIZE_DIALOG, NULL);
	column = gtk_tree_view_column_new_with_attributes (_("Screen"), renderer,
							   "icon-name", GPM_DEVICES_COLUMN_ICON, NULL);
	gtk_tree_view_append_column (treeview, column);

	/* column for text */
	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Label"), renderer,
							   "markup", GPM_DEVICES_COLUMN_TEXT, NULL);
	gtk_tree_view_column_set_sort_column_id (column, GPM_DEVICES_COLUMN_TEXT);
	gtk_tree_view_append_column (treeview, column);
	gtk_tree_view_column_set_expand (column, TRUE);
}

/**
 * gcm_prefs_devices_treeview_clicked_cb:
 **/
static void
gcm_prefs_devices_treeview_clicked_cb (GtkTreeSelection *selection, gboolean data)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *profile;
	GtkWidget *widget;
	gfloat localgamma;
	gfloat brightness;
	gfloat contrast;
	const gchar *filename;
	guint i;

	/* This will only work in single or browse selection mode! */
	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
		egg_debug ("no row selected");
		return;
	}

	gtk_tree_model_get (model, &iter,
			    GPM_DEVICES_COLUMN_ID, &current_device,
			    GPM_DEVICES_COLUMN_CLUT, &current_clut,
			    -1);

	/* show transaction_id */
	egg_debug ("selected row is: %i", current_device);

	g_object_get (current_clut,
		      "profile", &profile,
		      "gamma", &localgamma,
		      "brightness", &brightness,
		      "contrast", &contrast,
		      NULL);

	/* set adjustments */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hscale_gamma"));
	gtk_range_set_value (GTK_RANGE (widget), localgamma);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hscale_brightness"));
	gtk_range_set_value (GTK_RANGE (widget), brightness);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hscale_contrast"));
	gtk_range_set_value (GTK_RANGE (widget), contrast);

	/* set correct profile */
	if (profile == NULL) {
		widget = GTK_WIDGET (gtk_builder_get_object (builder, "combobox_profile"));
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), profiles_array->len);
	} else {
		profiles_array = gcm_utils_get_profile_filenames ();
		for (i=0; i<profiles_array->len; i++) {
			filename = g_ptr_array_index (profiles_array, i);
			if (g_strcmp0 (filename, profile) == 0) {
				widget = GTK_WIDGET (gtk_builder_get_object (builder, "combobox_profile"));
				gtk_combo_box_set_active (GTK_COMBO_BOX (widget), i);
				break;
			}
		}
	}

	g_free (profile);
}

/**
 * gcm_prefs_add_device:
 **/
static void
gcm_prefs_add_device (GnomeRROutput *output)
{
	GtkTreeIter iter;
	gchar *name;
	guint id;
	GcmClut *clut;
	GError *error = NULL;

	id = gnome_rr_output_get_id (output);
	name = gcm_utils_get_output_name (output);

	/* create a clut for this output */
	clut = gcm_utils_get_clut_for_output (output, &error);
	if (clut == NULL) {
		egg_warning ("failed to add device: %s", error->message);
		g_error_free (error);
		goto out;
	}

	gtk_list_store_append (list_store_devices, &iter);
	gtk_list_store_set (list_store_devices, &iter,
			    GPM_DEVICES_COLUMN_ID, id,
			    GPM_DEVICES_COLUMN_TEXT, name,
			    GPM_DEVICES_COLUMN_CLUT, clut,
			    GPM_DEVICES_COLUMN_ICON, "video-display", -1);
out:
	g_free (name);
}

/**
 * gcm_prefs_set_combo_simple_text:
 **/
static void
gcm_prefs_set_combo_simple_text (GtkWidget *combo_box)
{
	GtkCellRenderer *cell;
	GtkListStore *store;

	store = gtk_list_store_new (1, G_TYPE_STRING);
	gtk_combo_box_set_model (GTK_COMBO_BOX (combo_box), GTK_TREE_MODEL (store));
	g_object_unref (store);

	cell = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo_box), cell, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo_box), cell,
					"text", 0,
					NULL);
}

/**
 * gcm_prefs_add_profiles:
 **/
static void
gcm_prefs_add_profiles (GtkWidget *widget)
{
	const gchar *filename;
	guint i;
	gchar *displayname;
	GcmProfile *profile;
	gboolean ret;
	GError *error = NULL;

	/* get profiles */
	profiles_array = gcm_utils_get_profile_filenames ();
	for (i=0; i<profiles_array->len; i++) {
		filename = g_ptr_array_index (profiles_array, i);
		profile = gcm_profile_new ();
		ret = gcm_profile_load (profile, filename, &error);
		if (!ret) {
			egg_warning ("failed to add profile: %s", error->message);
			g_error_free (error);
			g_object_unref (profile);
			continue;
		}
		g_object_get (profile,
			      "description", &displayname,
			      NULL);
		gtk_combo_box_append_text (GTK_COMBO_BOX (widget), displayname);
		g_free (displayname);
		g_object_unref (profile);
	}

	/* add a clear entry */
	gtk_combo_box_append_text (GTK_COMBO_BOX (widget), _("None"));
}

/**
 * gcm_prefs_history_type_combo_changed_cb:
 **/
static void
gcm_prefs_history_type_combo_changed_cb (GtkWidget *widget, gpointer data)
{
	guint active;
	gchar *copyright = NULL;
	gchar *description = NULL;
	const gchar *filename = NULL;
	gboolean ret;
	GError *error = NULL;
	GnomeRROutput *output;

	active = gtk_combo_box_get_active (GTK_COMBO_BOX(widget));
	egg_debug ("now %i", active);

	if (active < profiles_array->len)
		filename = g_ptr_array_index (profiles_array, active);
	egg_debug ("profile=%s", filename);

	/* set new profile */
	g_object_set (current_clut,
		      "profile", filename,
		      NULL);

	/* get new data */
	ret = gcm_clut_load_from_profile (current_clut, &error);
	if (!ret) {
		egg_warning ("failed to load profile: %s", error->message);
		g_error_free (error);
		goto out;
	}
	g_object_get (current_clut,
		      "copyright", &copyright,
		      "description", &description,
		      NULL);

	/* set new descriptions */
	if (copyright == NULL) {
		widget = GTK_WIDGET (gtk_builder_get_object (builder, "label_title_copyright"));
		gtk_widget_hide (widget);
		widget = GTK_WIDGET (gtk_builder_get_object (builder, "label_copyright"));
		gtk_widget_hide (widget);
	} else {
		widget = GTK_WIDGET (gtk_builder_get_object (builder, "label_title_copyright"));
		gtk_widget_show (widget);
		widget = GTK_WIDGET (gtk_builder_get_object (builder, "label_copyright"));
		gtk_label_set_label (GTK_LABEL(widget), copyright);
		gtk_widget_show (widget);
	}

	/* set new descriptions */
	if (description == NULL) {
		widget = GTK_WIDGET (gtk_builder_get_object (builder, "label_title_description"));
		gtk_widget_hide (widget);
		widget = GTK_WIDGET (gtk_builder_get_object (builder, "label_description"));
		gtk_widget_hide (widget);
	} else {
		widget = GTK_WIDGET (gtk_builder_get_object (builder, "label_title_description"));
		gtk_widget_show (widget);
		widget = GTK_WIDGET (gtk_builder_get_object (builder, "label_description"));
		gtk_label_set_label (GTK_LABEL(widget), description);
		gtk_widget_show (widget);
	}

	/* set new descriptions */
	if (copyright == NULL && description == NULL) {
		widget = GTK_WIDGET (gtk_builder_get_object (builder, "table_details"));
		gtk_widget_hide (widget);
	} else {
		widget = GTK_WIDGET (gtk_builder_get_object (builder, "table_details"));
		gtk_widget_show (widget);
	}

	/* save new profile */
	ret = gcm_clut_save_to_config (current_clut, &error);
	if (!ret) {
		egg_warning ("failed to save config: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* get the device */
	output = gnome_rr_screen_get_output_by_id (rr_screen, current_device);
	if (output == NULL) {
		egg_warning ("failed to get output");
		goto out;
	}

	/* actually set the new profile */
	ret = gcm_utils_set_output_gamma (output, &error);
	if (!ret) {
		egg_warning ("failed to set output gamma: %s", error->message);
		g_error_free (error);
		goto out;
	}
out:
	g_free (copyright);
	g_free (description);
}

/**
 * gcm_prefs_slider_changed_cb:
 **/
static void
gcm_prefs_slider_changed_cb (GtkRange *range, gpointer *user_data)
{
	gfloat localgamma;
	gfloat brightness;
	gfloat contrast;
	GtkWidget *widget;
	gboolean ret;
	GError *error = NULL;
	GnomeRROutput *output;

	/* get values */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hscale_gamma"));
	localgamma = gtk_range_get_value (GTK_RANGE (widget));
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hscale_brightness"));
	brightness = gtk_range_get_value (GTK_RANGE (widget));
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hscale_contrast"));
	contrast = gtk_range_get_value (GTK_RANGE (widget));

	g_object_set (current_clut,
		      "gamma", localgamma,
		      "brightness", brightness,
		      "contrast", contrast,
		      NULL);

	/* save new profile */
	ret = gcm_clut_save_to_config (current_clut, &error);
	if (!ret) {
		egg_warning ("failed to save config: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* get the device */
	output = gnome_rr_screen_get_output_by_id (rr_screen, current_device);
	if (output == NULL) {
		egg_warning ("failed to get output");
		goto out;
	}

	/* actually set the new profile */
	ret = gcm_utils_set_output_gamma (output, &error);
	if (!ret) {
		egg_warning ("failed to set output gamma: %s", error->message);
		g_error_free (error);
		goto out;
	}
out:
	return;
}

/**
 * main:
 **/
int
main (int argc, char **argv)
{
	gboolean verbose = FALSE;
	guint retval = 0;
	GOptionContext *context;
	GtkWidget *main_window;
	GtkWidget *widget;
	UniqueApp *unique_app;
	guint xid = 0;
	GError *error = NULL;
	GMainLoop *loop;
	GtkTreeSelection *selection;
	GnomeRROutput **outputs;
	guint i;
	gboolean connected;

	const GOptionEntry options[] = {
		{ "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose,
		  _("Show extra debugging information"), NULL },
		{ "parent-window", 'p', 0, G_OPTION_ARG_INT, &xid,
		  /* TRANSLATORS: we can make this modal (stay on top of) another window */
		  _("Set the parent window to make this modal"), NULL },
		{ NULL}
	};

	gtk_init (&argc, &argv);

	context = g_option_context_new ("gnome-color-manager prefs program");
	g_option_context_add_main_entries (context, options, NULL);
	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);

	egg_debug_init (verbose);

	/* block in a loop */
	loop = g_main_loop_new (NULL, FALSE);

	/* are we already activated? */
	unique_app = unique_app_new ("org.gnome.ColorManager.Prefs", NULL);
	if (unique_app_is_running (unique_app)) {
		egg_debug ("You have another instance running. This program will now close");
		unique_app_send_message (unique_app, UNIQUE_ACTIVATE, NULL);
		goto out;
	}
	g_signal_connect (unique_app, "message-received",
			  G_CALLBACK (gcm_prefs_message_received_cb), NULL);

	/* get UI */
	builder = gtk_builder_new ();
	retval = gtk_builder_add_from_file (builder, GCM_DATA "/gcm-prefs.ui", &error);
	if (retval == 0) {
		egg_warning ("failed to load ui: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* create list stores */
	list_store_devices = gtk_list_store_new (GPM_DEVICES_COLUMN_LAST, G_TYPE_UINT,
						 G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER);

	/* create transaction_id tree view */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "treeview_devices"));
	gtk_tree_view_set_model (GTK_TREE_VIEW (widget),
				 GTK_TREE_MODEL (list_store_devices));
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
	g_signal_connect (selection, "changed",
			  G_CALLBACK (gcm_prefs_devices_treeview_clicked_cb), NULL);

	/* add columns to the tree view */
	gcm_prefs_add_devices_columns (GTK_TREE_VIEW (widget));
	gtk_tree_view_columns_autosize (GTK_TREE_VIEW (widget)); /* show */

	main_window = GTK_WIDGET (gtk_builder_get_object (builder, "dialog_prefs"));

	/* Hide window first so that the dialogue resizes itself without redrawing */
	gtk_widget_hide (main_window);
	gtk_window_set_icon_name (GTK_WINDOW (main_window), GCM_STOCK_ICON);
	g_signal_connect (main_window, "delete_event",
			  G_CALLBACK (gcm_prefs_delete_event_cb), loop);

	widget = GTK_WIDGET (gtk_builder_get_object (builder, "button_close"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gcm_prefs_close_cb), loop);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "button_help"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gcm_prefs_help_cb), NULL);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "button_reset"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gcm_prefs_reset_cb), NULL);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "button_calibrate"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gcm_prefs_calibrate_cb), NULL);

	/* setup icc profiles list */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "combobox_profile"));
	gcm_prefs_set_combo_simple_text (widget);
	gcm_prefs_add_profiles (widget);
	g_signal_connect (G_OBJECT (widget), "changed",
			  G_CALLBACK (gcm_prefs_history_type_combo_changed_cb), NULL);

	/* set ranges */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hscale_gamma"));
	gtk_range_set_range (GTK_RANGE (widget), 0.1f, 5.0f);
	gtk_scale_add_mark (GTK_SCALE (widget), 1.0f, GTK_POS_TOP, "");
	gtk_scale_add_mark (GTK_SCALE (widget), 1.8f, GTK_POS_TOP, "");
	gtk_scale_add_mark (GTK_SCALE (widget), 2.2f, GTK_POS_TOP, "");

	/* set ranges */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hscale_brightness"));
	gtk_range_set_range (GTK_RANGE (widget), 0.0f, 99.0f);

	/* set ranges */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hscale_contrast"));
	gtk_range_set_range (GTK_RANGE (widget), 1.0f, 100.0f);

	/* get screen */
	rr_screen = gnome_rr_screen_new (gdk_screen_get_default (), NULL, NULL, &error);
	if (rr_screen == NULL) {
		egg_warning ("failed to get rr screen: %s", error->message);
		goto out;
	}

	/* add devices */
	outputs = gnome_rr_screen_list_outputs (rr_screen);
	for (i=0; outputs[i] != NULL; i++) {
		/* if nothing connected then ignore */
		connected = gnome_rr_output_is_connected (outputs[i]);
		if (!connected)
			continue;

		/* add element */
		gcm_prefs_add_device (outputs[i]);
	}

	/* set the parent window if it is specified */
	if (xid != 0) {
		egg_debug ("Setting xid %i", xid);
		gcm_window_set_parent_xid (GTK_WINDOW (main_window), xid);
	}

	/* show main UI */
	gtk_widget_show (main_window);

	/* connect up sliders */
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hscale_contrast"));
	g_signal_connect (widget, "value-changed",
			  G_CALLBACK (gcm_prefs_slider_changed_cb), NULL);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hscale_brightness"));
	g_signal_connect (widget, "value-changed",
			  G_CALLBACK (gcm_prefs_slider_changed_cb), NULL);
	widget = GTK_WIDGET (gtk_builder_get_object (builder, "hscale_gamma"));
	g_signal_connect (widget, "value-changed",
			  G_CALLBACK (gcm_prefs_slider_changed_cb), NULL);

	/* wait */
	g_main_loop_run (loop);

out:
	g_object_unref (unique_app);
	g_main_loop_unref (loop);
	if (rr_screen != NULL)
		gnome_rr_screen_destroy (rr_screen);
	if (builder != NULL)
		g_object_unref (builder);
	if (profiles_array != NULL)
		g_ptr_array_unref (profiles_array);
	return retval;
}


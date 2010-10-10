/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009-2010 Richard Hughes <richard@hughsie.com>
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

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include <locale.h>
#include <canberra-gtk.h>

#include "egg-debug.h"

#include "gcm-cell-renderer-profile-text.h"
#include "gcm-calibrate-argyll.h"
#include "gcm-cie-widget.h"
#include "gcm-image.h"
#include "gcm-profile.h"
#include "gcm-profile-store.h"
#include "gcm-trc-widget.h"
#include "gcm-utils.h"
#include "gcm-color.h"

typedef struct {
	GtkBuilder	*builder;
	GtkApplication	*application;
	GtkListStore	*list_store_profiles;
	GcmProfileStore	*profile_store;
	GtkWidget	*cie_widget;
	GtkWidget	*trc_widget;
	GtkWidget	*vcgt_widget;
	GtkWidget	*preview_widget_input;
	GtkWidget	*preview_widget_output;
	GSettings	*settings;
	guint		 example_index;
} GcmViewerPrivate;

typedef enum {
	GCM_VIEWER_NONE,
	GCM_VIEWER_CIE_1931,
	GCM_VIEWER_TRC,
	GCM_VIEWER_VCGT,
	GCM_VIEWER_PREVIEW_INPUT,
	GCM_VIEWER_PREVIEW_OUTPUT
} GcmViewerGraphType;

enum {
	GCM_PROFILES_COLUMN_ID,
	GCM_PROFILES_COLUMN_SORT,
	GCM_PROFILES_COLUMN_ICON,
	GCM_PROFILES_COLUMN_PROFILE,
	GCM_PROFILES_COLUMN_LAST
};

enum {
	GCM_VIEWER_COMBO_COLUMN_TEXT,
	GCM_VIEWER_COMBO_COLUMN_PROFILE,
	GCM_VIEWER_COMBO_COLUMN_TYPE,
	GCM_VIEWER_COMBO_COLUMN_SORTABLE,
	GCM_VIEWER_COMBO_COLUMN_LAST
};

static void gcm_viewer_profile_store_changed_cb (GcmProfileStore *profile_store, GcmViewerPrivate *viewer);

#define GCM_VIEWER_TREEVIEW_WIDTH		350 /* px */
#define GCM_VIEWER_MAX_EXAMPLE_IMAGES		4

/**
 * gcm_viewer_error_dialog:
 **/
static void
gcm_viewer_error_dialog (GcmViewerPrivate *viewer, const gchar *title, const gchar *message)
{
	GtkWindow *window;
	GtkWidget *dialog;

	window = GTK_WINDOW(gtk_builder_get_object (viewer->builder, "dialog_viewer"));
	dialog = gtk_message_dialog_new (window, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "%s", title);
	gtk_window_set_icon_name (GTK_WINDOW (dialog), GCM_STOCK_ICON);
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", message);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

/**
 * gcm_viewer_close_cb:
 **/
static void
gcm_viewer_close_cb (GtkWidget *widget, GcmViewerPrivate *viewer)
{
	gtk_application_quit (viewer->application);
}

/**
 * gcm_viewer_set_example_image:
 **/
static void
gcm_viewer_set_example_image (GcmViewerPrivate *viewer, GtkImage *image)
{
	gchar *filename;
	filename = g_strdup_printf ("%s/figures/viewer-example-%02i.png", GCM_DATA, viewer->example_index);
	gtk_image_set_from_file (image, filename);
	g_free (filename);
}

/**
 * gcm_viewer_image_next_cb:
 **/
static void
gcm_viewer_image_next_cb (GtkWidget *widget, GcmViewerPrivate *viewer)
{
	viewer->example_index++;
	if (viewer->example_index == GCM_VIEWER_MAX_EXAMPLE_IMAGES)
		viewer->example_index = 0;
	gcm_viewer_set_example_image (viewer, GTK_IMAGE (viewer->preview_widget_input));
	gcm_viewer_set_example_image (viewer, GTK_IMAGE (viewer->preview_widget_output));
}

/**
 * gcm_viewer_image_prev_cb:
 **/
static void
gcm_viewer_image_prev_cb (GtkWidget *widget, GcmViewerPrivate *viewer)
{
	if (viewer->example_index == 0)
		viewer->example_index = GCM_VIEWER_MAX_EXAMPLE_IMAGES;
	viewer->example_index--;
	gcm_viewer_set_example_image (viewer, GTK_IMAGE (viewer->preview_widget_input));
	gcm_viewer_set_example_image (viewer, GTK_IMAGE (viewer->preview_widget_output));
}

/**
 * gcm_viewer_preferences_cb:
 **/
static void
gcm_viewer_preferences_cb (GtkWidget *widget, GcmViewerPrivate *viewer)
{
	gboolean ret;
	GError *error = NULL;
	ret = g_spawn_command_line_async ("gnome-control-center color", &error);
	if (!ret) {
		egg_warning ("failed to run prefs: %s", error->message);
		g_error_free (error);
	}
}

/**
 * gcm_viewer_help_cb:
 **/
static void
gcm_viewer_help_cb (GtkWidget *widget, GcmViewerPrivate *viewer)
{
	gcm_gnome_help ("viewer");
}

/**
 * gcm_viewer_delete_event_cb:
 **/
static gboolean
gcm_viewer_delete_event_cb (GtkWidget *widget, GdkEvent *event, GcmViewerPrivate *viewer)
{
	gcm_viewer_close_cb (widget, viewer);
	return FALSE;
}

/**
 * gcm_viewer_profile_kind_to_icon_name:
 **/
static const gchar *
gcm_viewer_profile_kind_to_icon_name (GcmProfileKind kind)
{
	if (kind == GCM_PROFILE_KIND_DISPLAY_DEVICE)
		return "video-display";
	if (kind == GCM_PROFILE_KIND_INPUT_DEVICE)
		return "scanner";
	if (kind == GCM_PROFILE_KIND_OUTPUT_DEVICE)
		return "printer";
	if (kind == GCM_PROFILE_KIND_COLORSPACE_CONVERSION)
		return "view-refresh";
	if (kind == GCM_PROFILE_KIND_ABSTRACT)
		return "insert-link";
	return "image-missing";
}

/**
 * gcm_viewer_profile_get_sort_string:
 **/
static const gchar *
gcm_viewer_profile_get_sort_string (GcmProfileKind kind)
{
	if (kind == GCM_PROFILE_KIND_DISPLAY_DEVICE)
		return "1";
	if (kind == GCM_PROFILE_KIND_INPUT_DEVICE)
		return "2";
	if (kind == GCM_PROFILE_KIND_OUTPUT_DEVICE)
		return "3";
	return "4";
}

/**
 * gcm_viewer_update_profile_list:
 **/
static void
gcm_viewer_update_profile_list (GcmViewerPrivate *viewer)
{
	GtkTreeIter iter;
	const gchar *description;
	const gchar *icon_name;
	GcmProfileKind profile_kind = GCM_PROFILE_KIND_UNKNOWN;
	GcmProfile *profile;
	guint i;
	const gchar *filename = NULL;
	gchar *sort = NULL;
	GPtrArray *profile_array = NULL;

	egg_debug ("updating profile list");

	/* get new list */
	profile_array = gcm_profile_store_get_array (viewer->profile_store);

	/* clear existing list */
	gtk_list_store_clear (viewer->list_store_profiles);

	/* update each list */
	for (i=0; i<profile_array->len; i++) {
		profile = g_ptr_array_index (profile_array, i);

		profile_kind = gcm_profile_get_kind (profile);
		icon_name = gcm_viewer_profile_kind_to_icon_name (profile_kind);
		gtk_list_store_append (viewer->list_store_profiles, &iter);
		description = gcm_profile_get_description (profile);
		sort = g_strdup_printf ("%s%s",
					gcm_viewer_profile_get_sort_string (profile_kind),
					description);
		filename = gcm_profile_get_filename (profile);
		egg_debug ("add %s to profiles list", filename);
		gtk_list_store_set (viewer->list_store_profiles, &iter,
				    GCM_PROFILES_COLUMN_ID, filename,
				    GCM_PROFILES_COLUMN_SORT, sort,
				    GCM_PROFILES_COLUMN_ICON, icon_name,
				    GCM_PROFILES_COLUMN_PROFILE, profile,
				    -1);

		g_free (sort);
	}
	if (profile_array != NULL)
		g_ptr_array_unref (profile_array);
}

/**
 * gcm_viewer_profile_delete_cb:
 **/
static void
gcm_viewer_profile_delete_cb (GtkWidget *widget, GcmViewerPrivate *viewer)
{
	GtkWidget *dialog;
	GtkResponseType response;
	GtkWindow *window;
	gint retval;
	const gchar *filename;
	GcmProfile *profile;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;

	/* ask the user to confirm */
	window = GTK_WINDOW(gtk_builder_get_object (viewer->builder, "dialog_viewer"));
	dialog = gtk_message_dialog_new (window, GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_QUESTION, GTK_BUTTONS_CANCEL,
					 /* TRANSLATORS: title, usually we can tell based on the EDID data or output name */
					 _("Permanently delete profile?"));
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  /* TRANSLATORS: dialog message */
						  _("Are you sure you want to remove this profile from your system permanently?"));
	gtk_window_set_icon_name (GTK_WINDOW (dialog), GCM_STOCK_ICON);
	/* TRANSLATORS: button, delete a profile */
	gtk_dialog_add_button (GTK_DIALOG (dialog), _("Delete"), GTK_RESPONSE_YES);
	response = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
	if (response != GTK_RESPONSE_YES)
		goto out;

	/* get the selected row */
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "treeview_profiles"));
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
		egg_debug ("no row selected");
		goto out;
	}

	/* get profile */
	gtk_tree_model_get (model, &iter,
			    GCM_PROFILES_COLUMN_PROFILE, &profile,
			    -1);

	/* try to remove file */
	filename = gcm_profile_get_filename (profile);
	retval = g_unlink (filename);
	if (retval != 0)
		goto out;
out:
	return;
}

/**
 * gcm_viewer_file_chooser_get_icc_profile:
 **/
static GFile *
gcm_viewer_file_chooser_get_icc_profile (GcmViewerPrivate *viewer)
{
	GtkWindow *window;
	GtkWidget *dialog;
	GFile *file = NULL;
	GtkFileFilter *filter;

	/* create new dialog */
	window = GTK_WINDOW(gtk_builder_get_object (viewer->builder, "dialog_viewer"));
	/* TRANSLATORS: dialog for file->open dialog */
	dialog = gtk_file_chooser_dialog_new (_("Select ICC Profile File"), window,
					       GTK_FILE_CHOOSER_ACTION_OPEN,
					       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					       _("Import"), GTK_RESPONSE_ACCEPT,
					      NULL);
	gtk_window_set_icon_name (GTK_WINDOW (dialog), GCM_STOCK_ICON);
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER(dialog), g_get_home_dir ());
	gtk_file_chooser_set_create_folders (GTK_FILE_CHOOSER(dialog), FALSE);
	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER(dialog), FALSE);

	/* setup the filter */
	filter = gtk_file_filter_new ();
	gtk_file_filter_add_mime_type (filter, "application/vnd.iccprofile");

	/* we can remove this when we depend on a new shared-mime-info */
	gtk_file_filter_add_pattern (filter, "*.icc");
	gtk_file_filter_add_pattern (filter, "*.icm");
	gtk_file_filter_add_pattern (filter, "*.ICC");
	gtk_file_filter_add_pattern (filter, "*.ICM");

	/* TRANSLATORS: filter name on the file->open dialog */
	gtk_file_filter_set_name (filter, _("Supported ICC profiles"));
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER(dialog), filter);

	/* setup the all files filter */
	filter = gtk_file_filter_new ();
	gtk_file_filter_add_pattern (filter, "*");
	/* TRANSLATORS: filter name on the file->open dialog */
	gtk_file_filter_set_name (filter, _("All files"));
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER(dialog), filter);

	/* did user choose file */
	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
		file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER(dialog));

	/* we're done */
	gtk_widget_destroy (dialog);

	/* or NULL for missing */
	return file;
}

/**
 * gcm_viewer_profile_import_file:
 **/
static gboolean
gcm_viewer_profile_import_file (GcmViewerPrivate *viewer, GFile *file)
{
	gboolean ret;
	GError *error = NULL;
	GFile *destination = NULL;

	/* check if correct type */
	ret = gcm_utils_is_icc_profile (file);
	if (!ret) {
		egg_debug ("not a ICC profile");
		goto out;
	}

	/* copy icc file to ~/.color/icc */
	destination = gcm_utils_get_profile_destination (file);
	ret = gcm_utils_mkdir_and_copy (file, destination, &error);
	if (!ret) {
		/* TRANSLATORS: could not read file */
		gcm_viewer_error_dialog (viewer, _("Failed to copy file"), error->message);
		g_error_free (error);
		goto out;
	}
out:
	if (destination != NULL)
		g_object_unref (destination);
	return ret;
}

/**
 * gcm_viewer_profile_import_cb:
 **/
static void
gcm_viewer_profile_import_cb (GtkWidget *widget, GcmViewerPrivate *viewer)
{
	GFile *file;

	/* get new file */
	file = gcm_viewer_file_chooser_get_icc_profile (viewer);
	if (file == NULL) {
		egg_warning ("failed to get filename");
		goto out;
	}

	/* import this */
	gcm_viewer_profile_import_file (viewer, file);
out:
	if (file != NULL)
		g_object_unref (file);
}

/**
 * gcm_viewer_drag_data_received_cb:
 **/
static void
gcm_viewer_drag_data_received_cb (GtkWidget *widget, GdkDragContext *context, gint x, gint y, GtkSelectionData *data, guint _time, GcmViewerPrivate *viewer)
{
	const guchar *filename;
	gchar **filenames = NULL;
	GFile *file = NULL;
	guint i;
	gboolean ret;
	gboolean success = FALSE;

	/* get filenames */
	filename = gtk_selection_data_get_data (data);
	if (filename == NULL)
		goto out;

	/* import this */
	egg_debug ("dropped: %p (%s)", data, filename);

	/* split, as multiple drag targets are accepted */
	filenames = g_strsplit_set ((const gchar *)filename, "\r\n", -1);
	for (i=0; filenames[i]!=NULL; i++) {

		/* blank entry */
		if (filenames[i][0] == '\0')
			continue;

		/* convert the URI */
		file = g_file_new_for_uri (filenames[i]);

		/* try to import it */
		ret = gcm_viewer_profile_import_file (viewer, file);
		if (ret)
			success = TRUE;

		g_object_unref (file);
	}

out:
	gtk_drag_finish (context, success, FALSE, _time);
	g_strfreev (filenames);
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
	if (our_window == NULL) {
		egg_warning ("failed to get our window");
		return;
	}

	/* set this above our parent */
	gtk_window_set_modal (window, TRUE);
	gdk_window_set_transient_for (our_window, parent_window);
}

/**
 * gcm_viewer_add_profiles_columns:
 **/
static void
gcm_viewer_add_profiles_columns (GcmViewerPrivate *viewer, GtkTreeView *treeview)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	/* image */
	renderer = gtk_cell_renderer_pixbuf_new ();
	g_object_set (renderer, "stock-size", GTK_ICON_SIZE_DND, NULL);
	column = gtk_tree_view_column_new_with_attributes ("", renderer,
							   "icon-name", GCM_PROFILES_COLUMN_ICON, NULL);
	gtk_tree_view_append_column (treeview, column);

	/* set minimum width */
	gtk_widget_set_size_request (GTK_WIDGET (treeview), GCM_VIEWER_TREEVIEW_WIDTH, -1);

	/* column for text */
	renderer = gcm_cell_renderer_profile_text_new ();
	g_object_set (renderer,
		      "wrap-mode", PANGO_WRAP_WORD,
		      "wrap-width", GCM_VIEWER_TREEVIEW_WIDTH - 62,
		      NULL);
	column = gtk_tree_view_column_new_with_attributes ("", renderer,
							   "profile", GCM_PROFILES_COLUMN_PROFILE, NULL);
	gtk_tree_view_column_set_sort_column_id (column, GCM_PROFILES_COLUMN_SORT);
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (viewer->list_store_profiles), GCM_PROFILES_COLUMN_SORT, GTK_SORT_ASCENDING);
	gtk_tree_view_append_column (treeview, column);
	gtk_tree_view_column_set_expand (column, TRUE);
}

/**
 * gcm_viewer_profile_kind_to_string:
 **/
static gchar *
gcm_viewer_profile_kind_to_string (GcmProfileKind kind)
{
	if (kind == GCM_PROFILE_KIND_INPUT_DEVICE) {
		/* TRANSLATORS: this the ICC profile type */
		return _("Input device");
	}
	if (kind == GCM_PROFILE_KIND_DISPLAY_DEVICE) {
		/* TRANSLATORS: this the ICC profile type */
		return _("Display device");
	}
	if (kind == GCM_PROFILE_KIND_OUTPUT_DEVICE) {
		/* TRANSLATORS: this the ICC profile type */
		return _("Output device");
	}
	if (kind == GCM_PROFILE_KIND_DEVICELINK) {
		/* TRANSLATORS: this the ICC profile type */
		return _("Devicelink");
	}
	if (kind == GCM_PROFILE_KIND_COLORSPACE_CONVERSION) {
		/* TRANSLATORS: this the ICC profile type */
		return _("Colorspace conversion");
	}
	if (kind == GCM_PROFILE_KIND_ABSTRACT) {
		/* TRANSLATORS: this the ICC profile kind */
		return _("Abstract");
	}
	if (kind == GCM_PROFILE_KIND_NAMED_COLOR) {
		/* TRANSLATORS: this the ICC profile type */
		return _("Named color");
	}
	/* TRANSLATORS: this the ICC profile type */
	return _("Unknown");
}

/**
 * gcm_viewer_profile_colorspace_to_string:
 **/
static gchar *
gcm_viewer_profile_colorspace_to_string (GcmColorspace colorspace)
{
	if (colorspace == GCM_COLORSPACE_XYZ) {
		/* TRANSLATORS: this the ICC colorspace type */
		return _("XYZ");
	}
	if (colorspace == GCM_COLORSPACE_LAB) {
		/* TRANSLATORS: this the ICC colorspace type */
		return _("LAB");
	}
	if (colorspace == GCM_COLORSPACE_LUV) {
		/* TRANSLATORS: this the ICC colorspace type */
		return _("LUV");
	}
	if (colorspace == GCM_COLORSPACE_YCBCR) {
		/* TRANSLATORS: this the ICC colorspace type */
		return _("YCbCr");
	}
	if (colorspace == GCM_COLORSPACE_YXY) {
		/* TRANSLATORS: this the ICC colorspace type */
		return _("Yxy");
	}
	if (colorspace == GCM_COLORSPACE_RGB) {
		/* TRANSLATORS: this the ICC colorspace type */
		return _("RGB");
	}
	if (colorspace == GCM_COLORSPACE_GRAY) {
		/* TRANSLATORS: this the ICC colorspace type */
		return _("Gray");
	}
	if (colorspace == GCM_COLORSPACE_HSV) {
		/* TRANSLATORS: this the ICC colorspace type */
		return _("HSV");
	}
	if (colorspace == GCM_COLORSPACE_CMYK) {
		/* TRANSLATORS: this the ICC colorspace type */
		return _("CMYK");
	}
	if (colorspace == GCM_COLORSPACE_CMY) {
		/* TRANSLATORS: this the ICC colorspace type */
		return _("CMY");
	}
	/* TRANSLATORS: this the ICC colorspace type */
	return _("Unknown");
}

/**
 * gcm_viewer_profiles_treeview_clicked_cb:
 **/
static void
gcm_viewer_profiles_treeview_clicked_cb (GtkTreeSelection *selection, GcmViewerPrivate *viewer)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkWidget *widget;
	GcmProfile *profile;
	GcmClut *clut_trc = NULL;
	GcmClut *clut_vcgt = NULL;
	const gchar *profile_copyright;
	const gchar *profile_manufacturer;
	const gchar *profile_model ;
	const gchar *profile_datetime;
	gchar *temp;
	const gchar *filename;
	gchar *basename = NULL;
	gchar *size_text = NULL;
	GcmProfileKind profile_kind;
	GcmColorspace profile_colorspace;
	const gchar *profile_kind_text;
	const gchar *profile_colorspace_text;
	gboolean ret;
	gboolean has_vcgt;
	guint size = 0;
	guint temperature;
	guint filesize;
	gboolean show_section = FALSE;

	/* This will only work in single or browse selection mode! */
	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
		egg_debug ("no row selected");
		return;
	}

	/* get profile */
	gtk_tree_model_get (model, &iter,
			    GCM_PROFILES_COLUMN_PROFILE, &profile,
			    -1);

	/* set the preview widgets */
	if (gcm_profile_get_colorspace (profile) == GCM_COLORSPACE_RGB) {
		gcm_image_set_input_profile (GCM_IMAGE(viewer->preview_widget_input), profile);
		gcm_image_set_abstract_profile (GCM_IMAGE(viewer->preview_widget_input), NULL);
		gcm_image_set_output_profile (GCM_IMAGE(viewer->preview_widget_output), profile);
		gcm_image_set_abstract_profile (GCM_IMAGE(viewer->preview_widget_output), NULL);
		show_section = TRUE;
	} else if (gcm_profile_get_colorspace (profile) == GCM_COLORSPACE_LAB) {
		gcm_image_set_input_profile (GCM_IMAGE(viewer->preview_widget_input), NULL);
		gcm_image_set_abstract_profile (GCM_IMAGE(viewer->preview_widget_input), profile);
		gcm_image_set_output_profile (GCM_IMAGE(viewer->preview_widget_output), NULL);
		gcm_image_set_abstract_profile (GCM_IMAGE(viewer->preview_widget_output), profile);
		show_section = TRUE;
	} else {
		gcm_image_set_input_profile (GCM_IMAGE(viewer->preview_widget_input), NULL);
		gcm_image_set_abstract_profile (GCM_IMAGE(viewer->preview_widget_input), NULL);
		gcm_image_set_output_profile (GCM_IMAGE(viewer->preview_widget_output), NULL);
		gcm_image_set_abstract_profile (GCM_IMAGE(viewer->preview_widget_output), NULL);
	}

	/* setup cie widget */
	gcm_cie_widget_set_from_profile (viewer->cie_widget, profile);

	/* get curve data */
	clut_trc = gcm_profile_generate_curve (profile, 256);

	/* only show if there is useful information */
	if (clut_trc != NULL)
		size = gcm_clut_get_size (clut_trc);
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "vbox_trc_axis"));
	if (size > 0) {
		g_object_set (viewer->trc_widget,
			      "clut", clut_trc,
			      NULL);
	} else {
		gtk_widget_hide (widget);
	}

	/* get vcgt data */
	clut_vcgt = gcm_profile_generate_vcgt (profile, 256);

	/* only show if there is useful information */
	if (clut_vcgt != NULL)
		size = gcm_clut_get_size (clut_vcgt);
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "vbox_vcgt_axis"));
	if (size > 0) {
		g_object_set (viewer->vcgt_widget,
			      "clut", clut_vcgt,
			      NULL);
	} else {
		gtk_widget_hide (widget);
	}

	/* set kind */
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "hbox_type"));
	profile_kind = gcm_profile_get_kind (profile);
	if (profile_kind == GCM_PROFILE_KIND_UNKNOWN) {
		gtk_widget_hide (widget);
	} else {
		gtk_widget_show (widget);
		widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "label_type"));
		profile_kind_text = gcm_viewer_profile_kind_to_string (profile_kind);
		gtk_label_set_label (GTK_LABEL (widget), profile_kind_text);
	}

	/* set colorspace */
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "hbox_colorspace"));
	profile_colorspace = gcm_profile_get_colorspace (profile);
	if (profile_colorspace == GCM_COLORSPACE_UNKNOWN) {
		gtk_widget_hide (widget);
	} else {
		gtk_widget_show (widget);
		widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "label_colorspace"));
		profile_colorspace_text = gcm_viewer_profile_colorspace_to_string (profile_colorspace);
		gtk_label_set_label (GTK_LABEL (widget), profile_colorspace_text);
	}

	/* set vcgt */
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "hbox_vcgt"));
	gtk_widget_set_visible (widget, (profile_kind == GCM_PROFILE_KIND_DISPLAY_DEVICE));
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "label_vcgt"));
	has_vcgt = gcm_profile_get_has_vcgt (profile);
	if (has_vcgt) {
		/* TRANSLATORS: if the device has a VCGT profile */
		gtk_label_set_label (GTK_LABEL (widget), _("Yes"));
	} else {
		/* TRANSLATORS: if the device has a VCGT profile */
		gtk_label_set_label (GTK_LABEL (widget), _("No"));
	}

	/* set basename */
	filename = gcm_profile_get_filename (profile);
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "label_filename"));
	basename = g_path_get_basename (filename);
	temp = g_markup_escape_text (basename, -1);
	gtk_label_set_label (GTK_LABEL (widget), temp);
	g_free (temp);

	/* set whitepoint */
	temperature = gcm_profile_get_temperature (profile);
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "label_temp"));
	temp = g_strdup_printf ("%i°K", temperature);
	gtk_label_set_label (GTK_LABEL (widget), temp);
	g_free (temp);
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "hbox_temp"));
	gtk_widget_set_visible (widget, (temperature > 0));

	/* set size */
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "hbox_size"));
	filesize = gcm_profile_get_size (profile);
	if (filesize == 0) {
		gtk_widget_hide (widget);
	} else {
		gtk_widget_show (widget);
		widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "label_size"));
		size_text = g_format_size_for_display (filesize);
		gtk_label_set_label (GTK_LABEL (widget), size_text);
	}

	/* set new copyright */
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "hbox_copyright"));
	profile_copyright = gcm_profile_get_copyright (profile);
	if (profile_copyright == NULL) {
		gtk_widget_hide (widget);
	} else {
		gtk_widget_show (widget);
		widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "label_copyright"));
		temp = gcm_utils_linkify (profile_copyright);
		gtk_label_set_label (GTK_LABEL (widget), temp);
		g_free (temp);
	}

	/* set new manufacturer */
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "hbox_profile_manufacturer"));
	profile_manufacturer = gcm_profile_get_manufacturer (profile);
	if (profile_manufacturer == NULL) {
		gtk_widget_hide (widget);
	} else {
		gtk_widget_show (widget);
		widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "label_profile_manufacturer"));
		temp = gcm_utils_linkify (profile_manufacturer);
		gtk_label_set_label (GTK_LABEL (widget), temp);
		g_free (temp);
	}

	/* set new model */
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "hbox_profile_model"));
	profile_model = gcm_profile_get_model (profile);
	if (profile_model == NULL) {
		gtk_widget_hide (widget);
	} else {
		gtk_widget_show (widget);
		widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "label_profile_model"));
		gtk_label_set_label (GTK_LABEL(widget), profile_model);
	}

	/* set new datetime */
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "hbox_datetime"));
	profile_datetime = gcm_profile_get_datetime (profile);
	if (profile_datetime == NULL) {
		gtk_widget_hide (widget);
	} else {
		gtk_widget_show (widget);
		widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "label_datetime"));
		gtk_label_set_label (GTK_LABEL(widget), profile_datetime);
	}

	/* set delete sensitivity */
	ret = gcm_profile_get_can_delete (profile);
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "button_profile_delete"));
	gtk_widget_set_sensitive (widget, ret);
	if (ret) {
		/* TRANSLATORS: this is the tooltip when the profile can be deleted */
		gtk_widget_set_tooltip_text (widget, _("Delete this profile"));
	} else {
		/* TRANSLATORS: this is the tooltip when the profile cannot be deleted */
		gtk_widget_set_tooltip_text (widget, _("This profile cannot be deleted"));
	}

	/* should we show the pane at all */
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "vbox_graph"));
	gtk_widget_set_visible (widget, show_section);
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "vbox_profile_info"));
	gtk_widget_set_visible (widget, TRUE);

	if (clut_trc != NULL)
		g_object_unref (clut_trc);
	if (clut_vcgt != NULL)
		g_object_unref (clut_vcgt);
	g_free (size_text);
	g_free (basename);
}

/**
 * gcm_viewer_set_combo_simple_text:
 **/
static void
gcm_viewer_set_combo_simple_text (GtkWidget *combo_box)
{
	GtkCellRenderer *renderer;
	GtkListStore *store;

	store = gtk_list_store_new (4, G_TYPE_STRING, GCM_TYPE_PROFILE, G_TYPE_UINT, G_TYPE_STRING);
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (store), GCM_VIEWER_COMBO_COLUMN_SORTABLE, GTK_SORT_ASCENDING);
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
					"text", GCM_VIEWER_COMBO_COLUMN_TEXT,
					NULL);
}

/**
 * gcm_viewer_startup_phase1_idle_cb:
 **/
static gboolean
gcm_viewer_startup_phase1_idle_cb (GcmViewerPrivate *viewer)
{
	gboolean ret;
	GtkWidget *widget;
	GtkTreeSelection *selection;
	GtkTreePath *path;
	GcmProfileSearchFlags search_flags = GCM_PROFILE_STORE_SEARCH_ALL;

	/* volume checking is optional */
	ret = g_settings_get_boolean (viewer->settings, GCM_SETTINGS_USE_PROFILES_FROM_VOLUMES);
	if (!ret)
		search_flags &= ~GCM_PROFILE_STORE_SEARCH_VOLUMES;

	/* search the disk for profiles */
	gcm_profile_store_search (viewer->profile_store, search_flags);
	g_signal_connect (viewer->profile_store, "changed", G_CALLBACK(gcm_viewer_profile_store_changed_cb), viewer);

	/* update list of profiles */
	gcm_viewer_update_profile_list (viewer);

	/* select a profile to display */
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "treeview_profiles"));
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
	path = gtk_tree_path_new_from_string ("0");
	gtk_tree_selection_select_path (selection, path);
	gtk_tree_path_free (path);

	return FALSE;
}

/**
 * gcm_viewer_setup_drag_and_drop:
 **/
static void
gcm_viewer_setup_drag_and_drop (GtkWidget *widget)
{
	GtkTargetEntry entry;

	/* setup a dummy entry */
	entry.target = g_strdup ("text/plain");
	entry.flags = GTK_TARGET_OTHER_APP;
	entry.info = 0;

	gtk_drag_dest_set (widget, GTK_DEST_DEFAULT_ALL, &entry, 1, GDK_ACTION_MOVE | GDK_ACTION_COPY);
	g_free (entry.target);
}

/**
 * gcm_viewer_profile_store_changed_cb:
 **/
static void
gcm_viewer_profile_store_changed_cb (GcmProfileStore *profile_store, GcmViewerPrivate *viewer)
{
	/* clear and update the profile list */
	gcm_viewer_update_profile_list (viewer);
}

/**
 * gcm_viewer_graph_combo_changed_cb:
 **/
static void
gcm_viewer_graph_combo_changed_cb (GtkWidget *widget, GcmViewerPrivate *viewer)
{
	gint active;

	/* no selection */
	active = gtk_combo_box_get_active (GTK_COMBO_BOX(widget));
	if (active == -1)
		return;

	/* hide or show the correct graphs */
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "hbox_graph_widgets"));
	gtk_widget_set_visible (widget, active != 0);

	/* hide or show the correct graphs */
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "vbox_cie_axis"));
	gtk_widget_set_visible (widget, active == GCM_VIEWER_CIE_1931);

	/* hide or show the correct graphs */
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "vbox_trc_axis"));
	gtk_widget_set_visible (widget, active == GCM_VIEWER_TRC);

	/* hide or show the correct graphs */
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "vbox_vcgt_axis"));
	gtk_widget_set_visible (widget, active == GCM_VIEWER_VCGT);

	/* hide or show the correct graphs */
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "vbox_preview_input"));
	gtk_widget_set_visible (widget, active == GCM_VIEWER_PREVIEW_INPUT);

	/* hide or show the correct graphs */
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "vbox_preview_output"));
	gtk_widget_set_visible (widget, active == GCM_VIEWER_PREVIEW_OUTPUT);

	/* hide or show the buttons */
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "button_image_prev"));
	gtk_widget_set_visible (widget, (active == GCM_VIEWER_PREVIEW_INPUT) ||
					(active == GCM_VIEWER_PREVIEW_OUTPUT));
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "button_image_next"));
	gtk_widget_set_visible (widget, (active == GCM_VIEWER_PREVIEW_INPUT) ||
					(active == GCM_VIEWER_PREVIEW_OUTPUT));

	/* save to GSettings */
	g_settings_set_enum (viewer->settings, GCM_SETTINGS_PROFILE_GRAPH_TYPE, active);
}

/**
 * gcm_viewer_setup_graph_combobox:
 **/
static void
gcm_viewer_setup_graph_combobox (GcmViewerPrivate *viewer, GtkWidget *widget)
{
	gint active;

	/* TRANSLATORS: combo-entry, no graph selected to be shown */
	gtk_combo_box_append_text (GTK_COMBO_BOX(widget), _("None"));

	/* TRANSLATORS: combo-entry, this is a graph plot type (look it up on google...) */
	gtk_combo_box_append_text (GTK_COMBO_BOX(widget), _("CIE 1931 xy"));

	/* TRANSLATORS: combo-entry, this is a graph plot type (what goes in, v.s. what goes out) */
	gtk_combo_box_append_text (GTK_COMBO_BOX(widget), _("Transfer response curve"));

	/* TRANSLATORS: combo-entry, this is a graph plot type (what data we snd the graphics card) */
	gtk_combo_box_append_text (GTK_COMBO_BOX(widget), _("Video card gamma table"));

	/* TRANSLATORS: combo-entry, this is a preview image of what the profile looks like */
	gtk_combo_box_append_text (GTK_COMBO_BOX(widget), _("Image preview (from sRGB)"));
	gtk_combo_box_append_text (GTK_COMBO_BOX(widget), _("Image preview (to sRGB)"));

	/* get from settings */
	active = g_settings_get_enum (viewer->settings, GCM_SETTINGS_PROFILE_GRAPH_TYPE);
	gtk_combo_box_set_active (GTK_COMBO_BOX (widget), active);
}

/**
 * main:
 **/
int
main (int argc, char **argv)
{
	guint retval = 0;
	GOptionContext *context;
	GtkWidget *main_window;
	GtkWidget *widget;
	guint xid = 0;
	GError *error = NULL;
	GtkTreeSelection *selection;
	GdkScreen *screen;
	GcmViewerPrivate *viewer;

	const GOptionEntry options[] = {
		{ "parent-window", 'p', 0, G_OPTION_ARG_INT, &xid,
		  /* TRANSLATORS: we can make this modal (stay on top of) another window */
		  _("Set the parent window to make this modal"), NULL },
		{ NULL}
	};

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	gtk_init (&argc, &argv);

	context = g_option_context_new ("gnome-color-manager prefs program");
	g_option_context_add_main_entries (context, options, NULL);
	g_option_context_add_group (context, egg_debug_get_option_group ());
	g_option_context_add_group (context, gtk_get_option_group (TRUE));
	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);

	viewer = g_new0 (GcmViewerPrivate, 1);

	/* ensure single instance */
	viewer->application = gtk_application_new ("org.gnome.ColorManager.Profile", &argc, &argv);

	/* setup defaults */
	viewer->settings = g_settings_new (GCM_SETTINGS_SCHEMA);

	/* get UI */
	viewer->builder = gtk_builder_new ();
	retval = gtk_builder_add_from_file (viewer->builder, GCM_DATA "/gcm-viewer.ui", &error);
	if (retval == 0) {
		egg_warning ("failed to load ui: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* add application specific icons to search path */
	gtk_icon_theme_append_search_path (gtk_icon_theme_get_default (),
	                                   GCM_DATA G_DIR_SEPARATOR_S "icons");

	/* maintain a list of profiles */
	viewer->profile_store = gcm_profile_store_new ();

	/* create list stores */
	viewer->list_store_profiles = gtk_list_store_new (GCM_PROFILES_COLUMN_LAST, G_TYPE_STRING,
						  G_TYPE_STRING, G_TYPE_STRING, GCM_TYPE_PROFILE);

	/* create profile tree view */
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "treeview_profiles"));
	gtk_tree_view_set_model (GTK_TREE_VIEW (widget),
				 GTK_TREE_MODEL (viewer->list_store_profiles));
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
	g_signal_connect (selection, "changed",
			  G_CALLBACK (gcm_viewer_profiles_treeview_clicked_cb), viewer);

	/* add columns to the tree view */
	gcm_viewer_add_profiles_columns (viewer, GTK_TREE_VIEW (widget));
	gtk_tree_view_columns_autosize (GTK_TREE_VIEW (widget));

	main_window = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "dialog_viewer"));
	gtk_application_add_window (viewer->application, GTK_WINDOW (main_window));

	/* Hide window first so that the dialogue resizes itself without redrawing */
	gtk_widget_hide (main_window);
	gtk_window_set_icon_name (GTK_WINDOW (main_window), GCM_STOCK_ICON);
	g_signal_connect (main_window, "delete_event",
			  G_CALLBACK (gcm_viewer_delete_event_cb), viewer);
	g_signal_connect (main_window, "drag-data-received",
			  G_CALLBACK (gcm_viewer_drag_data_received_cb), viewer);
	gcm_viewer_setup_drag_and_drop (GTK_WIDGET(main_window));

	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "button_close"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gcm_viewer_close_cb), viewer);
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "button_preferences"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gcm_viewer_preferences_cb), viewer);
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "button_help"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gcm_viewer_help_cb), viewer);
	gtk_widget_set_sensitive (widget, FALSE);
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "button_profile_delete"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gcm_viewer_profile_delete_cb), viewer);
	gtk_widget_set_sensitive (widget, FALSE);
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "button_profile_import"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gcm_viewer_profile_import_cb), viewer);

	/* image next/prev */
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "button_image_next"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gcm_viewer_image_next_cb), viewer);
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "button_image_prev"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gcm_viewer_image_prev_cb), viewer);

	/* hidden until a profile is selected */
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "vbox_graph"));
	gtk_widget_set_visible (widget, FALSE);
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "vbox_profile_info"));
	gtk_widget_set_visible (widget, FALSE);

	/* hide widgets by default */
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "combobox_graph"));
	gcm_viewer_set_combo_simple_text (widget);
	gcm_viewer_setup_graph_combobox (viewer, widget);
	g_signal_connect (widget, "changed",
			  G_CALLBACK (gcm_viewer_graph_combo_changed_cb), viewer);

	/* use cie widget */
	viewer->cie_widget = gcm_cie_widget_new ();
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "hbox_cie_widget"));
	gtk_box_pack_start (GTK_BOX(widget), viewer->cie_widget, TRUE, TRUE, 0);
	gtk_box_reorder_child (GTK_BOX(widget), viewer->cie_widget, 0);

	/* use trc widget */
	viewer->trc_widget = gcm_trc_widget_new ();
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "hbox_trc_widget"));
	gtk_box_pack_start (GTK_BOX(widget), viewer->trc_widget, TRUE, TRUE, 0);
	gtk_box_reorder_child (GTK_BOX(widget), viewer->trc_widget, 0);

	/* use vcgt widget */
	viewer->vcgt_widget = gcm_trc_widget_new ();
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "hbox_vcgt_widget"));
	gtk_box_pack_start (GTK_BOX(widget), viewer->vcgt_widget, TRUE, TRUE, 0);
	gtk_box_reorder_child (GTK_BOX(widget), viewer->vcgt_widget, 0);

	/* use preview input */
	viewer->preview_widget_input = GTK_WIDGET (gcm_image_new ());
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "vbox_preview_input"));
	gtk_box_pack_end (GTK_BOX(widget), viewer->preview_widget_input, FALSE, FALSE, 0);
	gcm_viewer_set_example_image (viewer, GTK_IMAGE (viewer->preview_widget_input));
	gtk_widget_set_visible (viewer->preview_widget_input, TRUE);

	/* use preview output */
	viewer->preview_widget_output = GTK_WIDGET (gcm_image_new ());
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "vbox_preview_output"));
	gtk_box_pack_end (GTK_BOX(widget), viewer->preview_widget_output, FALSE, FALSE, 0);
	gcm_viewer_set_example_image (viewer, GTK_IMAGE (viewer->preview_widget_output));
	gtk_widget_set_visible (viewer->preview_widget_output, TRUE);

	/* do we set a default size to make the window larger? */
	screen = gdk_screen_get_default ();
	if (gdk_screen_get_width (screen) < 1024 ||
	    gdk_screen_get_height (screen) < 768) {
		gtk_widget_set_size_request (viewer->cie_widget, 50, 50);
		gtk_widget_set_size_request (viewer->trc_widget, 50, 50);
		gtk_widget_set_size_request (viewer->vcgt_widget, 50, 50);
		gtk_widget_set_size_request (viewer->preview_widget_input, 50, 50);
		gtk_widget_set_size_request (viewer->preview_widget_output, 50, 50);
	} else {
		gtk_widget_set_size_request (viewer->cie_widget, 200, 200);
		gtk_widget_set_size_request (viewer->trc_widget, 200, 200);
		gtk_widget_set_size_request (viewer->vcgt_widget, 200, 200);
		gtk_widget_set_size_request (viewer->preview_widget_input, 200, 200);
		gtk_widget_set_size_request (viewer->preview_widget_output, 200, 200);
	}

	/* show main UI */
	gtk_widget_show (main_window);

	/* set the parent window if it is specified */
	if (xid != 0) {
		egg_debug ("Setting xid %i", xid);
		gcm_window_set_parent_xid (GTK_WINDOW (main_window), xid);
	}

	/* refresh UI */
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "combobox_graph"));
	gcm_viewer_graph_combo_changed_cb (widget, viewer);

	/* do all this after the window has been set up */
	g_idle_add ((GSourceFunc) gcm_viewer_startup_phase1_idle_cb, viewer);

	/* wait */
	gtk_application_run (viewer->application);
out:
	g_object_unref (viewer->application);
	if (viewer->settings != NULL)
		g_object_unref (viewer->settings);
	if (viewer->builder != NULL)
		g_object_unref (viewer->builder);
	if (viewer->profile_store != NULL)
		g_object_unref (viewer->profile_store);
	g_free (viewer);
	return retval;
}

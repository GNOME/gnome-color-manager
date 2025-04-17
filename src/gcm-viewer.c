/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009-2012 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <glib/gstdio.h>
#include <locale.h>
#include <colord.h>
#include <math.h>

#include "gcm-cell-renderer-profile-text.h"
#include "gcm-cell-renderer-color.h"
#include "gcm-cie-widget.h"
#include "gcm-trc-widget.h"
#include "gcm-utils.h"
#include "gcm-debug.h"

typedef struct {
	GtkBuilder	*builder;
	GtkApplication	*application;
	GtkListStore	*list_store_profiles;
	CdClient	*client;
	GtkWidget	*cie_widget;
	GtkWidget	*trc_widget;
	GtkWidget	*vcgt_widget;
	GtkWidget	*preview_widget_input;
	GtkWidget	*preview_widget_output;
	guint		 example_index;
	gchar		*profile_id;
	gchar		*filename;
	guint		 xid;
	const gchar	*lang;
	GtkListStore	*liststore_nc;
	GtkListStore	*liststore_metadata;
	gboolean	 clearing_store;
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
	GCM_NAMED_COLORS_COLUMN_TITLE,
	GCM_NAMED_COLORS_COLUMN_SORT,
	GCM_NAMED_COLORS_COLUMN_COLOR,
	GCM_NAMED_COLORS_COLUMN_LAST
};

enum {
	GCM_METADATA_COLUMN_KEY,
	GCM_METADATA_COLUMN_VALUE,
	GCM_METADATA_COLUMN_LAST
};

enum {
	GCM_PROFILES_COLUMN_ID,
	GCM_PROFILES_COLUMN_SORT,
	GCM_PROFILES_COLUMN_ICON,
	GCM_PROFILES_COLUMN_PROFILE,
	GCM_PROFILES_COLUMN_LAST
};

#define GCM_VIEWER_APPLICATION_ID		"org.gnome.ColorProfileViewer"
#define GCM_VIEWER_TREEVIEW_WIDTH		350 /* px */
#define GCM_VIEWER_MAX_EXAMPLE_IMAGES		4

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

static void
gcm_viewer_set_example_image (GcmViewerPrivate *viewer, GtkImage *image)
{
	g_autofree gchar *filename;
	g_autofree gchar *path = NULL;
	g_autoptr(GdkPixbuf) pixbuf = NULL;
	g_autoptr(GError) error = NULL;

	filename = g_strdup_printf ("viewer-example-%02u.png", viewer->example_index);
	path = g_build_filename (PKGDATADIR, "figures", filename, NULL);
	pixbuf = gdk_pixbuf_new_from_file (path, &error);
	if (pixbuf == NULL) {
		g_warning ("failed to load %s: %s", filename, error->message);
		return;
	}
	gtk_image_set_from_pixbuf (image, pixbuf);
}

static void
gcm_viewer_image_next_cb (GtkWidget *widget, GcmViewerPrivate *viewer)
{
	viewer->example_index++;
	if (viewer->example_index == GCM_VIEWER_MAX_EXAMPLE_IMAGES)
		viewer->example_index = 0;
	gcm_viewer_set_example_image (viewer, GTK_IMAGE (viewer->preview_widget_input));
	gcm_viewer_set_example_image (viewer, GTK_IMAGE (viewer->preview_widget_output));
}

static void
gcm_viewer_image_prev_cb (GtkWidget *widget, GcmViewerPrivate *viewer)
{
	if (viewer->example_index == 0)
		viewer->example_index = GCM_VIEWER_MAX_EXAMPLE_IMAGES;
	viewer->example_index--;
	gcm_viewer_set_example_image (viewer, GTK_IMAGE (viewer->preview_widget_input));
	gcm_viewer_set_example_image (viewer, GTK_IMAGE (viewer->preview_widget_output));
}

static const gchar *
gcm_viewer_profile_kind_to_icon_name (CdProfileKind kind)
{
	if (kind == CD_PROFILE_KIND_DISPLAY_DEVICE)
		return "video-display";
	if (kind == CD_PROFILE_KIND_INPUT_DEVICE)
		return "scanner";
	if (kind == CD_PROFILE_KIND_OUTPUT_DEVICE)
		return "printer";
	if (kind == CD_PROFILE_KIND_COLORSPACE_CONVERSION)
		return "view-refresh";
	if (kind == CD_PROFILE_KIND_ABSTRACT)
		return "insert-link";
	if (kind == CD_PROFILE_KIND_NAMED_COLOR)
		return "emblem-photos";
	return "image-missing";
}

static const gchar *
gcm_viewer_profile_get_sort_string (CdProfile *profile)
{
	const gchar *id;
	static gchar sort[] = {'\0', '\0', '\0'};
	CdProfileKind kind;

	/* first order, profile kind */
	kind = cd_profile_get_kind (profile);
	if (kind == CD_PROFILE_KIND_DISPLAY_DEVICE)
		sort[0] = '1';
	else if (kind == CD_PROFILE_KIND_INPUT_DEVICE)
		sort[0] = '2';
	else if (kind == CD_PROFILE_KIND_OUTPUT_DEVICE)
		sort[0] = '3';
	else if (kind == CD_PROFILE_KIND_NAMED_COLOR)
		sort[0] = '4';
	else if (kind == CD_PROFILE_KIND_ABSTRACT)
		sort[0] = '5';
	else
		sort[0] = '9';

	/* second order, profile data source */
	id = cd_profile_get_metadata_item (profile,
					   CD_PROFILE_METADATA_DATA_SOURCE);
	if (g_strcmp0 (id, CD_PROFILE_METADATA_DATA_SOURCE_CALIB) == 0)
		sort[1] = '1';
	else if (g_strcmp0 (id, CD_PROFILE_METADATA_DATA_SOURCE_EDID) == 0)
		sort[1] = '2';
	else if (g_strcmp0 (id, CD_PROFILE_METADATA_DATA_SOURCE_STANDARD) == 0)
		sort[1] = '3';
	else if (g_strcmp0 (id, CD_PROFILE_METADATA_DATA_SOURCE_TEST) == 0)
		sort[1] = '4';
	else
		sort[1] = '0';

	return sort;
}

static void
gcm_viewer_update_profile_connect_cb (GObject *source_object,
				      GAsyncResult *res,
				      gpointer user_data)
{
	CdProfileKind profile_kind;
	const gchar *description;
	const gchar *filename;
	const gchar *icon_name;
	GtkTreeIter iter;
	GtkTreePath *path;
	GtkTreeSelection *selection;
	GtkWidget *widget;
	CdProfile *profile = CD_PROFILE (source_object);
	GcmViewerPrivate *viewer = (GcmViewerPrivate *) user_data;
	g_autoptr(GError) error = NULL;
	g_autofree gchar *sort = NULL;

	/* connect to the profile */
	if (!cd_profile_connect_finish (profile, res, &error)) {
		g_warning ("failed to connect to profile: %s", error->message);
		return;
	}

	/* ignore profiles from other user accounts */
	if (!cd_profile_has_access (profile))
		return;

	profile_kind = cd_profile_get_kind (profile);
	icon_name = gcm_viewer_profile_kind_to_icon_name (profile_kind);
	filename = cd_profile_get_filename (profile);
	if (filename == NULL)
		return;
	description = cd_profile_get_title (profile);
	sort = g_strdup_printf ("%s%s",
				gcm_viewer_profile_get_sort_string (profile),
				description);
	g_debug ("add %s to profiles list", filename);
	gtk_list_store_append (viewer->list_store_profiles, &iter);
	gtk_list_store_set (viewer->list_store_profiles, &iter,
			    GCM_PROFILES_COLUMN_ID, filename,
			    GCM_PROFILES_COLUMN_SORT, sort,
			    GCM_PROFILES_COLUMN_ICON, icon_name,
			    GCM_PROFILES_COLUMN_PROFILE, profile,
			    -1);

	/* select a profile to display if nothing already selected */
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "treeview_profiles"));
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));

	if (!gtk_tree_selection_get_selected (selection, NULL, NULL)) {
		path = gtk_tree_path_new_from_string ("0");
		gtk_tree_selection_select_path (selection, path);
		gtk_tree_path_free (path);
	}
}

static void
gcm_viewer_update_get_profiles_cb (GObject *source_object,
				   GAsyncResult *res,
				   gpointer user_data)
{
	CdProfile *profile;
	guint i;
	GcmViewerPrivate *viewer = (GcmViewerPrivate *) user_data;
	g_autoptr(GError) error = NULL;
	g_autoptr(GPtrArray) profile_array = NULL;

	g_debug ("updating profile list");

	/* get new list */
	profile_array = cd_client_get_profiles_finish (CD_CLIENT (source_object),
						       res,
						       &error);
	if (profile_array == NULL) {
		g_warning ("failed to get profiles: %s", error->message);
		return;
	}

	/* clear existing list */
	viewer->clearing_store = TRUE;
	gtk_list_store_clear (viewer->list_store_profiles);
	viewer->clearing_store = FALSE;

	/* update each list */
	for (i = 0; i < profile_array->len; i++) {
		profile = g_ptr_array_index (profile_array, i);
		cd_profile_connect (profile,
				    NULL,
				    gcm_viewer_update_profile_connect_cb,
				    viewer);
	}
}

static void
gcm_viewer_update_profile_list (GcmViewerPrivate *viewer)
{
	g_debug ("updating profile list");

	/* get new list */
	cd_client_get_profiles (viewer->client,
				NULL,
				gcm_viewer_update_get_profiles_cb,
				viewer);
}

static void
gcm_viewer_profile_delete_cb (GtkWidget *widget, GcmViewerPrivate *viewer)
{
	CdProfile *profile;
	const gchar *filename;
	GtkResponseType response;
	GtkTreeIter iter;
	GtkTreeModel *model;
	GtkTreeSelection *selection;
	GtkWidget *dialog;
	GtkWindow *window;
	g_autoptr(GError) error = NULL;
	g_autoptr(GFile) file = NULL;

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
		return;

	/* get the selected row */
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "treeview_profiles"));
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
		g_debug ("no row selected");
		return;
	}

	/* get profile */
	gtk_tree_model_get (model, &iter,
			    GCM_PROFILES_COLUMN_PROFILE, &profile,
			    -1);

	/* try to remove file */
	filename = cd_profile_get_filename (profile);
	file = g_file_new_for_path (filename);
	if (!g_file_delete (file, NULL, &error)) {
		g_warning ("failed to be deleted: %s", error->message);
		return;
	}
}

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
					       _("_Cancel"), GTK_RESPONSE_CANCEL,
					       _("_Import"), GTK_RESPONSE_ACCEPT,
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

static gboolean
gcm_viewer_profile_import_file (GcmViewerPrivate *viewer, GFile *file)
{
	g_autoptr(GError) error = NULL;
	g_autoptr(CdProfile) profile_tmp = NULL;

	/* copy icc file to users profile path */
	profile_tmp = cd_client_import_profile_sync (viewer->client,
						     file, NULL, &error);
	if (profile_tmp == NULL) {
		/* TRANSLATORS: could not read file */
		gcm_viewer_error_dialog (viewer, _("Failed to copy file"), error->message);
		return FALSE;
	}

	/* success */
	return TRUE;
}

static void
gcm_viewer_profile_import_cb (GtkWidget *widget, GcmViewerPrivate *viewer)
{
	g_autoptr(GFile) file = NULL;

	/* get new file */
	file = gcm_viewer_file_chooser_get_icc_profile (viewer);
	if (file == NULL) {
		g_warning ("failed to get filename");
		return;
	}

	/* import this */
	gcm_viewer_profile_import_file (viewer, file);
}

static void
gcm_viewer_drag_data_received_cb (GtkWidget *widget, GdkDragContext *context, gint x, gint y, GtkSelectionData *data, guint _time, GcmViewerPrivate *viewer)
{
	const guchar *filename;
	g_auto(GStrv) filenames = NULL;
	guint i;
	gboolean ret;
	gboolean success = FALSE;

	/* get filenames */
	filename = gtk_selection_data_get_data (data);
	if (filename == NULL)
		goto out;

	/* import this */
	g_debug ("dropped: %p (%s)", data, filename);

	/* split, as multiple drag targets are accepted */
	filenames = g_strsplit_set ((const gchar *)filename, "\r\n", -1);
	for (i = 0; filenames[i]!=NULL; i++) {
		g_autoptr(GFile) file = NULL;

		/* blank entry */
		if (filenames[i][0] == '\0')
			continue;

		/* convert the URI */
		file = g_file_new_for_uri (filenames[i]);

		/* try to import it */
		ret = gcm_viewer_profile_import_file (viewer, file);
		if (ret)
			success = TRUE;
	}

out:
	gtk_drag_finish (context, success, FALSE, _time);
}

static void
gcm_window_set_parent_xid (GtkWindow *window, guint32 xid)
{
	GdkDisplay *display;
	GdkWindow *parent_window;
	GdkWindow *our_window;

	display = gdk_display_get_default ();
	parent_window = gdk_x11_window_foreign_new_for_display (display, xid);
	if (parent_window == NULL) {
		g_warning ("failed to get parent window");
		return;
	}

	gtk_widget_realize (GTK_WIDGET (window));
	our_window = gtk_widget_get_window (GTK_WIDGET (window));
	if (our_window == NULL) {
		g_warning ("failed to get our window");
		return;
	}

	/* set this above our parent */
	gtk_window_set_modal (window, TRUE);
	gdk_window_set_transient_for (our_window, parent_window);
	gtk_window_set_title (window, "");
}

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
							   "profile", GCM_PROFILES_COLUMN_PROFILE,
							   NULL);
	gtk_tree_view_column_set_sort_column_id (column, GCM_PROFILES_COLUMN_SORT);
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (viewer->list_store_profiles),
					      GCM_PROFILES_COLUMN_SORT,
					      GTK_SORT_ASCENDING);
	gtk_tree_view_append_column (treeview, column);
	gtk_tree_view_column_set_expand (column, TRUE);
}

static gchar *
gcm_viewer_profile_kind_to_string (CdProfileKind kind)
{
	if (kind == CD_PROFILE_KIND_INPUT_DEVICE) {
		/* TRANSLATORS: this the ICC profile type */
		return _("Input device");
	}
	if (kind == CD_PROFILE_KIND_DISPLAY_DEVICE) {
		/* TRANSLATORS: this the ICC profile type */
		return _("Display device");
	}
	if (kind == CD_PROFILE_KIND_OUTPUT_DEVICE) {
		/* TRANSLATORS: this the ICC profile type */
		return _("Output device");
	}
	if (kind == CD_PROFILE_KIND_DEVICELINK) {
		/* TRANSLATORS: this the ICC profile type */
		return _("Devicelink");
	}
	if (kind == CD_PROFILE_KIND_COLORSPACE_CONVERSION) {
		/* TRANSLATORS: this the ICC profile type */
		return _("Colorspace conversion");
	}
	if (kind == CD_PROFILE_KIND_ABSTRACT) {
		/* TRANSLATORS: this the ICC profile kind */
		return _("Abstract");
	}
	if (kind == CD_PROFILE_KIND_NAMED_COLOR) {
		/* TRANSLATORS: this the ICC profile type */
		return _("Named color");
	}
	/* TRANSLATORS: this the ICC profile type */
	return _("Unknown");
}

static gchar *
gcm_viewer_profile_colorspace_to_string (CdColorspace colorspace)
{
	if (colorspace == CD_COLORSPACE_XYZ) {
		/* TRANSLATORS: this the ICC colorspace type */
		return _("XYZ");
	}
	if (colorspace == CD_COLORSPACE_LAB) {
		/* TRANSLATORS: this the ICC colorspace type */
		return _("LAB");
	}
	if (colorspace == CD_COLORSPACE_LUV) {
		/* TRANSLATORS: this the ICC colorspace type */
		return _("LUV");
	}
	if (colorspace == CD_COLORSPACE_YCBCR) {
		/* TRANSLATORS: this the ICC colorspace type */
		return _("YCbCr");
	}
	if (colorspace == CD_COLORSPACE_YXY) {
		/* TRANSLATORS: this the ICC colorspace type */
		return _("Yxy");
	}
	if (colorspace == CD_COLORSPACE_RGB) {
		/* TRANSLATORS: this the ICC colorspace type */
		return _("RGB");
	}
	if (colorspace == CD_COLORSPACE_GRAY) {
		/* TRANSLATORS: this the ICC colorspace type */
		return _("Gray");
	}
	if (colorspace == CD_COLORSPACE_HSV) {
		/* TRANSLATORS: this the ICC colorspace type */
		return _("HSV");
	}
	if (colorspace == CD_COLORSPACE_CMYK) {
		/* TRANSLATORS: this the ICC colorspace type */
		return _("CMYK");
	}
	if (colorspace == CD_COLORSPACE_CMY) {
		/* TRANSLATORS: this the ICC colorspace type */
		return _("CMY");
	}
	/* TRANSLATORS: this the ICC colorspace type */
	return _("Unknown");
}

static gboolean
gcm_viewer_add_named_colors (GcmViewerPrivate *viewer, CdIcc *icc)
{
	CdColorSwatch *nc;
	GPtrArray *ncs = NULL;
	GtkTreeIter iter;
	guint i;

	/* get profile named colors */
	gtk_list_store_clear (viewer->liststore_nc);
	ncs = cd_icc_get_named_colors (icc);
	for (i = 0; i < ncs->len; i++) {
		nc = g_ptr_array_index (ncs, i);
		gtk_list_store_append (viewer->liststore_nc, &iter);
		gtk_list_store_set (viewer->liststore_nc,
				    &iter,
				    GCM_NAMED_COLORS_COLUMN_SORT, "1",
				    GCM_NAMED_COLORS_COLUMN_TITLE, cd_color_swatch_get_name (nc),
				    GCM_NAMED_COLORS_COLUMN_COLOR, cd_color_swatch_get_value (nc),
				    -1);
	}
	g_ptr_array_unref (ncs);
	return TRUE;
}

struct {
	const gchar *key;
	const gchar *localised;
} metadata_keys[] = {
	/* TRANSLATORS: e.g. sRGB or adbode RGB */
	{ CD_PROFILE_METADATA_STANDARD_SPACE,		N_("Standard space") },
	/* TRANSLATORS: the raw EDID MD5 of the display device */
	{ CD_PROFILE_METADATA_EDID_MD5,			N_("Display checksum") },
	{ CD_PROFILE_METADATA_EDID_MODEL,		N_("Display model") },
	{ CD_PROFILE_METADATA_EDID_SERIAL,		N_("Display serial number") },
	/* TRANSLATORS: the PNPID is a three letter code, e.g. DEL */
	{ CD_PROFILE_METADATA_EDID_MNFT,		N_("Display PNPID") },
	/* TRANSLATORS: who made the display */
	{ CD_PROFILE_METADATA_EDID_VENDOR,		N_("Display vendor") },
	/* TRANSLATORS: the raw profile MD5 */
	{ CD_PROFILE_METADATA_FILE_CHECKSUM,		N_("File checksum") },
	/* TRANSLATORS: the color framework, e.g. 'colord' */
	{ CD_PROFILE_METADATA_CMF_PRODUCT,		N_("Framework product") },
	/* TRANSLATORS: the framework binary, e.g. gcm-viewer */
	{ CD_PROFILE_METADATA_CMF_BINARY,		N_("Framework program") },
	/* TRANSLATORS: the framework release, e.g. '1.22' */
	{ CD_PROFILE_METADATA_CMF_VERSION,		N_("Framework version") },
	/* TRANSLATORS: Where the profile data came from, e.g. 'test' or 'standard' */
	{ CD_PROFILE_METADATA_DATA_SOURCE,		N_("Data source type") },
	/* TRANSLATORS: how the qualifier is formatted, e.g. 'ColorSpace.Paper.DPI' */
	{ CD_PROFILE_METADATA_MAPPING_FORMAT,		N_("Mapping format") },
	/* TRANSLATORS: The qualifiers for the profile, e.g. 'RGB.Glossy.300dpi' */
	{ CD_PROFILE_METADATA_MAPPING_QUALIFIER,	N_("Mapping qualifier") },
	/* TRANSLATORS: The original device the profile was made for */
	{ CD_PROFILE_METADATA_MAPPING_DEVICE_ID,	N_("Mapping device") },
	/* TRANSLATORS: The average error when making the profile */
	{ CD_PROFILE_METADATA_ACCURACY_DE76_AVG,	N_("Delta-E average") },
	/* TRANSLATORS: the maximum error when making the profile */
	{ CD_PROFILE_METADATA_ACCURACY_DE76_MAX,	N_("Delta-E maximum") },
	/* TRANSLATORS: the RMS error when making the profile */
	{ CD_PROFILE_METADATA_ACCURACY_DE76_RMS,	N_("Delta-E RMS") },
	/* TRANSLATORS: The device name, e.g. 'huey' */
	{ CD_PROFILE_METADATA_MEASUREMENT_DEVICE,	N_("Calibration device") },
	/* TRANSLATORS: screen type e.g. 'glossy' or 'matte' */
	{ CD_PROFILE_METADATA_SCREEN_SURFACE,		N_("Screen surface finish") },
	/* TRANSLATORS: e.g. DVI or VGA */
	{ CD_PROFILE_METADATA_CONNECTION_TYPE,		N_("Connection type") },
	/* TRANSLATORS: e.g. Screen brightness in percent */
	{ CD_PROFILE_METADATA_SCREEN_BRIGHTNESS,	N_("Screen brightness") },
	/* TRANSLATORS: e.g. the 3D volume of the gamut graph */
	{ "GAMUT_volume",				N_("Gamut volume") },
	/* TRANSLATORS: e.g. what proportion of sRGB is covered */
	{ "GAMUT_coverage(srgb)",			N_("sRGB coverage") },
	/* TRANSLATORS: e.g. what proportion of AdobeRGB is covered */
	{ "GAMUT_coverage(adobe-rgb)",			N_("Adobe RGB coverage") },
	{ NULL, NULL }
};

static const gchar *
gcm_viewer_get_localised_metadata_key (const gchar *key)
{
	guint i;
	for (i = 0; metadata_keys[i].key != NULL; i++) {
		if (g_strcmp0 (key, metadata_keys[i].key) == 0)
			return metadata_keys[i].localised;
	}
	return key;
}

/* other CMS's add keys that we don't want to show */
static gboolean
gcm_viewer_is_blacklisted_metadata_key (const gchar *key)
{
	guint i;
	const gchar *blacklist[] = { "prefix",
				     "EDID_blue*",
				     "EDID_date",
				     "EDID_gamma",
				     "EDID_green*",
				     "EDID_mnft_id",
				     "EDID_model_id",
				     "EDID_red*",
				     "EDID_white*",
				     "OSD_*",			/* OSD controls... */
				     "vcgt",
				     "model",
				     "OPENICC_automatic_generated",
				     NULL };
	for (i = 0; blacklist[i] != NULL; i++) {
		if (g_regex_match_simple (blacklist[i],
					  key, 0, 0)) {
			return FALSE;
		}
	}
	return TRUE;
}

static gboolean
gcm_viewer_add_metadata (GcmViewerPrivate *viewer,
			 CdProfile *profile)
{
	GList *l;
	GtkTreeIter iter;
	const gchar *value;
	const gchar *key;
	g_autoptr(GError) error = NULL;
	g_autoptr(GHashTable) metadata = NULL;
	g_autoptr(GList) keys = NULL;

	/* clear existing */
	gtk_list_store_clear (viewer->liststore_metadata);

	/* get all the metadata entries */
	metadata = cd_profile_get_metadata (profile);
	if (metadata == NULL) {
		g_warning ("failed to get metadata");
		return FALSE;
	}

	/* add items */
	keys = g_hash_table_get_keys (metadata);
	if (keys == NULL)
		return FALSE;
	for (l = keys; l != NULL; l = l->next) {
		if (!gcm_viewer_is_blacklisted_metadata_key (l->data))
			continue;
		key = gcm_viewer_get_localised_metadata_key ((gchar *) l->data);
		value = g_hash_table_lookup (metadata, l->data);
		if (value == NULL || value[0] == '\0')
			continue;
		g_debug ("Adding '%s', '%s'", key, value);
		gtk_list_store_append (viewer->liststore_metadata, &iter);
		gtk_list_store_set (viewer->liststore_metadata,
				    &iter,
				    GCM_METADATA_COLUMN_KEY, key,
				    GCM_METADATA_COLUMN_VALUE, value,
				    -1);
	}

	/* success */
	return TRUE;
}

static const gchar *
cd_icc_warning_to_string (CdProfileWarning kind_enum)
{
	const gchar *kind = NULL;
	switch (kind_enum) {
	case CD_PROFILE_WARNING_DESCRIPTION_MISSING:
		/* TRANSLATORS: the profile is broken */
		kind = _("No description has been set");
		break;
	case CD_PROFILE_WARNING_COPYRIGHT_MISSING:
		/* TRANSLATORS: the profile is broken */
		kind = _("No copyright has been set");
		break;
	case CD_PROFILE_WARNING_VCGT_NON_MONOTONIC:
		/* TRANSLATORS: the profile is broken */
		kind = _("The display compensation table is invalid");
		break;
	case CD_PROFILE_WARNING_SCUM_DOT:
		/* TRANSLATORS: the profile is broken */
		kind = _("A scum dot is present for media white");
		break;
	case CD_PROFILE_WARNING_GRAY_AXIS_INVALID:
		/* TRANSLATORS: the profile is broken */
		kind = _("The gray axis contains significant amounts of color");
		break;
	case CD_PROFILE_WARNING_GRAY_AXIS_NON_MONOTONIC:
		/* TRANSLATORS: the profile is broken */
		kind = _("The gray axis is non-monotonic");
		break;
	case CD_PROFILE_WARNING_PRIMARIES_INVALID:
		/* TRANSLATORS: the profile is broken */
		kind = _("One or more of the primaries are invalid");
		break;
	case CD_PROFILE_WARNING_PRIMARIES_NON_ADDITIVE:
		/* TRANSLATORS: the profile is broken */
		kind = _("The primaries do not add to white");
		break;
	case CD_PROFILE_WARNING_PRIMARIES_UNLIKELY:
		/* TRANSLATORS: the profile is broken */
		kind = _("One or more of the primaries is unlikely");
		break;
	case CD_PROFILE_WARNING_WHITEPOINT_INVALID:
		/* TRANSLATORS: the profile is broken */
		kind = _("The white is not D50 white");
		break;
	case CD_PROFILE_WARNING_WHITEPOINT_UNLIKELY:
		/* TRANSLATORS: the profile is broken */
		kind = _("The whitepoint temperature is unlikely");
		break;
	default:
		/* TRANSLATORS: the profile is broken */
		kind = _("Unknown warning type");
		break;
	}
	return kind;
}

static void
gcm_viewer_set_profile (GcmViewerPrivate *viewer, CdProfile *profile)
{
	GtkWidget *widget;
	GtkWindow *window;
	const gchar *profile_copyright;
	const gchar *profile_manufacturer;
	const gchar *profile_model ;
	GDateTime *created;
	const gchar *profile_title;
	gchar *temp;
	const gchar *filename;
	CdProfileKind profile_kind;
	CdColorspace profile_colorspace;
	const gchar *profile_kind_text;
	const gchar *profile_colorspace_text;
	gboolean ret;
	gboolean has_vcgt;
	guint size;
	guint temperature;
	guint filesize;
	gboolean show_section_to = FALSE;
	gboolean show_section_from = FALSE;
	gchar **warnings;
	guint i;
	CdProfileWarning warning;
	GString *str;
	g_autofree gchar *size_text = NULL;
	g_autoptr(CdIcc) icc = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(GPtrArray) clut_trc = NULL;
	g_autoptr(GPtrArray) clut_vcgt = NULL;

	/* connect to the profile */
	ret = cd_profile_connect_sync (profile, NULL, &error);
	if (!ret) {
		g_warning ("failed to connect to profile: %s",
			   error->message);
		return;
	}

	/* set the preview widgets */
	icc = cd_profile_load_icc (profile, CD_ICC_LOAD_FLAGS_ALL, NULL, &error);
	if (icc == NULL) {
		g_warning ("failed to load ICC profile: %s",
			   error->message);
		return;
	}

	/* convert the image if required */
	if (cd_profile_get_colorspace (profile) == CD_COLORSPACE_RGB &&
	    cd_profile_get_kind (profile) != CD_PROFILE_KIND_NAMED_COLOR) {
		show_section_to = TRUE;
		show_section_from = TRUE;
		/* profile -> sRGB */
		gcm_utils_image_convert (GTK_IMAGE (viewer->preview_widget_input),
					 icc, NULL, NULL, NULL);
		/* sRGB -> profile */
		gcm_utils_image_convert (GTK_IMAGE (viewer->preview_widget_output),
					 NULL, NULL, icc, NULL);
	} else if (cd_profile_get_colorspace (profile) == CD_COLORSPACE_LAB &&
		   cd_profile_get_kind (profile) != CD_PROFILE_KIND_NAMED_COLOR) {
		/* sRGB -> profile -> sRGB */
		gcm_utils_image_convert (GTK_IMAGE (viewer->preview_widget_input),
					 NULL, icc, NULL, NULL);
		show_section_to = TRUE;
	}

	/* setup cie widget */
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "vbox_cie"));
	if (cd_profile_get_colorspace (profile) == CD_COLORSPACE_RGB &&
	    cd_profile_get_kind (profile) != CD_PROFILE_KIND_NAMED_COLOR) {
		gcm_cie_widget_set_from_profile (viewer->cie_widget,
						 icc);
		gtk_widget_show (widget);
	} else {
		gtk_widget_hide (widget);
	}

	/* get curve data */
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "vbox_trc"));
	clut_trc = cd_icc_get_response (icc, 256, NULL);
	if (clut_trc != NULL) {
		g_object_set (viewer->trc_widget,
			      "data", clut_trc,
			      NULL);
		gtk_widget_show (widget);
	} else {
		gtk_widget_hide (widget);
	}

	/* get vcgt data */
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "vbox_vcgt"));
	clut_vcgt = cd_icc_get_vcgt (icc, 256, NULL);
	if (clut_vcgt != NULL) {
		g_object_set (viewer->vcgt_widget,
			      "data", clut_vcgt,
			      NULL);
		gtk_widget_show (widget);
	} else {
		gtk_widget_hide (widget);
	}

	/* set kind */
	profile_kind = cd_profile_get_kind (profile);
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "label_title_type"));
	gtk_widget_set_visible (widget, profile_kind != CD_PROFILE_KIND_UNKNOWN);
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "label_type"));
	gtk_widget_set_visible (widget, profile_kind != CD_PROFILE_KIND_UNKNOWN);
	if (profile_kind != CD_PROFILE_KIND_UNKNOWN) {
		gtk_widget_show (widget);
		profile_kind_text = gcm_viewer_profile_kind_to_string (profile_kind);
		gtk_label_set_label (GTK_LABEL (widget), profile_kind_text);
	}

	/* set colorspace */
	profile_colorspace = cd_profile_get_colorspace (profile);
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "label_title_colorspace"));
	gtk_widget_set_visible (widget, profile_colorspace != CD_COLORSPACE_UNKNOWN);
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "label_colorspace"));
	gtk_widget_set_visible (widget, profile_colorspace != CD_COLORSPACE_UNKNOWN);
	if (profile_colorspace != CD_COLORSPACE_UNKNOWN) {
		profile_colorspace_text = gcm_viewer_profile_colorspace_to_string (profile_colorspace);
		gtk_label_set_label (GTK_LABEL (widget), profile_colorspace_text);
	}

	/* set vcgt */
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "label_vcgt"));
	has_vcgt = cd_profile_get_has_vcgt (profile);
	if (has_vcgt) {
		/* TRANSLATORS: if the device has a VCGT profile */
		gtk_label_set_label (GTK_LABEL (widget), _("Yes"));
	} else {
		/* TRANSLATORS: if the device has a VCGT profile */
		gtk_label_set_label (GTK_LABEL (widget), _("No"));
	}

	/* set title */
	window = GTK_WINDOW (gtk_builder_get_object (viewer->builder, "dialog_viewer"));
	profile_title = cd_profile_get_title (profile);
	gtk_window_set_title (window, profile_title);

	/* set filename */
	filename = cd_profile_get_filename (profile);
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "label_filename"));
	temp = g_markup_escape_text (filename, -1);
	gtk_label_set_label (GTK_LABEL (widget), temp);
	g_free (temp);

	/* set warnings */
	warnings = cd_profile_get_warnings (profile);
	size = g_strv_length (warnings);
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "label_title_warnings"));
	gtk_widget_set_visible (widget, size != 0);
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "label_warnings"));
	gtk_widget_set_visible (widget, size != 0);
	if (size > 0) {
		str = g_string_new ("");
		/* TRANSLATORS: profiles that have warnings are useable,
		 * but may not be any good */
		g_string_append_printf (str, "%s\n", _("The profile has the following problems:"));
		for (i = 0; i < size; i++) {
			g_debug ("warnings[i]=%s", warnings[i]);
			warning = cd_profile_warning_from_string (warnings[i]);
			g_string_append_printf (str, "• %s\n",
						cd_icc_warning_to_string (warning));
		}
		if (str->len > 0)
			g_string_set_size (str, str->len - 1);
		gtk_label_set_label (GTK_LABEL (widget), str->str);
		g_string_free (str, TRUE);
	}

	/* set profile version */
	temp = g_strdup_printf ("%.1f", cd_icc_get_version (icc));
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "label_version"));
	gtk_label_set_label (GTK_LABEL (widget), temp);
	g_free (temp);

	/* set whitepoint */
	temperature = cd_icc_get_temperature (icc);
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "label_title_temp"));
	gtk_widget_set_visible (widget, temperature > 0);
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "label_temp"));
	gtk_widget_set_visible (widget, temperature > 0);
	if (temperature > 0) {
		if (fabs (temperature - 5000) < 10) {
			temp = g_strdup_printf ("%uK (D50)", temperature);
		} else if (fabs (temperature - 6500) < 10) {
			temp = g_strdup_printf ("%uK (D65)", temperature);
		} else {
			temp = g_strdup_printf ("%uK", temperature);
		}
		gtk_label_set_label (GTK_LABEL (widget), temp);
		g_free (temp);
	}

	/* set size */
	filesize = cd_icc_get_size (icc);
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "label_title_size"));
	gtk_widget_set_visible (widget, filesize > 0);
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "label_size"));
	gtk_widget_set_visible (widget, filesize > 0);
	if (filesize > 0) {
		size_text = g_format_size (filesize);
		gtk_label_set_label (GTK_LABEL (widget), size_text);
	}

	/* set new copyright */
	profile_copyright = cd_icc_get_copyright (icc, viewer->lang, NULL);
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "label_title_copyright"));
	gtk_widget_set_visible (widget, profile_copyright != NULL);
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "label_copyright"));
	gtk_widget_set_visible (widget, profile_copyright != NULL);
	if (profile_copyright != NULL) {
		temp = gcm_utils_linkify (profile_copyright);
		gtk_label_set_label (GTK_LABEL (widget), profile_copyright);
		g_free (temp);
	}

	/* set new manufacturer */
	profile_manufacturer = cd_icc_get_manufacturer (icc, viewer->lang, NULL);
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "label_title_profile_manufacturer"));
	gtk_widget_set_visible (widget, profile_manufacturer != NULL);
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "label_profile_manufacturer"));
	gtk_widget_set_visible (widget, profile_manufacturer != NULL);
	if (profile_manufacturer != NULL) {
		temp = gcm_utils_linkify (profile_manufacturer);
		gtk_label_set_label (GTK_LABEL (widget), temp);
		g_free (temp);
	}

	/* set new model */
	profile_model = cd_icc_get_model (icc, viewer->lang, NULL);
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "label_title_profile_model"));
	gtk_widget_set_visible (widget, profile_model != NULL);
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "label_profile_model"));
	gtk_widget_set_visible (widget, profile_model != NULL);
	if (profile_model != NULL) {
		temp = g_markup_escape_text (profile_model, -1);
		gtk_label_set_label (GTK_LABEL(widget), temp);
		g_free (temp);
	}

	/* set new datetime */
	created = cd_icc_get_created  (icc);
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "label_title_datetime"));
	gtk_widget_set_visible (widget, created != NULL);
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "label_datetime"));
	gtk_widget_set_visible (widget, created != NULL);
	if (created != NULL) {
		/* TRANSLATORS: this is the icc creation date strftime format */
		temp = g_date_time_format (created, _("%B %e %Y, %I∶%M∶%S %p"));
		gtk_label_set_label (GTK_LABEL(widget), temp);
		g_free (temp);
	}

	/* setup named color tab */
	ret = FALSE;
	if (profile_kind == CD_PROFILE_KIND_NAMED_COLOR)
		ret = gcm_viewer_add_named_colors (viewer, icc);
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "vbox_named_colors"));
	gtk_widget_set_visible (widget, ret);

	/* setup metadata tab */
	ret = gcm_viewer_add_metadata (viewer, profile);
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "vbox_metadata"));
	gtk_widget_set_visible (widget, ret);

	/* set delete sensitivity */
	ret = cd_icc_get_can_delete (icc);
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "toolbutton_profile_delete"));
	gtk_widget_set_sensitive (widget, ret);
	if (ret) {
		/* TRANSLATORS: this is the tooltip when the profile can be deleted */
		gtk_widget_set_tooltip_text (widget, _("Delete this profile"));
	} else {
		/* TRANSLATORS: this is the tooltip when the profile cannot be deleted */
		gtk_widget_set_tooltip_text (widget, _("This profile cannot be deleted"));
	}

	/* should we show the image previews at all */
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "vbox_to_srgb"));
	gtk_widget_set_visible (widget, show_section_to);
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "vbox_from_srgb"));
	gtk_widget_set_visible (widget, show_section_from);
}

static void
gcm_viewer_profiles_treeview_clicked_cb (GtkTreeSelection *selection,
					 GcmViewerPrivate *viewer)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	g_autoptr(CdProfile) profile = NULL;

	/* ignore */
	if (viewer->clearing_store)
		return;

	/* This will only work in single or browse selection mode! */
	if (!gtk_tree_selection_get_selected (selection, &model, &iter)) {
		g_debug ("no row selected");
		return;
	}

	/* show profile */
	gtk_tree_model_get (model, &iter,
			    GCM_PROFILES_COLUMN_PROFILE, &profile,
			    -1);
	gcm_viewer_set_profile (viewer, profile);
}

static void
gcm_viewer_client_profile_added_cb (CdClient *client,
				    CdProfile *profile,
				    GcmViewerPrivate *viewer)
{
	g_debug ("%s added, rescanning",
		 cd_profile_get_id (profile));
	gcm_viewer_update_profile_list (viewer);
}

static void
gcm_viewer_client_profile_removed_cb (CdClient *client,
				      CdProfile *profile,
				      GcmViewerPrivate *viewer)
{
	g_debug ("%s removed, rescanning",
		 cd_profile_get_object_path (profile));
	gcm_viewer_update_profile_list (viewer);
}

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

static void
gcm_viewer_add_named_colors_columns (GcmViewerPrivate *viewer,
				     GtkTreeView *treeview)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	/* column for text */
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer,
		      "wrap-mode", PANGO_WRAP_WORD,
		      NULL);
	column = gtk_tree_view_column_new_with_attributes ("", renderer,
							   "text", GCM_NAMED_COLORS_COLUMN_TITLE,
							   NULL);
	gtk_tree_view_column_set_sort_column_id (column, GCM_NAMED_COLORS_COLUMN_SORT);
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (viewer->liststore_nc),
					      GCM_NAMED_COLORS_COLUMN_SORT,
					      GTK_SORT_ASCENDING);
	gtk_tree_view_append_column (treeview, column);
	gtk_tree_view_column_set_expand (column, TRUE);

	/* image */
	renderer = gcm_cell_renderer_color_new ();
	g_object_set (renderer, "stock-size", GTK_ICON_SIZE_MENU, NULL);
	column = gtk_tree_view_column_new_with_attributes ("", renderer,
							   "color", GCM_NAMED_COLORS_COLUMN_COLOR,
							   NULL);
	gtk_tree_view_append_column (treeview, column);
	gtk_tree_view_column_set_expand (column, FALSE);
}

static void
gcm_viewer_add_metadata_columns (GcmViewerPrivate *viewer,
				 GtkTreeView *treeview)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	/* column for text */
	renderer = gtk_cell_renderer_text_new ();
	g_object_set (renderer,
		      "wrap-mode", PANGO_WRAP_WORD,
		      NULL);
	column = gtk_tree_view_column_new_with_attributes ("", renderer,
							   "text", GCM_METADATA_COLUMN_KEY,
							   NULL);
	gtk_tree_view_column_set_sort_column_id (column, GCM_METADATA_COLUMN_KEY);
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (viewer->liststore_metadata),
					      GCM_METADATA_COLUMN_KEY,
					      GTK_SORT_ASCENDING);
	gtk_tree_view_append_column (treeview, column);
	gtk_tree_view_column_set_expand (column, FALSE);

	column = gtk_tree_view_column_new_with_attributes ("", renderer,
							   "text", GCM_METADATA_COLUMN_VALUE,
							   NULL);
	gtk_tree_view_append_column (treeview, column);
	gtk_tree_view_column_set_expand (column, TRUE);
}

static void
gcm_viewer_named_color_treeview_clicked (GtkTreeSelection *selection, GcmViewerPrivate *viewer)
{
	g_debug ("named color changed");
}

static void
gcm_viewer_show_single_profile_by_filename (GcmViewerPrivate *viewer,
					    const gchar *filename)
{
	CdProfile *profile;
	gboolean ret;
	gsize len;
	g_autofree gchar *checksum = NULL;
	g_autofree gchar *data = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(GHashTable) properties = NULL;

	/* compute checksum */
	ret = g_file_get_contents (viewer->filename,
				   &data, &len, &error);
	if (!ret) {
		g_warning ("failed to read file %s: %s",
			   viewer->filename,
			   error->message);
		gcm_viewer_error_dialog (viewer,
					 _("Failed to open ICC profile"),
					 error->message);
		g_application_release (G_APPLICATION (viewer->application));
		return;
	}
	checksum = g_compute_checksum_for_data (G_CHECKSUM_MD5,
						(const guchar *) data,
						len);



	properties = g_hash_table_new_full (g_str_hash, g_str_equal,
					    NULL, NULL);
	g_hash_table_insert (properties,
			     (gpointer) CD_PROFILE_PROPERTY_FILENAME,
			     viewer->filename);
	g_hash_table_insert (properties,
			     (gpointer) CD_PROFILE_METADATA_FILE_CHECKSUM,
			     checksum);

	/* create a temp profile */
	profile = cd_client_create_profile_sync (viewer->client,
						 checksum,
						 CD_OBJECT_SCOPE_TEMP,
						 properties,
						 NULL,
						 &error);
	if (profile == NULL) {
		g_warning ("failed to create profile %s: %s",
			   viewer->filename,
			   error->message);
		gcm_viewer_error_dialog (viewer,
					 _("Failed to import file"),
					 error->message);
		g_application_release (G_APPLICATION (viewer->application));
		return;
	}
	gcm_viewer_set_profile (viewer, profile);
}

static void
gcm_viewer_show_single_profile_by_id (GcmViewerPrivate *viewer,
				      const gchar *id)
{
	g_autoptr(CdProfile) profile = NULL;
	g_autoptr(GError) error = NULL;

	/* select a specific profile only */
	profile = cd_client_find_profile_sync (viewer->client,
					       id,
					       NULL,
					       &error);
	if (profile == NULL) {
		g_warning ("failed to get profile %s: %s",
			   viewer->profile_id,
			   error->message);
		return;
	}
	gcm_viewer_set_profile (viewer, profile);
}

static void
gcm_viewer_activate_cb (GApplication *application, GcmViewerPrivate *viewer)
{
	gboolean ret;
	gint retval;
	GtkStyleContext *context;
	GtkTreeSelection *selection;
	GtkWidget *main_window = NULL;
	GtkWidget *widget;
	g_autoptr(CdProfile) profile = NULL;
	g_autoptr(GError) error = NULL;

	main_window = GTK_WIDGET (gtk_application_get_active_window (GTK_APPLICATION (application)));

	if (main_window != NULL) {
		gtk_window_present (GTK_WINDOW (main_window));
		return;
	}

	/* get UI */
	viewer->builder = gtk_builder_new ();
	retval = gtk_builder_add_from_resource (viewer->builder,
						"/org/gnome/color-manager/gcm-viewer.ui",
						&error);
	if (retval == 0) {
		g_warning ("failed to load ui: %s", error->message);
		return;
	}

	/* add application specific icons to search path */
	gtk_icon_theme_append_search_path (gtk_icon_theme_get_default (),
					   PKGDATADIR G_DIR_SEPARATOR_S "icons");

	/* maintain a list of profiles */
	viewer->client = cd_client_new ();
	ret = cd_client_connect_sync (viewer->client,
				      NULL,
				      &error);
	if (!ret) {
		g_warning ("failed to connect to colord: %s", error->message);
		return;
	}

	/* create list stores */
	viewer->list_store_profiles = gtk_list_store_new (GCM_PROFILES_COLUMN_LAST,
							  G_TYPE_STRING,
							  G_TYPE_STRING,
							  G_TYPE_STRING,
							  CD_TYPE_PROFILE);

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

	/* hide the profiles pane */
	if (viewer->profile_id != NULL ||
	    viewer->filename != NULL) {
		widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder,
				     "vbox_profiles"));
		gtk_widget_hide (widget);
	}

	main_window = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "dialog_viewer"));
	gtk_application_add_window (viewer->application, GTK_WINDOW (main_window));

	/* Hide window first so that the dialogue resizes itself without redrawing */
	gtk_widget_hide (main_window);
	gtk_window_set_icon_name (GTK_WINDOW (main_window), GCM_STOCK_ICON);
	g_signal_connect (main_window, "drag-data-received",
			  G_CALLBACK (gcm_viewer_drag_data_received_cb), viewer);
	gcm_viewer_setup_drag_and_drop (GTK_WIDGET(main_window));

	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "toolbutton_profile_delete"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gcm_viewer_profile_delete_cb), viewer);
	gtk_widget_set_sensitive (widget, FALSE);
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "toolbutton_profile_import"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gcm_viewer_profile_import_cb), viewer);

	/* image next/prev */
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "button_image_next"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gcm_viewer_image_next_cb), viewer);
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "button_image_next1"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gcm_viewer_image_next_cb), viewer);
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "button_image_prev"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gcm_viewer_image_prev_cb), viewer);
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "button_image_prev1"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gcm_viewer_image_prev_cb), viewer);

	/* use named colors */
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder,
						     "treeview_named_colors"));
	viewer->liststore_nc = gtk_list_store_new (GCM_NAMED_COLORS_COLUMN_LAST,
					       G_TYPE_STRING,
					       G_TYPE_STRING,
					       CD_TYPE_COLOR_XYZ);
	gtk_tree_view_set_model (GTK_TREE_VIEW (widget),
				 GTK_TREE_MODEL (viewer->liststore_nc));
	gcm_viewer_add_named_colors_columns (viewer, GTK_TREE_VIEW (widget));
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widget));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);
	g_signal_connect (selection, "changed",
			  G_CALLBACK (gcm_viewer_named_color_treeview_clicked), viewer);

	/* use metadata */
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder,
						     "treeview_metadata"));
	viewer->liststore_metadata = gtk_list_store_new (GCM_METADATA_COLUMN_LAST,
							 G_TYPE_STRING,
							 G_TYPE_STRING);
	gtk_tree_view_set_model (GTK_TREE_VIEW (widget),
				 GTK_TREE_MODEL (viewer->liststore_metadata));
	gcm_viewer_add_metadata_columns (viewer, GTK_TREE_VIEW (widget));

	/* use cie widget */
	viewer->cie_widget = gcm_cie_widget_new ();
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "vbox_cie_widget"));
	gtk_box_pack_start (GTK_BOX(widget), viewer->cie_widget, TRUE, TRUE, 0);
	gtk_box_reorder_child (GTK_BOX(widget), viewer->cie_widget, 0);

	/* use trc widget */
	viewer->trc_widget = gcm_trc_widget_new ();
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "vbox_trc_widget"));
	gtk_box_pack_start (GTK_BOX(widget), viewer->trc_widget, TRUE, TRUE, 0);
	gtk_box_reorder_child (GTK_BOX(widget), viewer->trc_widget, 0);

	/* use vcgt widget */
	viewer->vcgt_widget = gcm_trc_widget_new ();
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "vbox_vcgt_widget"));
	gtk_box_pack_start (GTK_BOX(widget), viewer->vcgt_widget, TRUE, TRUE, 0);
	gtk_box_reorder_child (GTK_BOX(widget), viewer->vcgt_widget, 0);

	/* use preview input */
	viewer->preview_widget_input = GTK_WIDGET (gtk_image_new ());
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "vbox_preview_input"));
	gtk_box_pack_end (GTK_BOX(widget), viewer->preview_widget_input, FALSE, FALSE, 0);
	gcm_viewer_set_example_image (viewer, GTK_IMAGE (viewer->preview_widget_input));
	gtk_widget_set_visible (viewer->preview_widget_input, TRUE);

	/* use preview output */
	viewer->preview_widget_output = GTK_WIDGET (gtk_image_new ());
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder, "vbox_preview_output"));
	gtk_box_pack_end (GTK_BOX(widget), viewer->preview_widget_output, FALSE, FALSE, 0);
	gcm_viewer_set_example_image (viewer, GTK_IMAGE (viewer->preview_widget_output));
	gtk_widget_set_visible (viewer->preview_widget_output, TRUE);

	/* make profiles toolbar sexy */
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder,
						     "scrolledwindow_profiles"));
	context = gtk_widget_get_style_context (widget);
	gtk_style_context_set_junction_sides (context, GTK_JUNCTION_BOTTOM);

	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder,
						     "toolbar_profiles"));
	context = gtk_widget_get_style_context (widget);
	gtk_style_context_add_class (context, GTK_STYLE_CLASS_INLINE_TOOLBAR);
	gtk_style_context_set_junction_sides (context, GTK_JUNCTION_TOP);

	/* make profile labels sexy */
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder,
						     "label_title_type"));
	context = gtk_widget_get_style_context (widget);
	gtk_style_context_add_class (context, "dim-label");
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder,
						     "label_title_colorspace"));
	context = gtk_widget_get_style_context (widget);
	gtk_style_context_add_class (context, "dim-label");
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder,
						     "label_title_datetime"));
	context = gtk_widget_get_style_context (widget);
	gtk_style_context_add_class (context, "dim-label");
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder,
						     "label_title_profile_manufacturer"));
	context = gtk_widget_get_style_context (widget);
	gtk_style_context_add_class (context, "dim-label");
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder,
						     "label_title_profile_model"));
	context = gtk_widget_get_style_context (widget);
	gtk_style_context_add_class (context, "dim-label");
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder,
						     "label_title_vcgt"));
	context = gtk_widget_get_style_context (widget);
	gtk_style_context_add_class (context, "dim-label");
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder,
						     "label_title_temp"));
	context = gtk_widget_get_style_context (widget);
	gtk_style_context_add_class (context, "dim-label");
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder,
						     "label_title_copyright"));
	context = gtk_widget_get_style_context (widget);
	gtk_style_context_add_class (context, "dim-label");
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder,
						     "label_title_size"));
	context = gtk_widget_get_style_context (widget);
	gtk_style_context_add_class (context, "dim-label");
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder,
						     "label_title_filename"));
	context = gtk_widget_get_style_context (widget);
	gtk_style_context_add_class (context, "dim-label");
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder,
						     "label_title_warnings"));
	context = gtk_widget_get_style_context (widget);
	gtk_style_context_add_class (context, "dim-label");
	widget = GTK_WIDGET (gtk_builder_get_object (viewer->builder,
						     "label_title_version"));
	context = gtk_widget_get_style_context (widget);
	gtk_style_context_add_class (context, "dim-label");

	/* set the parent window if it is specified */
	if (viewer->xid != 0) {
		g_debug ("Setting xid %u", viewer->xid);
		gcm_window_set_parent_xid (GTK_WINDOW (main_window), viewer->xid);
	}

	/* show main UI */
	gtk_widget_show (main_window);

	/* specified an ID */
	if (viewer->profile_id != NULL) {
		gcm_viewer_show_single_profile_by_id (viewer, viewer->profile_id);
		return;
	}

	/* specified a filename */
	if (viewer->filename != NULL) {
		gcm_viewer_show_single_profile_by_filename (viewer, viewer->filename);
		return;
	}

	/* do all this after the window has been set up */
	g_signal_connect (viewer->client, "profile-added",
			  G_CALLBACK (gcm_viewer_client_profile_added_cb),
			  viewer);
	g_signal_connect (viewer->client, "profile-removed",
			  G_CALLBACK (gcm_viewer_client_profile_removed_cb),
			  viewer);

	/* update list of profiles */
	gcm_viewer_update_profile_list (viewer);
}

int
main (int argc, char **argv)
{
	GcmViewerPrivate *viewer;
	int status = 0;

	viewer = g_new0 (GcmViewerPrivate, 1);
	viewer->lang = g_getenv ("LANG");

	const GOptionEntry options[] = {
		{ "parent-window", 'p', 0, G_OPTION_ARG_INT, &viewer->xid,
		  /* TRANSLATORS: we can make this modal (stay on top of) another window */
		  _("Set the parent window to make this modal"), NULL },
		{ "profile", 'f', 0, G_OPTION_ARG_STRING, &viewer->profile_id,
		  /* TRANSLATORS: show just the one profile, rather than all */
		  _("Set the specific profile to show"), NULL },
		{ "file", '\0', 0, G_OPTION_ARG_STRING, &viewer->filename,
		  /* TRANSLATORS: show just the one filename, rather than all */
		  _("Set the specific file to show"), NULL },
		{ NULL}
	};

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	g_set_prgname (GCM_VIEWER_APPLICATION_ID);

	/* ensure single instance */
	viewer->application = gtk_application_new (GCM_VIEWER_APPLICATION_ID, 0);
	g_signal_connect (viewer->application, "activate",
			  G_CALLBACK (gcm_viewer_activate_cb), viewer);

	g_application_add_main_option_entries (G_APPLICATION (viewer->application), options);
	g_application_set_option_context_summary (G_APPLICATION (viewer->application),
	                                          /* TRANSLATORS: summary shown in the command-line help */
	                                          _("GNOME Color Manager - Color Profile Viewer"));
	g_application_add_option_group (G_APPLICATION (viewer->application),
	                                gcm_debug_get_option_group ());

	/* wait */
	status = g_application_run (G_APPLICATION (viewer->application), argc, argv);

	g_object_unref (viewer->application);
	if (viewer->builder != NULL)
		g_object_unref (viewer->builder);
	if (viewer->client != NULL)
		g_object_unref (viewer->client);
	g_free (viewer->profile_id);
	g_free (viewer->filename);
	g_free (viewer);
	return status;
}

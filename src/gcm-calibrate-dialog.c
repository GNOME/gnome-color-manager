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
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/**
 * SECTION:gcm-calibrate-dialog
 * @short_description: Calibration object
 *
 * This object allows calibration functionality using CMS.
 */

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "gcm-enum.h"
#include "gcm-calibrate.h"
#include "gcm-calibrate-dialog.h"
#include "gcm-utils.h"

#include "egg-debug.h"

static void     gcm_calibrate_dialog_finalize	(GObject     *object);

#define GCM_CALIBRATE_DIALOG_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GCM_TYPE_CALIBRATE_DIALOG, GcmCalibrateDialogPrivate))

/**
 * GcmCalibrateDialogPrivate:
 *
 * Private #GcmCalibrateDialog data
 **/
struct _GcmCalibrateDialogPrivate
{
	GPtrArray			*cached_dialogs;
	GtkBuilder			*builder;
	GcmCalibrateDeviceKind		 device_kind;
	GcmCalibratePrintKind		 print_kind;
	GcmCalibrateReferenceKind	 reference_kind;
	GtkResponseType			 response;
	GMainLoop			*loop;
	gboolean			 move_window;
};

enum {
	SIGNAL_RESPONSE,
	SIGNAL_LAST
};

enum {
	PROP_0,
	PROP_DEVICE_KIND,
	PROP_PRINT_KIND,
	PROP_REFERENCE_KIND,
	PROP_LAST
};

typedef struct {
	gchar		*title;
	gchar		*message;
	gchar		*image_filename;
	gboolean	 show_okay;
	gboolean	 show_expander;
} GcmCalibrateDialogItem;

static gpointer gcm_calibrate_dialog_object = NULL;
static guint signals[SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE (GcmCalibrateDialog, gcm_calibrate_dialog, G_TYPE_OBJECT)

/**
 * gcm_calibrate_dialog_emit_response:
 **/
static void
gcm_calibrate_dialog_emit_response (GcmCalibrateDialog *calibrate_dialog, GtkResponseType response)
{
	calibrate_dialog->priv->response = response;
	g_signal_emit (calibrate_dialog, signals[SIGNAL_RESPONSE], 0, response);

	/* don't block anymore */
	if (g_main_loop_is_running (calibrate_dialog->priv->loop))
		g_main_loop_quit (calibrate_dialog->priv->loop);
}

/**
 * gcm_calibrate_dialog_button_clicked_lcd_cb:
 **/
static void
gcm_calibrate_dialog_button_clicked_lcd_cb (GtkWidget *widget, GcmCalibrateDialog *calibrate_dialog)
{
	calibrate_dialog->priv->device_kind = GCM_CALIBRATE_DEVICE_KIND_LCD;
	gcm_calibrate_dialog_emit_response (calibrate_dialog, GTK_RESPONSE_OK);
}

/**
 * gcm_calibrate_dialog_button_clicked_crt_cb:
 **/
static void
gcm_calibrate_dialog_button_clicked_crt_cb (GtkWidget *widget, GcmCalibrateDialog *calibrate_dialog)
{
	calibrate_dialog->priv->device_kind = GCM_CALIBRATE_DEVICE_KIND_CRT;
	gcm_calibrate_dialog_emit_response (calibrate_dialog, GTK_RESPONSE_OK);
}

/**
 * gcm_calibrate_dialog_button_clicked_projector_cb:
 **/
static void
gcm_calibrate_dialog_button_clicked_projector_cb (GtkWidget *widget, GcmCalibrateDialog *calibrate_dialog)
{
	calibrate_dialog->priv->device_kind = GCM_CALIBRATE_DEVICE_KIND_PROJECTOR;
	gcm_calibrate_dialog_emit_response (calibrate_dialog, GTK_RESPONSE_OK);
}

/**
 * gcm_calibrate_dialog_button_clicked_print_local_cb:
 **/
static void
gcm_calibrate_dialog_button_clicked_print_local_cb (GtkWidget *widget, GcmCalibrateDialog *calibrate_dialog)
{
	calibrate_dialog->priv->print_kind = GCM_CALIBRATE_PRINT_KIND_LOCAL;
	gcm_calibrate_dialog_emit_response (calibrate_dialog, GTK_RESPONSE_OK);
}

/**
 * gcm_calibrate_dialog_button_clicked_print_generate_cb:
 **/
static void
gcm_calibrate_dialog_button_clicked_print_generate_cb (GtkWidget *widget, GcmCalibrateDialog *calibrate_dialog)
{
	calibrate_dialog->priv->print_kind = GCM_CALIBRATE_PRINT_KIND_GENERATE;
	gcm_calibrate_dialog_emit_response (calibrate_dialog, GTK_RESPONSE_OK);
}

/**
 * gcm_calibrate_dialog_button_clicked_print_analyze_cb:
 **/
static void
gcm_calibrate_dialog_button_clicked_print_analyze_cb (GtkWidget *widget, GcmCalibrateDialog *calibrate_dialog)
{
	calibrate_dialog->priv->print_kind = GCM_CALIBRATE_PRINT_KIND_ANALYZE;
	gcm_calibrate_dialog_emit_response (calibrate_dialog, GTK_RESPONSE_OK);
}

/**
 * gcm_calibrate_dialog_button_clicked_ok_cb:
 **/
static void
gcm_calibrate_dialog_button_clicked_ok_cb (GtkWidget *widget, GcmCalibrateDialog *calibrate_dialog)
{
	gcm_calibrate_dialog_emit_response (calibrate_dialog, GTK_RESPONSE_OK);
}

/**
 * gcm_calibrate_dialog_button_clicked_cancel_cb:
 **/
static void
gcm_calibrate_dialog_button_clicked_cancel_cb (GtkWidget *widget, GcmCalibrateDialog *calibrate_dialog)
{
	gcm_calibrate_dialog_emit_response (calibrate_dialog, GTK_RESPONSE_CANCEL);
}

/**
 * gcm_calibrate_dialog_set_image_filename_private:
 **/
static void
gcm_calibrate_dialog_set_image_filename_private (GcmCalibrateDialog *calibrate_dialog, const gchar *image_filename)
{
	GtkWidget *widget;
	gchar *filename = NULL;
	GdkPixbuf *pixbuf;
	GError *error = NULL;
	GcmCalibrateDialogPrivate *priv = calibrate_dialog->priv;

	/* set the image */
	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "image_generic"));
	if (image_filename != NULL) {
		filename = g_build_filename (GCM_DATA, "icons", image_filename, NULL);
		pixbuf = gdk_pixbuf_new_from_file_at_size (filename, 200, 400, &error);
		if (pixbuf == NULL) {
			egg_warning ("failed to load image: %s", error->message);
			g_error_free (error);
			gtk_widget_hide (widget);
		} else {
			gtk_image_set_from_pixbuf (GTK_IMAGE (widget), pixbuf);
			gtk_widget_show (widget);
		}
		g_free (filename);
	} else {
		gtk_widget_hide (widget);
	}
}

/**
 * gcm_calibrate_dialog_set_image_filename:
 **/
void
gcm_calibrate_dialog_set_image_filename (GcmCalibrateDialog *calibrate_dialog, const gchar *image_filename)
{
	GcmCalibrateDialogPrivate *priv = calibrate_dialog->priv;
	GcmCalibrateDialogItem *dialog;

	/* get the last entry */
	dialog = g_ptr_array_index (priv->cached_dialogs, priv->cached_dialogs->len - 1);
	g_free (dialog->image_filename);
	dialog->image_filename = g_strdup (image_filename);

	/* actually load the image */
	gcm_calibrate_dialog_set_image_filename_private (calibrate_dialog, image_filename);
}

/**
 * gcm_calibrate_dialog_dialog_free:
 **/
static void
gcm_calibrate_dialog_dialog_free (GcmCalibrateDialogItem *dialog)
{
	g_free (dialog->title);
	g_free (dialog->message);
	g_free (dialog->image_filename);
	g_free (dialog);
}

/**
 * gcm_calibrate_dialog_set_window:
 **/
void
gcm_calibrate_dialog_set_window	(GcmCalibrateDialog *calibrate_dialog, GtkWindow *window)
{
	GtkWidget *widget;
	GcmCalibrateDialogPrivate *priv = calibrate_dialog->priv;

	/* do nothing, we can't unparent */
	if (window == NULL)
		return;

	/* ensure it's not the same thing */
	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "dialog_calibrate"));
	if (GTK_WINDOW (widget) == window) {
		egg_warning ("trying to set parent to self!");
		return;
	}

	/* make modal */
	gtk_window_set_transient_for (GTK_WINDOW (widget), window);
}

/**
 * gcm_calibrate_dialog_get_window:
 **/
GtkWindow *
gcm_calibrate_dialog_get_window (GcmCalibrateDialog *calibrate_dialog)
{
	GtkWindow *window;
	window = GTK_WINDOW (gtk_builder_get_object (calibrate_dialog->priv->builder, "dialog_calibrate"));
	return window;
}

/**
 * gcm_calibrate_dialog_run:
 **/
GtkResponseType
gcm_calibrate_dialog_run (GcmCalibrateDialog *calibrate_dialog)
{
	if (g_main_loop_is_running (calibrate_dialog->priv->loop))
		egg_error ("you can't call this recursively");

	g_main_loop_run (calibrate_dialog->priv->loop);
	return calibrate_dialog->priv->response;
}

/**
 * gcm_calibrate_set_button_ok_id:
 **/
void
gcm_calibrate_dialog_set_button_ok_id (GcmCalibrateDialog *calibrate_dialog, const gchar *button_id)
{
	GtkWidget *widget;
	GcmCalibrateDialogPrivate *priv = calibrate_dialog->priv;

	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "button_ok"));
	gtk_button_set_label (GTK_BUTTON (widget), button_id);
	gtk_button_set_use_stock (GTK_BUTTON (widget), g_str_has_prefix (button_id, "gtk-"));
}

/**
 * gcm_calibrate_dialog_show:
 **/
void
gcm_calibrate_dialog_show (GcmCalibrateDialog		*calibrate_dialog,
			   GcmCalibrateDialogTab	 tab,
			   const gchar			*title,
			   const gchar			*message)
{
	GtkWidget *widget;
	GcmCalibrateDialogPrivate *priv = calibrate_dialog->priv;
	gint x, y;
	GcmCalibrateDialogItem *dialog;

	/* save in case we need to reuse */
	dialog = g_new0 (GcmCalibrateDialogItem, 1);
	dialog->title = g_strdup (title);
	dialog->message = g_strdup (message);
	g_ptr_array_add (priv->cached_dialogs, dialog);

	/* hide elements that should not be seen */
	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "vbox_display_type"));
	gtk_widget_set_visible (widget, (tab == GCM_CALIBRATE_DIALOG_TAB_DISPLAY_TYPE));
	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "vbox_target_type"));
	gtk_widget_set_visible (widget, (tab == GCM_CALIBRATE_DIALOG_TAB_TARGET_TYPE));
	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "vbox_manual"));
	gtk_widget_set_visible (widget, (tab == GCM_CALIBRATE_DIALOG_TAB_MANUAL));
	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "vbox_generic"));
	gtk_widget_set_visible (widget, (tab == GCM_CALIBRATE_DIALOG_TAB_GENERIC));
	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "vbox_print_mode"));
	gtk_widget_set_visible (widget, (tab == GCM_CALIBRATE_DIALOG_TAB_PRINT_MODE));

	/* reset */
	gcm_calibrate_dialog_set_image_filename (calibrate_dialog, NULL);
	gcm_calibrate_dialog_set_show_expander (calibrate_dialog, FALSE);
	gcm_calibrate_dialog_set_show_button_ok (calibrate_dialog, FALSE);
	gcm_calibrate_dialog_set_image_filename_private (calibrate_dialog, NULL);
	gcm_calibrate_dialog_set_button_ok_id (calibrate_dialog, GTK_STOCK_OK);

	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "label_title"));
	gtk_label_set_label (GTK_LABEL (widget), title);

	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "label_message"));
	gtk_label_set_label (GTK_LABEL (widget), message);

	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "notebook1"));
	gtk_notebook_set_current_page (GTK_NOTEBOOK (widget), tab);

	/* move the dialog out of the way, so the grey square doesn't cover it */
	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "dialog_calibrate"));
	if (calibrate_dialog->priv->move_window) {
		gtk_window_get_position (GTK_WINDOW (widget), &x, &y);
		egg_debug ("currently at %i,%i, moving left", x, y);
		gtk_window_move (GTK_WINDOW (widget), 10, y);
	}

	gtk_widget_show (widget);
}

/**
 * gcm_calibrate_dialog_hide:
 **/
void
gcm_calibrate_dialog_hide (GcmCalibrateDialog *calibrate_dialog)
{
	GtkWidget *widget;
	widget = GTK_WIDGET (gtk_builder_get_object (calibrate_dialog->priv->builder, "dialog_calibrate"));
	gtk_widget_hide (widget);
}

/**
 * gcm_calibrate_dialog_pop:
 **/
void
gcm_calibrate_dialog_pop (GcmCalibrateDialog *calibrate_dialog)
{
	GcmCalibrateDialogPrivate *priv = calibrate_dialog->priv;
	GtkWidget *widget;
	gchar *text;
	guint len;
	GcmCalibrateDialogItem *dialog;

	/* save in case we need to reuse */
	len = priv->cached_dialogs->len;
	if (len < 2) {
		egg_warning ("cannot pop dialog as nothing to recover");
		return;
	}
	dialog = g_ptr_array_index (priv->cached_dialogs, len-2);

	/* set the text */
	text = g_strdup_printf ("<big><b>%s</b></big>", dialog->title);
	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "label_title"));
	gtk_label_set_markup (GTK_LABEL(widget), text);
	g_free (text);

	/* set the text */
	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "label_message"));
	gtk_label_set_markup (GTK_LABEL(widget), dialog->message);

	/* show the okay button */
	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "button_ok"));
	gtk_widget_set_visible (widget, dialog->show_okay);

	/* show the expander */
	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "expander_details"));
	gtk_widget_set_visible (widget, dialog->show_expander);

	/* show the correct image */
	gcm_calibrate_dialog_set_image_filename_private (calibrate_dialog, dialog->image_filename);

	/* remove from the stack */
	g_ptr_array_remove_index (priv->cached_dialogs, len-1);
}

/**
 * gcm_calibrate_dialog_pack_details:
 **/
void
gcm_calibrate_dialog_pack_details (GcmCalibrateDialog *calibrate_dialog, GtkWidget *widget)
{
	GtkWidget *vbox;
	vbox = GTK_WIDGET (gtk_builder_get_object (calibrate_dialog->priv->builder, "vbox_details"));
	gtk_box_pack_end (GTK_BOX(vbox), widget, TRUE, TRUE, 6);
	gtk_widget_show (widget);
}

/**
 * gcm_calibrate_dialog_set_show_button_ok:
 **/
void
gcm_calibrate_dialog_set_show_button_ok (GcmCalibrateDialog *calibrate_dialog, gboolean visible)
{
	GtkWidget *widget;
	GcmCalibrateDialogItem *dialog;
	GcmCalibrateDialogPrivate *priv = calibrate_dialog->priv;

	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "button_ok"));
	gtk_widget_set_visible (widget, visible);

	/* get the last entry */
	dialog = g_ptr_array_index (priv->cached_dialogs, priv->cached_dialogs->len - 1);
	dialog->show_okay = visible;
}

/**
 * gcm_calibrate_dialog_set_show_expander:
 **/
void
gcm_calibrate_dialog_set_show_expander (GcmCalibrateDialog *calibrate_dialog, gboolean visible)
{
	GcmCalibrateDialogItem *dialog;
	GtkWidget *widget;
	GcmCalibrateDialogPrivate *priv = calibrate_dialog->priv;

	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "expander_details"));
	gtk_widget_set_visible (widget, visible);

	/* get the last entry */
	dialog = g_ptr_array_index (priv->cached_dialogs, priv->cached_dialogs->len - 1);
	dialog->show_expander = visible;
}

/**
 * gcm_calibrate_dialog_set_move_window:
 **/
void
gcm_calibrate_dialog_set_move_window (GcmCalibrateDialog *calibrate_dialog, gboolean move_window)
{
	calibrate_dialog->priv->move_window = move_window;
}

/**
 * gcm_calibrate_dialog_delete_event_cb:
 **/
static gboolean
gcm_calibrate_dialog_delete_event_cb (GtkWidget *widget, GdkEvent *event, GcmCalibrateDialog *calibrate_dialog)
{
	gcm_calibrate_dialog_emit_response (calibrate_dialog, GTK_RESPONSE_CANCEL);
	return FALSE;
}

/**
 * gcm_calibrate_dialog_reference_kind_to_thumbnail_image_filename:
 **/
static const gchar *
gcm_calibrate_dialog_reference_kind_to_thumbnail_image_filename (GcmCalibrateReferenceKind kind)
{
	if (kind == GCM_CALIBRATE_REFERENCE_KIND_CMP_DIGITAL_TARGET_3)
		return "CMP-DigitalTarget3.png";
	if (kind == GCM_CALIBRATE_REFERENCE_KIND_CMP_DT_003)
		return NULL;
	if (kind == GCM_CALIBRATE_REFERENCE_KIND_COLOR_CHECKER)
		return "ColorChecker24.png";
	if (kind == GCM_CALIBRATE_REFERENCE_KIND_COLOR_CHECKER_DC)
		return "ColorCheckerDC.png";
	if (kind == GCM_CALIBRATE_REFERENCE_KIND_COLOR_CHECKER_SG)
		return "ColorCheckerSG.png";
	if (kind == GCM_CALIBRATE_REFERENCE_KIND_HUTCHCOLOR)
		return NULL;
	if (kind == GCM_CALIBRATE_REFERENCE_KIND_I1_RGB_SCAN_1_4)
		return "i1_RGB_Scan_14.png";
	if (kind == GCM_CALIBRATE_REFERENCE_KIND_IT8)
		return "IT872.png";
	if (kind == GCM_CALIBRATE_REFERENCE_KIND_LASER_SOFT_DC_PRO)
		return "LaserSoftDCPro.png";
	if (kind == GCM_CALIBRATE_REFERENCE_KIND_QPCARD_201)
		return "QPcard_201.png";
	return NULL;
}

/**
 * gcm_calibrate_dialog_reference_kind_to_localised_string:
 **/
static const gchar *
gcm_calibrate_dialog_reference_kind_to_localised_string (GcmCalibrateReferenceKind kind)
{
	if (kind == GCM_CALIBRATE_REFERENCE_KIND_CMP_DIGITAL_TARGET_3) {
		/* TRANSLATORS: this is probably a brand name */
		return _("CMP Digital Target 3");
	}
	if (kind == GCM_CALIBRATE_REFERENCE_KIND_CMP_DT_003) {
		/* TRANSLATORS: this is probably a brand name */
		return _("CMP DT 003");
	}
	if (kind == GCM_CALIBRATE_REFERENCE_KIND_COLOR_CHECKER) {
		/* TRANSLATORS: this is probably a brand name */
		return _("Color Checker");
	}
	if (kind == GCM_CALIBRATE_REFERENCE_KIND_COLOR_CHECKER_DC) {
		/* TRANSLATORS: this is probably a brand name */
		return _("Color Checker DC");
	}
	if (kind == GCM_CALIBRATE_REFERENCE_KIND_COLOR_CHECKER_SG) {
		/* TRANSLATORS: this is probably a brand name */
		return _("Color Checker SG");
	}
	if (kind == GCM_CALIBRATE_REFERENCE_KIND_HUTCHCOLOR) {
		/* TRANSLATORS: this is probably a brand name */
		return _("Hutchcolor");
	}
	if (kind == GCM_CALIBRATE_REFERENCE_KIND_I1_RGB_SCAN_1_4) {
		/* TRANSLATORS: this is probably a brand name */
		return _("i1 RGB Scan 1.4");
	}
	if (kind == GCM_CALIBRATE_REFERENCE_KIND_IT8) {
		/* TRANSLATORS: this is probably a brand name */
		return _("IT8.7/2");
	}
	if (kind == GCM_CALIBRATE_REFERENCE_KIND_LASER_SOFT_DC_PRO) {
		/* TRANSLATORS: this is probably a brand name */
		return _("Laser Soft DC Pro");
	}
	if (kind == GCM_CALIBRATE_REFERENCE_KIND_QPCARD_201) {
		/* TRANSLATORS: this is probably a brand name */
		return _("QPcard 201");
	}
	return NULL;
}

/**
 * gcm_calibrate_dialog_reference_kind_combobox_cb:
 **/
static void
gcm_calibrate_dialog_reference_kind_combobox_cb (GtkComboBox *combo_box, GcmCalibrateDialog *calibrate_dialog)
{
	const gchar *filename;
	gchar *path;
	GtkWidget *widget;
	GcmCalibrateDialogPrivate *priv = calibrate_dialog->priv;

	/* not sorted so we can just use the index */
	priv->reference_kind = gtk_combo_box_get_active (GTK_COMBO_BOX (combo_box));
	filename = gcm_calibrate_dialog_reference_kind_to_thumbnail_image_filename (priv->reference_kind);

	/* fallback */
	if (filename == NULL)
		filename = "unknown.png";

	path = g_build_filename (GCM_DATA, "targets", filename, NULL);
	widget = GTK_WIDGET (gtk_builder_get_object (calibrate_dialog->priv->builder, "image_target"));
	gtk_image_set_from_file (GTK_IMAGE (widget), path);
	g_free (path);
}

/**
 * gcm_calibrate_dialog_setup_combo_simple_text:
 **/
static void
gcm_calibrate_dialog_setup_combo_simple_text (GtkWidget *combo_box)
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
 * gcm_calibrate_dialog_get_property:
 **/
static void
gcm_calibrate_dialog_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GcmCalibrateDialog *calibrate_dialog = GCM_CALIBRATE_DIALOG (object);
	GcmCalibrateDialogPrivate *priv = calibrate_dialog->priv;

	switch (prop_id) {
	case PROP_REFERENCE_KIND:
		g_value_set_uint (value, priv->reference_kind);
		break;
	case PROP_DEVICE_KIND:
		g_value_set_uint (value, priv->device_kind);
		break;
	case PROP_PRINT_KIND:
		g_value_set_uint (value, priv->print_kind);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gcm_calibrate_dialog_set_property:
 **/
static void
gcm_calibrate_dialog_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gcm_calibrate_dialog_class_init:
 **/
static void
gcm_calibrate_dialog_class_init (GcmCalibrateDialogClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gcm_calibrate_dialog_finalize;
	object_class->get_property = gcm_calibrate_dialog_get_property;
	object_class->set_property = gcm_calibrate_dialog_set_property;

	/**
	 * GcmCalibrateDialog:reference-kind:
	 */
	pspec = g_param_spec_uint ("reference-kind", NULL, NULL,
				   0, G_MAXUINT, 0,
				   G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_REFERENCE_KIND, pspec);

	/**
	 * GcmCalibrateDialog:device-kind:
	 */
	pspec = g_param_spec_uint ("device-kind", NULL, NULL,
				   0, G_MAXUINT, 0,
				   G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_DEVICE_KIND, pspec);

	/**
	 * GcmCalibrateDialog:print-kind:
	 */
	pspec = g_param_spec_uint ("print-kind", NULL, NULL,
				   0, G_MAXUINT, 0,
				   G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_PRINT_KIND, pspec);

	/**
	 * GcmCalibrateDialog::response:
	 **/
	signals[SIGNAL_RESPONSE] =
		g_signal_new ("response",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GcmCalibrateDialogClass, response),
			      NULL, NULL, g_cclosure_marshal_VOID__INT,
			      G_TYPE_NONE, 1, G_TYPE_INT);

	g_type_class_add_private (klass, sizeof (GcmCalibrateDialogPrivate));
}

/**
 * gcm_calibrate_dialog_init:
 **/
static void
gcm_calibrate_dialog_init (GcmCalibrateDialog *calibrate_dialog)
{
	gint retval;
	GError *error = NULL;
	GtkWidget *widget;
	guint i;

	calibrate_dialog->priv = GCM_CALIBRATE_DIALOG_GET_PRIVATE (calibrate_dialog);

	calibrate_dialog->priv->device_kind = GCM_CALIBRATE_DEVICE_KIND_UNKNOWN;
	calibrate_dialog->priv->print_kind = GCM_CALIBRATE_PRINT_KIND_UNKNOWN;
	calibrate_dialog->priv->reference_kind = GCM_CALIBRATE_REFERENCE_KIND_UNKNOWN;
	calibrate_dialog->priv->move_window = FALSE;
	calibrate_dialog->priv->loop = g_main_loop_new (NULL, FALSE);
	calibrate_dialog->priv->cached_dialogs = g_ptr_array_new_with_free_func ((GDestroyNotify)gcm_calibrate_dialog_dialog_free);

	/* get UI */
	calibrate_dialog->priv->builder = gtk_builder_new ();
	retval = gtk_builder_add_from_file (calibrate_dialog->priv->builder, GCM_DATA "/gcm-calibrate.ui", &error);
	if (retval == 0) {
		egg_error ("failed to load ui: %s", error->message);
		g_error_free (error);
	}

	widget = GTK_WIDGET (gtk_builder_get_object (calibrate_dialog->priv->builder, "dialog_calibrate"));
	g_signal_connect (widget, "delete_event",
			  G_CALLBACK (gcm_calibrate_dialog_delete_event_cb), calibrate_dialog);
	widget = GTK_WIDGET (gtk_builder_get_object (calibrate_dialog->priv->builder, "button_lcd"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gcm_calibrate_dialog_button_clicked_lcd_cb), calibrate_dialog);
	widget = GTK_WIDGET (gtk_builder_get_object (calibrate_dialog->priv->builder, "button_crt"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gcm_calibrate_dialog_button_clicked_crt_cb), calibrate_dialog);
	widget = GTK_WIDGET (gtk_builder_get_object (calibrate_dialog->priv->builder, "button_projector"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gcm_calibrate_dialog_button_clicked_projector_cb), calibrate_dialog);
	widget = GTK_WIDGET (gtk_builder_get_object (calibrate_dialog->priv->builder, "button_cancel"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gcm_calibrate_dialog_button_clicked_cancel_cb), calibrate_dialog);
	widget = GTK_WIDGET (gtk_builder_get_object (calibrate_dialog->priv->builder, "button_ok"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gcm_calibrate_dialog_button_clicked_ok_cb), calibrate_dialog);
	widget = GTK_WIDGET (gtk_builder_get_object (calibrate_dialog->priv->builder, "button_print_local"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gcm_calibrate_dialog_button_clicked_print_local_cb), calibrate_dialog);
	widget = GTK_WIDGET (gtk_builder_get_object (calibrate_dialog->priv->builder, "button_print_generate"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gcm_calibrate_dialog_button_clicked_print_generate_cb), calibrate_dialog);
	widget = GTK_WIDGET (gtk_builder_get_object (calibrate_dialog->priv->builder, "button_print_analyze"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gcm_calibrate_dialog_button_clicked_print_analyze_cb), calibrate_dialog);

	widget = GTK_WIDGET (gtk_builder_get_object (calibrate_dialog->priv->builder, "image_target"));
	gtk_widget_set_size_request (widget, 200, 140);

	widget = GTK_WIDGET (gtk_builder_get_object (calibrate_dialog->priv->builder, "combobox_target"));
	gcm_calibrate_dialog_setup_combo_simple_text (widget);
	g_signal_connect (widget, "changed", G_CALLBACK (gcm_calibrate_dialog_reference_kind_combobox_cb), calibrate_dialog);

	/* add the list of charts */
	for (i = 0; i < GCM_CALIBRATE_REFERENCE_KIND_UNKNOWN; i++) {
		gtk_combo_box_append_text (GTK_COMBO_BOX (widget),
					   gcm_calibrate_dialog_reference_kind_to_localised_string (i));
	}

	/* use IT8 by default */
	gtk_combo_box_set_active (GTK_COMBO_BOX (widget), GCM_CALIBRATE_REFERENCE_KIND_IT8);

	widget = GTK_WIDGET (gtk_builder_get_object (calibrate_dialog->priv->builder, "notebook1"));
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (widget), FALSE);
}

/**
 * gcm_calibrate_dialog_finalize:
 **/
static void
gcm_calibrate_dialog_finalize (GObject *object)
{
	GcmCalibrateDialog *calibrate_dialog = GCM_CALIBRATE_DIALOG (object);
	GcmCalibrateDialogPrivate *priv = calibrate_dialog->priv;

	g_object_unref (priv->builder);
	g_ptr_array_unref (priv->cached_dialogs);
	g_main_loop_unref (priv->loop);

	G_OBJECT_CLASS (gcm_calibrate_dialog_parent_class)->finalize (object);
}

/**
 * gcm_calibrate_dialog_new:
 *
 * Return value: a new GcmCalibrateDialog object.
 **/
GcmCalibrateDialog *
gcm_calibrate_dialog_new (void)
{
	if (gcm_calibrate_dialog_object != NULL) {
		g_object_ref (gcm_calibrate_dialog_object);
	} else {
		gcm_calibrate_dialog_object = g_object_new (GCM_TYPE_CALIBRATE_DIALOG, NULL);
		g_object_add_weak_pointer (gcm_calibrate_dialog_object, &gcm_calibrate_dialog_object);
	}
	return GCM_CALIBRATE_DIALOG (gcm_calibrate_dialog_object);
}

/***************************************************************************
 ***                          MAKE CHECK TESTS                           ***
 ***************************************************************************/
#ifdef EGG_TEST
#include "egg-test.h"

void
gcm_calibrate_dialog_test (EggTest *test)
{
	GcmCalibrateDialog *calibrate_dialog;
//	gboolean ret;
//	GError *error = NULL;

	if (!egg_test_start (test, "GcmCalibrateDialog"))
		return;

	/************************************************************/
	egg_test_title (test, "get a calibrate_dialog object");
	calibrate_dialog = gcm_calibrate_dialog_new ();
	egg_test_assert (test, calibrate_dialog != NULL);

	g_object_unref (calibrate_dialog);

	egg_test_end (test);
}
#endif


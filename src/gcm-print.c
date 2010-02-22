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

/**
 * SECTION:gcm-print
 * @short_description: Print device abstraction
 *
 * This object allows the programmer to detect a color sensor device.
 */

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "gcm-print.h"

#include "egg-debug.h"

static void     gcm_print_finalize	(GObject     *object);

#define GCM_PRINT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GCM_TYPE_PRINT, GcmPrintPrivate))

/**
 * GcmPrintPrivate:
 *
 * Private #GcmPrint data
 **/
struct _GcmPrintPrivate
{
	gboolean			 present;
	gchar				*vendor;
	GtkPrintSettings		*settings;
};

enum {
	PROP_0,
	PROP_PRESENT,
	PROP_VENDOR,
	PROP_LAST
};

G_DEFINE_TYPE (GcmPrint, gcm_print, G_TYPE_OBJECT)

/**
 * gcm_print_get_property:
 **/
static void
gcm_print_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GcmPrint *print = GCM_PRINT (object);
	GcmPrintPrivate *priv = print->priv;

	switch (prop_id) {
	case PROP_PRESENT:
		g_value_set_boolean (value, priv->present);
		break;
	case PROP_VENDOR:
		g_value_set_string (value, priv->vendor);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gcm_print_set_property:
 **/
static void
gcm_print_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gcm_print_class_init:
 **/
static void
gcm_print_class_init (GcmPrintClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gcm_print_finalize;
	object_class->get_property = gcm_print_get_property;
	object_class->set_property = gcm_print_set_property;

	/**
	 * GcmPrint:present:
	 */
	pspec = g_param_spec_boolean ("present", NULL, NULL,
				      FALSE,
				      G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_PRESENT, pspec);

	/**
	 * GcmPrint:vendor:
	 */
	pspec = g_param_spec_string ("vendor", NULL, NULL,
				     NULL,
				     G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_VENDOR, pspec);

	g_type_class_add_private (klass, sizeof (GcmPrintPrivate));
}

/**
 * gcm_print_begin_print_cb:
 *
 * Emitted after the user has finished changing print settings in the dialog, before the actual rendering starts.
 * A typical use for ::begin-print is to use the parameters from the GtkPrintContext and paginate the document accordingly,
 * and then set the number of pages with gtk_print_operation_set_n_pages().
 **/
static void
gcm_print_begin_print_cb (GtkPrintOperation *operation, GtkPrintContext *context, GcmPrint *print)
{
	gtk_print_operation_set_n_pages (operation, 5);
}

/**
 * gcm_print_draw_page_cb:
 *
 * Emitted for every page that is printed. The signal handler must render the page onto the cairo context
 **/
static void
gcm_print_draw_page_cb (GtkPrintOperation *operation, GtkPrintContext *context, gint page_nr, GcmPrint *print)
{
	cairo_t *cr;
	gdouble width = 0.0f;
	gdouble height = 0.0f;
	gint stride = 0;
	cairo_surface_t *surface;
	unsigned char *data = NULL;

	gtk_print_operation_set_use_full_page (operation, TRUE);
	width = gtk_print_context_get_width (context);
	height = gtk_print_context_get_height (context);
	cr = gtk_print_context_get_cairo_context (context);

//	gdk_cairo_set_source_pixbuf (cr, const GdkPixbuf *pixbuf, 0, 0);

	//stride = cairo_format_stride_for_width (format, width);
	//data = malloc (stride * height);

	surface = cairo_image_surface_create_for_data (data, CAIRO_FORMAT_RGB24, width, height, stride);

	cairo_set_source_surface (cr, surface, 0, 0);

	cairo_surface_destroy (surface);

}

/**
 * gcm_print_images:
 **/
gboolean
gcm_print_images (GcmPrint *print, GtkWindow *window, GPtrArray *filenames, GError **error)
{
	GtkPrintOperation *operation;
	GtkPrintOperationResult res;
	GcmPrintPrivate *priv = print->priv;
	gboolean ret = TRUE;

	/* set some common settings */
	gtk_print_settings_set_n_copies (priv->settings, 1);
	gtk_print_settings_set_number_up (priv->settings, 1);
	gtk_print_settings_set_orientation (priv->settings, GTK_PAGE_ORIENTATION_PORTRAIT);
	gtk_print_settings_set_quality (priv->settings, GTK_PRINT_QUALITY_HIGH);
	gtk_print_settings_set_use_color (priv->settings, TRUE);

	/* create new instance */
	operation = gtk_print_operation_new ();
	gtk_print_operation_set_print_settings (operation, priv->settings);
	g_signal_connect (operation, "begin-print", G_CALLBACK (gcm_print_begin_print_cb), print);
	g_signal_connect (operation, "draw-page", G_CALLBACK (gcm_print_draw_page_cb), print);

	/* do the print UI */
	res = gtk_print_operation_run (operation,
				       GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
				       window, error);

	/* all okay, so save future settings */
	if (res == GTK_PRINT_OPERATION_RESULT_APPLY) {
		g_object_unref (priv->settings);
		priv->settings = g_object_ref (gtk_print_operation_get_print_settings (operation));
	}

	/* rely on error being set */
	if (res == GTK_PRINT_OPERATION_RESULT_ERROR) {
		ret = FALSE;
		goto out;
	}
out:
	g_object_unref (operation);
	return TRUE;
}

/**
 * gcm_print_init:
 **/
static void
gcm_print_init (GcmPrint *print)
{
	print->priv = GCM_PRINT_GET_PRIVATE (print);
	print->priv->vendor = NULL;
	print->priv->settings = gtk_print_settings_new ();
}

/**
 * gcm_print_finalize:
 **/
static void
gcm_print_finalize (GObject *object)
{
	GcmPrint *print = GCM_PRINT (object);
	GcmPrintPrivate *priv = print->priv;

	g_free (priv->vendor);
	g_object_unref (print->priv->settings);

	G_OBJECT_CLASS (gcm_print_parent_class)->finalize (object);
}

/**
 * gcm_print_new:
 *
 * Return value: a new GcmPrint object.
 **/
GcmPrint *
gcm_print_new (void)
{
	GcmPrint *print;
	print = g_object_new (GCM_TYPE_PRINT, NULL);
	return GCM_PRINT (print);
}


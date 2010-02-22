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
	GtkPrintSettings		*settings;
};

G_DEFINE_TYPE (GcmPrint, gcm_print, G_TYPE_OBJECT)

/* temporary object so we can pass state */
typedef struct {
	GcmPrint *print;
	GPtrArray *filenames;
} GcmPrintTask;

/**
 * gcm_print_class_init:
 **/
static void
gcm_print_class_init (GcmPrintClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gcm_print_finalize;
	g_type_class_add_private (klass, sizeof (GcmPrintPrivate));
}

/**
 * gcm_print_begin_print_cb:
 *
 * Emitted after the user has finished changing print settings in the dialog,
 * before the actual rendering starts.
 **/
static void
gcm_print_begin_print_cb (GtkPrintOperation *operation, GtkPrintContext *context, GcmPrintTask *task)
{
	egg_debug ("setting %i pages", task->filenames->len);
	gtk_print_operation_set_n_pages (operation, task->filenames->len);
}

/**
 * gcm_print_draw_page_cb:
 *
 * Emitted for every page that is printed. The signal handler must render the page onto the cairo context
 **/
static void
gcm_print_draw_page_cb (GtkPrintOperation *operation, GtkPrintContext *context, gint page_nr, GcmPrintTask *task)
{
	cairo_t *cr;
	gdouble width = 0.0f;
	gdouble height = 0.0f;
	GError *error = NULL;
	const gchar *filename;
	GdkPixbuf *pixbuf = NULL;

	/* get the size of the page in _pixels_ */
	width = gtk_print_context_get_width (context);
	height = gtk_print_context_get_height (context);
	cr = gtk_print_context_get_cairo_context (context);

	/* load pixbuf */
	filename = g_ptr_array_index (task->filenames, page_nr);
	pixbuf = gdk_pixbuf_new_from_file_at_scale (filename, width, height, FALSE, &error);
	if (pixbuf == NULL) {
		egg_warning ("failed to load image: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* set the pixmap */
	gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
	cairo_paint (cr);
out:
	if (pixbuf != NULL)
		g_object_unref (pixbuf);
}

/**
 * gcm_print_images:
 **/
gboolean
gcm_print_images (GcmPrint *print, GtkWindow *window, GPtrArray *filenames, GError **error)
{
	GcmPrintPrivate *priv = print->priv;
	gboolean ret = TRUE;
	GcmPrintTask *task;
	GtkPrintOperation *operation;
	GtkPrintOperationResult res;

	/* create temp object */
	task = g_new0 (GcmPrintTask, 1);
	task->print = print;
	task->filenames = filenames;

	/* create new instance */
	operation = gtk_print_operation_new ();
	gtk_print_operation_set_print_settings (operation, priv->settings);
	g_signal_connect (operation, "begin-print", G_CALLBACK (gcm_print_begin_print_cb), task);
	g_signal_connect (operation, "draw-page", G_CALLBACK (gcm_print_draw_page_cb), task);

	/* we want this to be as big as possible, modulo page margins */
	gtk_print_operation_set_use_full_page (operation, TRUE);

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

	g_object_unref (priv->settings);

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

/***************************************************************************
 ***                          MAKE CHECK TESTS                           ***
 ***************************************************************************/
#ifdef EGG_TEST
#include "egg-test.h"

void
gcm_print_test (EggTest *test)
{
	gboolean ret;
	GError *error = NULL;
	GcmPrint *print;
	GPtrArray *filenames;

	if (!egg_test_start (test, "GcmPrint"))
		return;

	print = gcm_print_new ();

	/************************************************************/
	egg_test_title (test, "try to print");
	filenames = g_ptr_array_new_with_free_func (g_free);
	g_ptr_array_add (filenames, g_strdup ("/home/hughsie/Desktop/DSC_2182.tif"));
	g_ptr_array_add (filenames, g_strdup ("/home/hughsie/Desktop/DSC_2182-2.tif"));
	ret = gcm_print_images (print, NULL, filenames, &error);
	if (ret)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "failed to print: %s", error->message);
	g_ptr_array_unref (filenames);

	g_object_unref (print);
}
#endif


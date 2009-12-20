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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <stdlib.h>
#include <math.h>

#include "gcm-gamma-widget.h"

#include "egg-debug.h"

G_DEFINE_TYPE (GcmGammaWidget, gcm_gamma_widget, GTK_TYPE_DRAWING_AREA);
#define GCM_GAMMA_WIDGET_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GCM_TYPE_GAMMA_WIDGET, GcmGammaWidgetPrivate))

struct GcmGammaWidgetPrivate
{
	gdouble			 color_light;
	gdouble			 color_dark;
	gdouble			 color_red;
	gdouble			 color_green;
	gdouble			 color_blue;
	guint			 chart_width;
	guint			 chart_height;
	cairo_t			*cr;
	guint			 x_offset;
	guint			 y_offset;
};

static gboolean gcm_gamma_widget_expose (GtkWidget *gamma, GdkEventExpose *event);
static void	gcm_gamma_widget_finalize (GObject *object);

enum
{
	PROP_0,
	PROP_COLOR_LIGHT,
	PROP_COLOR_DARK,
	PROP_COLOR_RED,
	PROP_COLOR_GREEN,
	PROP_COLOR_BLUE,
	PROP_LAST
};

/**
 * dkp_gamma_get_property:
 **/
static void
dkp_gamma_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GcmGammaWidget *gama = GCM_GAMMA_WIDGET (object);
	switch (prop_id) {
	case PROP_COLOR_LIGHT:
		g_value_set_double (value, gama->priv->color_light);
		break;
	case PROP_COLOR_DARK:
		g_value_set_double (value, gama->priv->color_dark);
		break;
	case PROP_COLOR_RED:
		g_value_set_double (value, gama->priv->color_red);
		break;
	case PROP_COLOR_GREEN:
		g_value_set_double (value, gama->priv->color_green);
		break;
	case PROP_COLOR_BLUE:
		g_value_set_double (value, gama->priv->color_blue);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * dkp_gamma_set_property:
 **/
static void
dkp_gamma_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	GcmGammaWidget *gama = GCM_GAMMA_WIDGET (object);

	switch (prop_id) {
	case PROP_COLOR_LIGHT:
		gama->priv->color_light = g_value_get_double (value);
		break;
	case PROP_COLOR_DARK:
		gama->priv->color_dark = g_value_get_double (value);
		break;
	case PROP_COLOR_RED:
		gama->priv->color_red = g_value_get_double (value);
		break;
	case PROP_COLOR_GREEN:
		gama->priv->color_green = g_value_get_double (value);
		break;
	case PROP_COLOR_BLUE:
		gama->priv->color_blue = g_value_get_double (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}

	/* refresh widget */
	gtk_widget_hide (GTK_WIDGET (gama));
	gtk_widget_show (GTK_WIDGET (gama));
}

/**
 * gcm_gamma_widget_class_init:
 **/
static void
gcm_gamma_widget_class_init (GcmGammaWidgetClass *class)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	widget_class->expose_event = gcm_gamma_widget_expose;
	object_class->get_property = dkp_gamma_get_property;
	object_class->set_property = dkp_gamma_set_property;
	object_class->finalize = gcm_gamma_widget_finalize;

	g_type_class_add_private (class, sizeof (GcmGammaWidgetPrivate));

	/* properties */
	g_object_class_install_property (object_class,
					 PROP_COLOR_LIGHT,
					 g_param_spec_double ("color-light", NULL, NULL,
							       0.0f, G_MAXDOUBLE, 0.0f,
							       G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_COLOR_DARK,
					 g_param_spec_double ("color-dark", NULL, NULL,
							       0.0f, G_MAXDOUBLE, 0.0f,
							       G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_COLOR_RED,
					 g_param_spec_double ("color-red", NULL, NULL,
							       0.0f, G_MAXDOUBLE, 0.0f,
							       G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_COLOR_GREEN,
					 g_param_spec_double ("color-green", NULL, NULL,
							       0.0f, G_MAXDOUBLE, 0.0f,
							       G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_COLOR_BLUE,
					 g_param_spec_double ("color-blue", NULL, NULL,
							       0.0f, G_MAXDOUBLE, 0.0f,
							       G_PARAM_READWRITE));
}

/**
 * gcm_gamma_widget_init:
 **/
static void
gcm_gamma_widget_init (GcmGammaWidget *gama)
{
	PangoFontMap *fontmap;
	PangoContext *context;

	gama->priv = GCM_GAMMA_WIDGET_GET_PRIVATE (gama);
	gama->priv->color_light = 1.0f;
	gama->priv->color_dark = 0.0f;
	gama->priv->color_red = 0.5f;
	gama->priv->color_green = 0.5f;
	gama->priv->color_blue = 0.5f;

	/* do pango stuff */
	fontmap = pango_cairo_font_map_get_default ();
	context = pango_cairo_font_map_create_context (PANGO_CAIRO_FONT_MAP (fontmap));
	pango_context_set_base_gravity (context, PANGO_GRAVITY_AUTO);
}

/**
 * gcm_gamma_widget_finalize:
 **/
static void
gcm_gamma_widget_finalize (GObject *object)
{
//	GcmGammaWidget *gama = (GcmGammaWidget*) object;
	G_OBJECT_CLASS (gcm_gamma_widget_parent_class)->finalize (object);
}

/**
 * gcm_gamma_widget_draw_lines:
 **/
static void
gcm_gamma_widget_draw_lines (GcmGammaWidget *gama, cairo_t *cr)
{
	guint i;
	gdouble dark;
	gdouble light;

	/* just copy */
	dark = gama->priv->color_dark;
	light = gama->priv->color_light;

	cairo_save (cr);
	cairo_set_line_width (cr, 1);

	/* do horizontal lines */
	for (i=0; i<gama->priv->chart_height; i++) {

		/* set correct color */
		if (i%2 == 0) {
			cairo_set_source_rgb (cr, dark, dark, dark);
		} else {
			cairo_set_source_rgb (cr, light, light, light);
		}
		cairo_move_to (cr, 0.5, i + 0.5f);
		cairo_line_to (cr, gama->priv->chart_width - 1, i + 0.5f);
		cairo_stroke (cr);
	}

	cairo_restore (cr);
}

/**
 * gcm_gamma_widget_draw_box:
 **/
static void
gcm_gamma_widget_draw_box (GcmGammaWidget *gama, cairo_t *cr)
{
	guint box_width;
	guint box_height;
	guint mid_x;
	guint mid_y;

	cairo_save (cr);
	cairo_set_line_width (cr, 1);

	/* half the size in either direction */
	box_width = gama->priv->chart_width / 4;
	box_height = gama->priv->chart_height / 4;
	mid_x = gama->priv->chart_width / 2;
	mid_y = gama->priv->chart_height / 2;

	/* plain box */
	cairo_set_source_rgb (cr, gama->priv->color_red, gama->priv->color_green, gama->priv->color_blue);
	cairo_rectangle (cr, mid_x - box_width + 0.5f, (((mid_y - box_height)/2)*2) + 0.0f, box_width*2 + 0.5f, (((box_height*2)/2)*2) + 1.0f);
	cairo_fill (cr);

	cairo_restore (cr);
}

/**
 * gcm_gamma_widget_draw_bounding_box:
 **/
static void
gcm_gamma_widget_draw_bounding_box (cairo_t *cr, gint x, gint y, gint width, gint height)
{
	/* background */
	cairo_rectangle (cr, x, y, width, height);
	cairo_set_source_rgb (cr, 1, 1, 1);
	cairo_fill (cr);

	/* solid outline box */
	cairo_rectangle (cr, x + 0.5f, y + 0.5f, width - 1, height - 1);
	cairo_set_source_rgb (cr, 0.1, 0.1, 0.1);
	cairo_set_line_width (cr, 1);
	cairo_stroke (cr);
}

/**
 * gcm_gamma_widget_draw:
 **/
static void
gcm_gamma_widget_draw (GtkWidget *gamma_widget, cairo_t *cr)
{
	GtkAllocation allocation;

	GcmGammaWidget *gama = (GcmGammaWidget*) gamma_widget;
	g_return_if_fail (gama != NULL);
	g_return_if_fail (GCM_IS_GAMMA_WIDGET (gama));

	cairo_save (cr);

	/* make size adjustment */
	gtk_widget_get_allocation (gamma_widget, &allocation);
	gama->priv->chart_height = ((guint) (allocation.height / 2) * 2) - 1;
	gama->priv->chart_width = allocation.width;
	gama->priv->x_offset = 1;
	gama->priv->y_offset = 1;

	/* gamma background */
	gcm_gamma_widget_draw_bounding_box (cr, 0, 0, gama->priv->chart_width, gama->priv->chart_height);
	gcm_gamma_widget_draw_lines (gama, cr);
	gcm_gamma_widget_draw_box (gama, cr);

	cairo_restore (cr);
}

/**
 * gcm_gamma_widget_expose:
 *
 * Just repaint the entire gamma widget on expose.
 **/
static gboolean
gcm_gamma_widget_expose (GtkWidget *gamma_widget, GdkEventExpose *event)
{
	cairo_t *cr;

	/* get a cairo_t */
	cr = gdk_cairo_create (gtk_widget_get_window (gamma_widget));
	cairo_rectangle (cr,
			 event->area.x, event->area.y,
			 event->area.width, event->area.height);
	cairo_clip (cr);
	((GcmGammaWidget *)gamma_widget)->priv->cr = cr;

	gcm_gamma_widget_draw (gamma_widget, cr);

	cairo_destroy (cr);
	return FALSE;
}

/**
 * gcm_gamma_widget_new:
 * Return value: A new GcmGammaWidget object.
 **/
GtkWidget *
gcm_gamma_widget_new (void)
{
	return g_object_new (GCM_TYPE_GAMMA_WIDGET, NULL);
}

/***************************************************************************
 ***                          MAKE CHECK TESTS                           ***
 ***************************************************************************/
#ifdef EGG_TEST
#include "egg-test.h"

void
gcm_gamma_widget_test (EggTest *test)
{
	GtkWidget *widget;
	GtkWidget *image;
	GtkWidget *dialog;
	GtkWidget *vbox;
	gboolean ret;
	GError *error = NULL;
	gint response;
	gchar *filename_image;

	if (!egg_test_start (test, "GcmGammaWidget"))
		return;

	/************************************************************/
	egg_test_title (test, "get a gamma widget object");
	widget = gcm_gamma_widget_new ();
	egg_test_assert (test, widget != NULL);

	g_object_set (widget,
		      "color-light", 0.5f,
		      "color-dark", 0.0f,
		      "color-red", 0.25f,
		      "color-green", 0.25f,
		      "color-blue", 0.25f,
		      NULL);

	/************************************************************/
	egg_test_title (test, "get filename of image file");
	filename_image = egg_test_get_data_file ("gamma-widget.png");
	egg_test_assert (test, (filename_image != NULL));

	/* show in a dialog as an example */
	dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, "Does GAMMA widget match\nthe picture below?");
	image = gtk_image_new_from_file (filename_image);
	vbox = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	gtk_box_pack_end (GTK_BOX(vbox), widget, TRUE, TRUE, 12);
	gtk_box_pack_end (GTK_BOX(vbox), image, TRUE, TRUE, 12);
	gtk_widget_set_size_request (widget, 300, 300);
	gtk_window_set_resizable (GTK_WINDOW (dialog), TRUE);
	gtk_widget_show (widget);
	gtk_widget_show (image);

	response = gtk_dialog_run (GTK_DIALOG (dialog));

	/************************************************************/
	egg_test_title (test, "plotted as expected?");
	egg_test_assert (test, (response == GTK_RESPONSE_YES));

	gtk_widget_destroy (dialog);

	g_free (filename_image);

	egg_test_end (test);
}
#endif


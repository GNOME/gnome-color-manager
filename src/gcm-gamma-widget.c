/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009-2010 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include "config.h"
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <stdlib.h>
#include <math.h>

#include "gcm-gamma-widget.h"

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
};

static gboolean gcm_gamma_widget_draw (GtkWidget *gamma, cairo_t *cr);
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

static void
gcm_gamma_widget_class_init (GcmGammaWidgetClass *class)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	widget_class->draw = gcm_gamma_widget_draw;
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

static void
gcm_gamma_widget_init (GcmGammaWidget *gama)
{
	PangoContext *context;

	gama->priv = GCM_GAMMA_WIDGET_GET_PRIVATE (gama);
	gama->priv->color_light = 1.0f;
	gama->priv->color_dark = 0.0f;
	gama->priv->color_red = 0.5f;
	gama->priv->color_green = 0.5f;
	gama->priv->color_blue = 0.5f;

	/* do pango stuff */
	context = gtk_widget_get_pango_context (GTK_WIDGET (gama));
	pango_context_set_base_gravity (context, PANGO_GRAVITY_AUTO);
}

static void
gcm_gamma_widget_finalize (GObject *object)
{
//	GcmGammaWidget *gama = (GcmGammaWidget*) object;
	G_OBJECT_CLASS (gcm_gamma_widget_parent_class)->finalize (object);
}

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
	for (i = 0; i < gama->priv->chart_height; i++) {

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

static gboolean
gcm_gamma_widget_draw (GtkWidget *gamma_widget, cairo_t *cr)
{
	GtkAllocation allocation;

	GcmGammaWidget *gama = (GcmGammaWidget*) gamma_widget;
	g_return_val_if_fail (gama != NULL, FALSE);
	g_return_val_if_fail (GCM_IS_GAMMA_WIDGET (gama), FALSE);

	/* make size adjustment */
	gtk_widget_get_allocation (gamma_widget, &allocation);
	if (allocation.height <= 5 || allocation.width <= 5)
		return FALSE;

	/* save */
	gama->priv->chart_height = ((guint) (allocation.height / 2) * 2) - 1;
	gama->priv->chart_width = allocation.width;

	/* gamma background */
	gcm_gamma_widget_draw_bounding_box (cr, 0, 0, gama->priv->chart_width, gama->priv->chart_height);
	gcm_gamma_widget_draw_lines (gama, cr);
	gcm_gamma_widget_draw_box (gama, cr);
	return FALSE;
}

GtkWidget *
gcm_gamma_widget_new (void)
{
	return g_object_new (GCM_TYPE_GAMMA_WIDGET, NULL);
}


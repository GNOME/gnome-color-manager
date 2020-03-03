/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2006-2010 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include "config.h"
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <stdlib.h>
#include <math.h>
#include <colord.h>

#include "gcm-trc-widget.h"

G_DEFINE_TYPE (GcmTrcWidget, gcm_trc_widget, GTK_TYPE_DRAWING_AREA);
#define GCM_TRC_WIDGET_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GCM_TYPE_TRC_WIDGET, GcmTrcWidgetPrivate))
#define GCM_TRC_WIDGET_FONT "Sans 8"

struct GcmTrcWidgetPrivate
{
	gboolean		 use_grid;
	GPtrArray		*data;
	guint			 chart_width;
	guint			 chart_height;
	PangoLayout		*layout;
	guint			 x_offset;
	guint			 y_offset;
};

static gboolean gcm_trc_widget_draw (GtkWidget *trc, cairo_t *cr);
static void	gcm_trc_widget_finalize (GObject *object);

enum
{
	PROP_0,
	PROP_USE_GRID,
	PROP_DATA,
	PROP_LAST
};

static void
gcm_trc_widget_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GcmTrcWidget *trc = GCM_TRC_WIDGET (object);
	switch (prop_id) {
	case PROP_USE_GRID:
		g_value_set_boolean (value, trc->priv->use_grid);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gcm_trc_widget_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	GcmTrcWidget *trc = GCM_TRC_WIDGET (object);

	switch (prop_id) {
	case PROP_USE_GRID:
		trc->priv->use_grid = g_value_get_boolean (value);
		break;
	case PROP_DATA:
		if (trc->priv->data != NULL)
			g_ptr_array_unref (trc->priv->data);
		trc->priv->data = g_ptr_array_ref (g_value_get_boxed (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}

	/* refresh widget */
	gtk_widget_hide (GTK_WIDGET (trc));
	gtk_widget_show (GTK_WIDGET (trc));
}

static void
gcm_trc_widget_class_init (GcmTrcWidgetClass *class)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	widget_class->draw = gcm_trc_widget_draw;
	object_class->get_property = gcm_trc_widget_get_property;
	object_class->set_property = gcm_trc_widget_set_property;
	object_class->finalize = gcm_trc_widget_finalize;

	g_type_class_add_private (class, sizeof (GcmTrcWidgetPrivate));

	/* properties */
	g_object_class_install_property (object_class,
					 PROP_USE_GRID,
					 g_param_spec_boolean ("use-grid", NULL, NULL,
							       TRUE,
							       G_PARAM_READWRITE));
	g_object_class_install_property (object_class,
					 PROP_DATA,
					 g_param_spec_boxed ("data", NULL, NULL,
							     G_TYPE_PTR_ARRAY,
							     G_PARAM_WRITABLE));
}

static void
gcm_trc_widget_init (GcmTrcWidget *trc)
{
	PangoContext *context;
	PangoFontDescription *desc;

	trc->priv = GCM_TRC_WIDGET_GET_PRIVATE (trc);
	trc->priv->use_grid = TRUE;
	trc->priv->data = NULL;

	/* do pango stuff */
	context = gtk_widget_get_pango_context (GTK_WIDGET (trc));
	pango_context_set_base_gravity (context, PANGO_GRAVITY_AUTO);

	trc->priv->layout = pango_layout_new (context);
	desc = pango_font_description_from_string (GCM_TRC_WIDGET_FONT);
	pango_layout_set_font_description (trc->priv->layout, desc);
	pango_font_description_free (desc);
}

static void
gcm_trc_widget_finalize (GObject *object)
{
	GcmTrcWidget *trc = (GcmTrcWidget*) object;

	g_object_unref (trc->priv->layout);
	if (trc->priv->data != NULL)
		g_ptr_array_unref (trc->priv->data);
	G_OBJECT_CLASS (gcm_trc_widget_parent_class)->finalize (object);
}

static void
gcm_trc_widget_draw_grid (GcmTrcWidget *trc, cairo_t *cr)
{
	guint i;
	gdouble b;
	gdouble dotted[] = {1., 2.};
	gdouble divwidth  = (gdouble)trc->priv->chart_width / 10.0f;
	gdouble divheight = (gdouble)trc->priv->chart_height / 10.0f;

	cairo_save (cr);
	cairo_set_line_width (cr, 1);
	cairo_set_dash (cr, dotted, 2, 0.0);

	/* do vertical lines */
	cairo_set_source_rgb (cr, 0.1, 0.1, 0.1);
	for (i=1; i<10; i++) {
		b = ((gdouble) i * divwidth);
		cairo_move_to (cr, (gint)b + 0.5f, 0);
		cairo_line_to (cr, (gint)b + 0.5f, trc->priv->chart_height);
		cairo_stroke (cr);
	}

	/* do horizontal lines */
	for (i=1; i<10; i++) {
		b = ((gdouble) i * divheight);
		cairo_move_to (cr, 0, (gint)b + 0.5f);
		cairo_line_to (cr, trc->priv->chart_width, (int)b + 0.5f);
		cairo_stroke (cr);
	}

	cairo_restore (cr);
}

static void
gcm_trc_widget_map_to_display (GcmTrcWidget *trc, gdouble x, gdouble y, gdouble *x_retval, gdouble *y_retval)
{
	GcmTrcWidgetPrivate *priv = trc->priv;

	*x_retval = (x * (priv->chart_width - 1)) + priv->x_offset;
	*y_retval = ((priv->chart_height - 1) - y * (priv->chart_height - 1)) - priv->y_offset;
}

static void
gcm_trc_widget_draw_line (GcmTrcWidget *trc, cairo_t *cr)
{
	gdouble wx, wy;
	GcmTrcWidgetPrivate *priv = trc->priv;
	CdColorRGB *tmp;
	gfloat i;
	gfloat value;
	gfloat size;
	gfloat linewidth;

	/* nothing set yet */
	if (priv->data == NULL)
		return;

	/* set according to widget width */
	linewidth = priv->chart_width / 250.0f;
	size = priv->data->len;

	cairo_save (cr);

	/* do red */
	cairo_set_line_width (cr, linewidth + 1.0f);
	cairo_set_source_rgb (cr, 0.5f, 0.0f, 0.0f);
	for (i = 0; i < size; i++) {
		tmp = g_ptr_array_index (priv->data, (guint) i);
		value = tmp->R;
		gcm_trc_widget_map_to_display (trc, i/(size-1), value, &wx, &wy);
		if (i == 0)
			cairo_move_to (cr, wx, wy+1);
		else
			cairo_line_to (cr, wx, wy+1);
	}
	cairo_stroke_preserve (cr);
	cairo_set_line_width (cr, linewidth);
	cairo_set_source_rgb (cr, 1.0f, 0.0f, 0.0f);
	cairo_stroke (cr);

	/* do green */
	cairo_set_line_width (cr, linewidth + 1.0f);
	cairo_set_source_rgb (cr, 0.0f, 0.5f, 0.0f);
	for (i = 0; i < size; i++) {
		tmp = g_ptr_array_index (priv->data, (guint) i);
		value = tmp->G;
		gcm_trc_widget_map_to_display (trc, i/(size-1), value, &wx, &wy);
		if (i == 0)
			cairo_move_to (cr, wx, wy-1);
		else
			cairo_line_to (cr, wx, wy-1);
	}
	cairo_stroke_preserve (cr);
	cairo_set_line_width (cr, linewidth);
	cairo_set_source_rgb (cr, 0.0f, 1.0f, 0.0f);
	cairo_stroke (cr);

	/* do blue */
	cairo_set_line_width (cr, linewidth + 1.0f);
	cairo_set_source_rgb (cr, 0.0f, 0.0f, 0.5f);
	for (i = 0; i < size; i++) {
		tmp = g_ptr_array_index (priv->data, (guint) i);
		value = tmp->B;
		gcm_trc_widget_map_to_display (trc, i/(size-1), value, &wx, &wy);
		if (i == 0)
			cairo_move_to (cr, wx, wy);
		else
			cairo_line_to (cr, wx, wy);
	}
	cairo_stroke_preserve (cr);
	cairo_set_line_width (cr, linewidth);
	cairo_set_source_rgb (cr, 0.0f, 0.0f, 1.0f);
	cairo_stroke (cr);

	cairo_restore (cr);
}

static void
gcm_trc_widget_draw_bounding_box (cairo_t *cr, gint x, gint y, gint width, gint height)
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

static void
gcm_trc_widget_draw_trc (GtkWidget *trc_widget, cairo_t *cr)
{
	GtkAllocation allocation;

	GcmTrcWidget *trc = (GcmTrcWidget*) trc_widget;
	g_return_if_fail (trc != NULL);
	g_return_if_fail (GCM_IS_TRC_WIDGET (trc));

	cairo_save (cr);

	/* make size adjustment */
	gtk_widget_get_allocation (trc_widget, &allocation);
	trc->priv->chart_height = allocation.height;
	trc->priv->chart_width = allocation.width;
	trc->priv->x_offset = 1;
	trc->priv->y_offset = 1;

	/* trc background */
	gcm_trc_widget_draw_bounding_box (cr, 0, 0, trc->priv->chart_width, trc->priv->chart_height);
	if (trc->priv->use_grid)
		gcm_trc_widget_draw_grid (trc, cr);

	gcm_trc_widget_draw_line (trc, cr);

	cairo_restore (cr);
}

static gboolean
gcm_trc_widget_draw (GtkWidget *trc, cairo_t *cr)
{
	gcm_trc_widget_draw_trc (trc, cr);
	return FALSE;
}

GtkWidget *
gcm_trc_widget_new (void)
{
	return g_object_new (GCM_TYPE_TRC_WIDGET, NULL);
}


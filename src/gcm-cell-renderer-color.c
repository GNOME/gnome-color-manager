/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011-2015 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <lcms2.h>

#include "gcm-cell-renderer-color.h"

enum {
	PROP_0,
	PROP_COLOR,
	PROP_PROFILE,
	PROP_LAST
};

G_DEFINE_TYPE (GcmCellRendererColor, gcm_cell_renderer_color, GTK_TYPE_CELL_RENDERER_PIXBUF)

static gpointer parent_class = NULL;

static void
gcm_cell_renderer_color_get_property (GObject *object, guint param_id,
				      GValue *value, GParamSpec *pspec)
{
	GcmCellRendererColor *renderer = GCM_CELL_RENDERER_COLOR (object);

	switch (param_id) {
	case PROP_COLOR:
		g_value_set_boxed (value, g_boxed_copy (CD_TYPE_COLOR_XYZ,
							renderer->color));
		break;
	case PROP_PROFILE:
		g_value_set_object (value, renderer->profile);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
gcm_cell_renderer_set_color (GcmCellRendererColor *renderer)
{
	CdColorRGB8 rgb;
	g_autoptr(GdkPixbuf) pixbuf = NULL;
	gint height = 26; /* TODO: needs to be a property */
	gint width = 400; /* TODO: needs to be a property */
	gint x, y;
	guchar *pixels;
	guint pos;
	cmsHPROFILE profile_srgb = NULL;
	cmsHPROFILE profile_lab = NULL;
	cmsHTRANSFORM xform = NULL;

	/* nothing set yet */
	if (renderer->color == NULL)
		goto out;

	/* convert the color to sRGB */
	profile_lab = cmsCreateLab2Profile (NULL);
	profile_srgb = cmsCreate_sRGBProfile ();
	xform = cmsCreateTransform (profile_lab, TYPE_Lab_DBL,
				    profile_srgb, TYPE_RGB_8,
				    INTENT_ABSOLUTE_COLORIMETRIC, 0);
	cmsDoTransform (xform, renderer->color, &rgb, 1);

	/* create a pixbuf of the right size */
	pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, width, height);
	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);
	pixels = gdk_pixbuf_get_pixels (pixbuf);
	for (y=0; y<height; y++) {
		for (x=0; x<width; x++) {
			pos = (y*width+x) * 3;
			pixels[pos+0] = rgb.R;
			pixels[pos+1] = rgb.G;
			pixels[pos+2] = rgb.B;
		}
	}
out:
	g_object_set (renderer, "pixbuf", pixbuf, NULL);
	if (profile_srgb != NULL)
		cmsCloseProfile (profile_srgb);
	if (profile_lab != NULL)
		cmsCloseProfile (profile_lab);
	if (xform != NULL)
		cmsDeleteTransform (xform);
}

static void
gcm_cell_renderer_color_set_property (GObject *object, guint param_id,
				      const GValue *value, GParamSpec *pspec)
{
	CdColorLab *tmp;
	GcmCellRendererColor *renderer = GCM_CELL_RENDERER_COLOR (object);

	switch (param_id) {
	case PROP_COLOR:
		tmp = g_value_get_boxed (value);
		if (tmp == NULL)
			return;
		cd_color_lab_copy (tmp, renderer->color);
		gcm_cell_renderer_set_color (renderer);
		break;
	case PROP_PROFILE:
		if (renderer->profile != NULL)
			g_object_unref (renderer->profile);
		renderer->color = g_value_dup_object (value);
		gcm_cell_renderer_set_color (renderer);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
gcm_cell_renderer_finalize (GObject *object)
{
	GcmCellRendererColor *renderer;
	renderer = GCM_CELL_RENDERER_COLOR (object);
	g_free (renderer->icon_name);
	cd_color_lab_free (renderer->color);
	if (renderer->profile != NULL)
		g_object_unref (renderer->profile);
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gcm_cell_renderer_color_class_init (GcmCellRendererColorClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);
	object_class->finalize = gcm_cell_renderer_finalize;

	parent_class = g_type_class_peek_parent (class);

	object_class->get_property = gcm_cell_renderer_color_get_property;
	object_class->set_property = gcm_cell_renderer_color_set_property;

	g_object_class_install_property (object_class, PROP_COLOR,
					 g_param_spec_boxed ("color", NULL,
					 NULL,
					 CD_TYPE_COLOR_XYZ,
					 G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_PROFILE,
					 g_param_spec_object ("profile", NULL,
					 NULL,
					 CD_TYPE_PROFILE,
					 G_PARAM_READWRITE));
}

static void
gcm_cell_renderer_color_init (GcmCellRendererColor *renderer)
{
	renderer->color = cd_color_lab_new ();
}

GtkCellRenderer *
gcm_cell_renderer_color_new (void)
{
	return g_object_new (GCM_TYPE_CELL_RENDERER_COLOR, NULL);
}


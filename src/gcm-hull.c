/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011 Richard Hughes <richard@hughsie.com>
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

#include <glib-object.h>
#include <math.h>
#include <colord.h>

#include "gcm-hull.h"

static void     gcm_hull_finalize	(GObject     *object);

#define GCM_HULL_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GCM_TYPE_HULL, GcmHullPrivate))

/**
 * GcmHullPrivate:
 *
 * Private #GcmHull data
 **/
struct _GcmHullPrivate
{
	GPtrArray		*vertices;
	GPtrArray		*faces;
	guint			 flags;
};

typedef struct {
	CdColorXYZ	 xyz;
	CdColorRGB	 color;
} GcmHullVertex;

typedef struct {
	guint		*data;
	gsize		 size;
} GcmHullFace;

enum {
	PROP_0,
	PROP_FLAGS,
	PROP_LAST
};

G_DEFINE_TYPE (GcmHull, gcm_hull, G_TYPE_OBJECT)

/**
 * g_hull_vertex_free:
 **/
static void
g_hull_vertex_free (GcmHullVertex *vertex)
{
	g_slice_free (GcmHullVertex, vertex);
}

/**
 * g_hull_face_free:
 **/
static void
g_hull_face_free (GcmHullFace *face)
{
	g_free (face->data);
	g_slice_free (GcmHullFace, face);
}

/**
 * gcm_hull_add_vertex:
 **/
void
gcm_hull_add_vertex (GcmHull *hull,
		     CdColorXYZ *xyz,
		     CdColorRGB *color)
{
	GcmHullVertex *new;
	new = g_slice_new (GcmHullVertex);
	cd_color_xyz_copy (xyz, &new->xyz);
	cd_color_rgb_copy (color, &new->color);
	g_ptr_array_add (hull->priv->vertices, new);
}

/**
 * gcm_hull_add_face:
 **/
void
gcm_hull_add_face (GcmHull *hull,
		   const guint *data,
		   gsize size)
{
	GcmHullFace *face;
	face = g_slice_new (GcmHullFace);
	face->data = g_memdup (data, sizeof(guint) * size);
	face->size = size;
	g_ptr_array_add (hull->priv->faces, face);
}

/**
 * gcm_hull_export_to_ply:
 **/
gchar *
gcm_hull_export_to_ply (GcmHull *hull)
{
	gchar xyz_str[3][G_ASCII_DTOSTR_BUF_SIZE];
	GString *string;
	guint i;
	GcmHullFace *face;
	GcmHullVertex *vertex;
	CdColorRGB8 tmp;

	string = g_string_new ("ply\nformat ascii 1.0\n");
	g_string_append_printf (string, "element vertex %i\n",
				hull->priv->vertices->len);
	g_string_append (string, "property float x\n");
	g_string_append (string, "property float y\n");
	g_string_append (string, "property float z\n");
	g_string_append (string, "property uchar red\n");
	g_string_append (string, "property uchar green\n");
	g_string_append (string, "property uchar blue\n");
	g_string_append_printf (string, "element face %i\n",
				hull->priv->faces->len);
	g_string_append (string, "property list uchar uint vertex_indices\n");
	g_string_append (string, "end_header\n");

	for (i=0; i<hull->priv->vertices->len; i++) {
		vertex = g_ptr_array_index (hull->priv->vertices, i);
		cd_color_rgb_to_rgb8 (&vertex->color, &tmp);
		g_ascii_dtostr (xyz_str[0], G_ASCII_DTOSTR_BUF_SIZE, vertex->xyz.X);
		g_ascii_dtostr (xyz_str[1], G_ASCII_DTOSTR_BUF_SIZE, vertex->xyz.Y);
		g_ascii_dtostr (xyz_str[2], G_ASCII_DTOSTR_BUF_SIZE, vertex->xyz.Z);
		g_string_append_printf (string, "%s %s %s %i %i %i\n",
					xyz_str[0],
					xyz_str[1],
					xyz_str[2],
					tmp.R,
					tmp.G,
					tmp.B);
	}

	for (i=0; i<hull->priv->faces->len; i++) {
		face = g_ptr_array_index (hull->priv->faces, i);
		g_string_append_printf (string, "3 %i %i %i\n",
					face->data[0],
					face->data[1],
					face->data[2]);
	}

	return g_string_free (string, FALSE);
}

/**
 * gcm_hull_get_flags:
 **/
guint
gcm_hull_get_flags (GcmHull *hull)
{
	g_return_val_if_fail (GCM_IS_HULL (hull), 0);
	return hull->priv->flags;
}

/**
 * gcm_hull_set_flags:
 **/
void
gcm_hull_set_flags (GcmHull *hull, guint flags)
{
	g_return_if_fail (GCM_IS_HULL (hull));
	hull->priv->flags = flags;
}


/**
 * gcm_hull_get_property:
 **/
static void
gcm_hull_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GcmHull *hull = GCM_HULL (object);
	GcmHullPrivate *priv = hull->priv;

	switch (prop_id) {
	case PROP_FLAGS:
		g_value_set_uint (value, priv->flags);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gcm_hull_set_property:
 **/
static void
gcm_hull_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gcm_hull_class_init:
 **/
static void
gcm_hull_class_init (GcmHullClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gcm_hull_finalize;
	object_class->get_property = gcm_hull_get_property;
	object_class->set_property = gcm_hull_set_property;

	/**
	 * GcmHull:width:
	 */
	pspec = g_param_spec_uint ("flags", "flags for rendering", NULL,
				   0, G_MAXUINT, 0,
				   G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_FLAGS, pspec);

	g_type_class_add_private (klass, sizeof (GcmHullPrivate));
}

/**
 * gcm_hull_init:
 **/
static void
gcm_hull_init (GcmHull *hull)
{
	hull->priv = GCM_HULL_GET_PRIVATE (hull);
	hull->priv->vertices = g_ptr_array_new_with_free_func ((GDestroyNotify) g_hull_vertex_free);
	hull->priv->faces = g_ptr_array_new_with_free_func ((GDestroyNotify) g_hull_face_free);
}

/**
 * gcm_hull_finalize:
 **/
static void
gcm_hull_finalize (GObject *object)
{
	GcmHull *hull = GCM_HULL (object);
	GcmHullPrivate *priv = hull->priv;

	g_ptr_array_unref (priv->vertices);
	g_ptr_array_unref (priv->faces);

	G_OBJECT_CLASS (gcm_hull_parent_class)->finalize (object);
}

/**
 * gcm_hull_new:
 *
 * Return value: a new #GcmHull object.
 **/
GcmHull *
gcm_hull_new (void)
{
	GcmHull *hull;
	hull = g_object_new (GCM_TYPE_HULL, NULL);
	return GCM_HULL (hull);
}

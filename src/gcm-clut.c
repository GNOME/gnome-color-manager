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

#include <glib-object.h>
#include <math.h>
#include <gio/gio.h>

#include "gcm-clut.h"

static void     gcm_clut_finalize	(GObject     *object);

#define GCM_CLUT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GCM_TYPE_CLUT, GcmClutPrivate))

/**
 * GcmClutPrivate:
 *
 * Private #GcmClut data
 **/
struct _GcmClutPrivate
{
	GPtrArray 			*array;
	guint				 size;
};

enum {
	PROP_0,
	PROP_SIZE,
	PROP_LAST
};

G_DEFINE_TYPE (GcmClut, gcm_clut, G_TYPE_OBJECT)

/**
 * gcm_clut_set_source_array:
 **/
gboolean
gcm_clut_set_source_array (GcmClut *clut, GPtrArray *array)
{
	g_return_val_if_fail (GCM_IS_CLUT (clut), FALSE);
	g_return_val_if_fail (array != NULL, FALSE);

	/* just take a reference */
	if (clut->priv->array != NULL)
		g_ptr_array_unref (clut->priv->array);
	clut->priv->array = g_ptr_array_ref (array);

	/* set size from input array */
	clut->priv->size = array->len;

	/* all okay */
	return TRUE;
}

/**
 * gcm_clut_reset:
 **/
gboolean
gcm_clut_reset (GcmClut *clut)
{
	g_return_val_if_fail (GCM_IS_CLUT (clut), FALSE);

	/* setup nothing */
	g_ptr_array_set_size (clut->priv->array, 0);
	return TRUE;
}

/**
 * gcm_clut_get_size:
 **/
guint
gcm_clut_get_size (GcmClut *clut)
{
	return clut->priv->size;
}

/**
 * gcm_clut_get_array:
 **/
GPtrArray *
gcm_clut_get_array (GcmClut *clut)
{
	GPtrArray *array;
	guint i;
	guint value;
	const GcmClutData *tmp;
	GcmClutData *data;

	g_return_val_if_fail (GCM_IS_CLUT (clut), FALSE);

	array = g_ptr_array_new_with_free_func (g_free);
	if (clut->priv->array->len == 0) {
		/* generate a dummy gamma */
		g_debug ("falling back to dummy gamma");
		for (i=0; i<clut->priv->size; i++) {
			value = (i * 0xffff) / (clut->priv->size - 1);
			data = g_new0 (GcmClutData, 1);
			data->red = value;
			data->green = value;
			data->blue = value;
			g_ptr_array_add (array, data);
		}
	} else {
		/* just make a 1:1 copy */
		for (i=0; i<clut->priv->size; i++) {
			tmp = g_ptr_array_index (clut->priv->array, i);
			data = g_new0 (GcmClutData, 1);
			data->red = tmp->red;
			data->green = tmp->green;
			data->blue = tmp->blue;
			g_ptr_array_add (array, data);
		}
	}
	return array;
}

/**
 * gcm_clut_print:
 **/
void
gcm_clut_print (GcmClut *clut)
{
	GPtrArray *array;
	guint i;
	GcmClutData *tmp;

	g_return_if_fail (GCM_IS_CLUT (clut));
	array = clut->priv->array;
	for (i=0; i<array->len; i++) {
		tmp = g_ptr_array_index (array, i);
		g_print ("%x %x %x\n", tmp->red, tmp->green, tmp->blue);
	}
}

/**
 * gcm_clut_get_property:
 **/
static void
gcm_clut_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GcmClut *clut = GCM_CLUT (object);
	GcmClutPrivate *priv = clut->priv;

	switch (prop_id) {
	case PROP_SIZE:
		g_value_set_uint (value, priv->size);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gcm_clut_set_property:
 **/
static void
gcm_clut_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	GcmClut *clut = GCM_CLUT (object);
	GcmClutPrivate *priv = clut->priv;

	switch (prop_id) {
	case PROP_SIZE:
		priv->size = g_value_get_uint (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gcm_clut_class_init:
 **/
static void
gcm_clut_class_init (GcmClutClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gcm_clut_finalize;
	object_class->get_property = gcm_clut_get_property;
	object_class->set_property = gcm_clut_set_property;

	/**
	 * GcmClut:size:
	 */
	pspec = g_param_spec_uint ("size", NULL, NULL,
				   0, G_MAXUINT, 0,
				   G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_SIZE, pspec);

	g_type_class_add_private (klass, sizeof (GcmClutPrivate));
}

/**
 * gcm_clut_init:
 **/
static void
gcm_clut_init (GcmClut *clut)
{
	clut->priv = GCM_CLUT_GET_PRIVATE (clut);
	clut->priv->array = g_ptr_array_new_with_free_func (g_free);
}

/**
 * gcm_clut_finalize:
 **/
static void
gcm_clut_finalize (GObject *object)
{
	GcmClut *clut = GCM_CLUT (object);
	GcmClutPrivate *priv = clut->priv;

	g_ptr_array_unref (priv->array);

	G_OBJECT_CLASS (gcm_clut_parent_class)->finalize (object);
}

/**
 * gcm_clut_new:
 *
 * Return value: a new GcmClut object.
 **/
GcmClut *
gcm_clut_new (void)
{
	GcmClut *clut;
	clut = g_object_new (GCM_TYPE_CLUT, NULL);
	return GCM_CLUT (clut);
}


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

/**
 * SECTION:gcm-clut
 * @short_description: Color lookup table object
 *
 * This object represents a color lookup table that is useful to manipulating
 * gamma values in a trivial RGB color space.
 */

#include "config.h"

#include <glib-object.h>
#include <math.h>
#include <gio/gio.h>

#include "gcm-clut.h"
#include "gcm-utils.h"

#include "egg-debug.h"

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
	gdouble				 gamma;
	gdouble				 brightness;
	gdouble				 contrast;
	GSettings			*settings;
};

enum {
	PROP_0,
	PROP_SIZE,
	PROP_ID,
	PROP_GAMMA,
	PROP_BRIGHTNESS,
	PROP_CONTRAST,
	PROP_COPYRIGHT,
	PROP_DESCRIPTION,
	PROP_LAST
};

G_DEFINE_TYPE (GcmClut, gcm_clut, G_TYPE_OBJECT)

#if 0
/**
 * gcm_clut_set_source_data:
 **/
static gboolean
gcm_clut_set_source_data (GcmClut *clut, const GcmClutData *data, guint size)
{
	guint i;
	GcmClutData *tmp;

	g_return_val_if_fail (GCM_IS_CLUT (clut), FALSE);
	g_return_val_if_fail (data != NULL, FALSE);

	/* copy each element into the array */
	for (i=0; i<size; i++) {
		tmp = g_new0 (GcmClutData, 1);
		tmp->red = data[i].red;
		tmp->green = data[i].green;
		tmp->blue = data[i].blue;
		g_ptr_array_add (clut->priv->array, tmp);
	}

	return TRUE;
}
#endif

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
 * gcm_clut_get_adjusted_value:
 **/
static guint
gcm_clut_get_adjusted_value (guint value, gdouble min, gdouble max, gdouble custom_gamma)
{
	guint retval;

	/* optimise for the common case */
	if (min < 0.01f && max > 0.99f && custom_gamma > 0.99 && custom_gamma < 1.01)
		return value;
	retval = 65536.0f * ((powf (((gdouble)value/65536.0f), custom_gamma) * (max - min)) + min);
	return retval;
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
	gdouble min;
	gdouble max;
	gdouble custom_gamma;

	g_return_val_if_fail (GCM_IS_CLUT (clut), FALSE);
	g_return_val_if_fail (clut->priv->gamma != 0, FALSE);

	min = clut->priv->brightness / 100.0f;
	max = (1.0f - min) * (clut->priv->contrast / 100.0f) + min;
	egg_debug ("min=%f,max=%f", min, max);
	custom_gamma = clut->priv->gamma;

	array = g_ptr_array_new_with_free_func (g_free);
	if (clut->priv->array->len == 0) {
		/* generate a dummy gamma */
		egg_debug ("falling back to dummy gamma");
		for (i=0; i<clut->priv->size; i++) {
			value = (i * 0xffff) / (clut->priv->size - 1);
			data = g_new0 (GcmClutData, 1);
			data->red = gcm_clut_get_adjusted_value (value, min, max, custom_gamma);
			data->green = gcm_clut_get_adjusted_value (value, min, max, custom_gamma);
			data->blue = gcm_clut_get_adjusted_value (value, min, max, custom_gamma);
			g_ptr_array_add (array, data);
		}
	} else {
		/* just make a 1:1 copy */
		for (i=0; i<clut->priv->size; i++) {
			tmp = g_ptr_array_index (clut->priv->array, i);
			data = g_new0 (GcmClutData, 1);
			data->red = gcm_clut_get_adjusted_value (tmp->red, min, max, custom_gamma);
			data->green = gcm_clut_get_adjusted_value (tmp->green, min, max, custom_gamma);
			data->blue = gcm_clut_get_adjusted_value (tmp->blue, min, max, custom_gamma);
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
	case PROP_GAMMA:
		g_value_set_double (value, priv->gamma);
		break;
	case PROP_BRIGHTNESS:
		g_value_set_double (value, priv->brightness);
		break;
	case PROP_CONTRAST:
		g_value_set_double (value, priv->contrast);
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
	case PROP_GAMMA:
		priv->gamma = g_value_get_double (value);
		break;
	case PROP_BRIGHTNESS:
		priv->brightness = g_value_get_double (value);
		break;
	case PROP_CONTRAST:
		priv->contrast = g_value_get_double (value);
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

	/**
	 * GcmClut:gamma:
	 */
	pspec = g_param_spec_double ("gamma", NULL, NULL,
				     0.0, G_MAXDOUBLE, 1.01,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_GAMMA, pspec);

	/**
	 * GcmClut:brightness:
	 */
	pspec = g_param_spec_double ("brightness", NULL, NULL,
				     0.0, G_MAXDOUBLE, 1.02,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_BRIGHTNESS, pspec);

	/**
	 * GcmClut:contrast:
	 */
	pspec = g_param_spec_double ("contrast", NULL, NULL,
				     0.0, G_MAXDOUBLE, 1.03,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_CONTRAST, pspec);

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
	clut->priv->settings = g_settings_new (GCM_SETTINGS_SCHEMA);
	clut->priv->gamma = g_settings_get_double (clut->priv->settings, GCM_SETTINGS_DEFAULT_GAMMA);
	if (clut->priv->gamma < 0.01)
		clut->priv->gamma = 1.0f;
	clut->priv->brightness = 0.0f;
	clut->priv->contrast = 100.f;
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
	g_object_unref (clut->priv->settings);

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


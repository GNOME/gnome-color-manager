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

#include <glib.h>
#include <colord.h>

#include "gcm-named-color.h"

static void	gcm_named_color_class_init	(GcmNamedColorClass	*klass);
static void	gcm_named_color_init		(GcmNamedColor	*named_color);
static void	gcm_named_color_finalize	(GObject	*object);

#define GCM_NAMED_COLOR_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GCM_TYPE_NAMED_COLOR, GcmNamedColorPrivate))

/**
 * GcmNamedColorPrivate:
 *
 * Private #GcmNamedColor data
 **/
struct _GcmNamedColorPrivate
{
	gchar			*title;
	CdColorXYZ		*value;
};

enum {
	PROP_0,
	PROP_TITLE,
	PROP_VALUE,
	PROP_LAST
};

G_DEFINE_TYPE (GcmNamedColor, gcm_named_color, G_TYPE_OBJECT)

/**
 * gcm_named_color_get_title:
 * @named_color: a #GcmNamedColor instance.
 *
 * Gets the named color title.
 *
 * Return value: A string, or %NULL for unset or invalid
 **/
const gchar *
gcm_named_color_get_title (GcmNamedColor *named_color)
{
	g_return_val_if_fail (GCM_IS_NAMED_COLOR (named_color), NULL);
	return named_color->priv->title;
}

/**
 * gcm_named_color_set_title:
 * @named_color: a #GcmNamedColor instance.
 * @title: the new title for this color
 *
 * Sets the named color title.
 **/
void
gcm_named_color_set_title (GcmNamedColor *named_color,
			   const gchar *title)
{
	g_return_if_fail (GCM_IS_NAMED_COLOR (named_color));
	g_free (named_color->priv->title);
	named_color->priv->title = g_strdup (title);
}

/**
 * gcm_named_color_get_value:
 * @named_color: a #GcmNamedColor instance.
 *
 * Gets the named color value.
 *
 * Return value: An XYZ value, or %NULL for unset or invalid
 **/
const CdColorXYZ *
gcm_named_color_get_value (GcmNamedColor *named_color)
{
	g_return_val_if_fail (GCM_IS_NAMED_COLOR (named_color), NULL);
	return named_color->priv->value;
}

/**
 * gcm_named_color_set_value:
 * @named_color: a #GcmNamedColor instance.
 * @value: the new value for this color
 *
 * Sets the named color color value.
 **/
void
gcm_named_color_set_value (GcmNamedColor *named_color,
			   const CdColorXYZ *value)
{
	g_return_if_fail (GCM_IS_NAMED_COLOR (named_color));
	cd_color_xyz_free (named_color->priv->value);
	named_color->priv->value = cd_color_xyz_dup (value);
}

/*
 * gcm_named_color_set_property:
 */
static void
gcm_named_color_set_property (GObject *object,
			      guint prop_id,
			      const GValue *value,
			      GParamSpec *pspec)
{
	GcmNamedColor *named_color = GCM_NAMED_COLOR (object);

	switch (prop_id) {
	case PROP_TITLE:
		gcm_named_color_set_title (named_color,
					   g_value_get_string (value));
		break;
	case PROP_VALUE:
		gcm_named_color_set_value (named_color,
					   g_value_get_boxed (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/*
 * gcm_named_color_get_property:
 */
static void
gcm_named_color_get_property (GObject *object,
			      guint prop_id,
			      GValue *value,
			      GParamSpec *pspec)
{
	GcmNamedColor *named_color = GCM_NAMED_COLOR (object);

	switch (prop_id) {
	case PROP_TITLE:
		g_value_set_string (value, named_color->priv->title);
		break;
	case PROP_VALUE:
		g_value_set_boxed (value, named_color->priv->value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
    }
}

/*
 * gcm_named_color_class_init:
 */
static void
gcm_named_color_class_init (GcmNamedColorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gcm_named_color_finalize;
	object_class->set_property = gcm_named_color_set_property;
	object_class->get_property = gcm_named_color_get_property;

	/**
	 * GcmNamedColor:title:
	 *
	 * The color title.
	 *
	 * Since: 0.1.6
	 **/
	g_object_class_install_property (object_class,
					 PROP_TITLE,
					 g_param_spec_string ("title",
							      NULL, NULL,
							      NULL,
							      G_PARAM_READWRITE));
	/**
	 * GcmNamedColor:value:
	 *
	 * The color value.
	 *
	 * Since: 0.1.6
	 **/
	g_object_class_install_property (object_class,
					 PROP_VALUE,
					 g_param_spec_boxed ("value",
							     NULL, NULL,
							     CD_TYPE_COLOR_XYZ,
							     G_PARAM_READWRITE));

	g_type_class_add_private (klass, sizeof (GcmNamedColorPrivate));
}

/*
 * gcm_named_color_init:
 */
static void
gcm_named_color_init (GcmNamedColor *named_color)
{
	named_color->priv = GCM_NAMED_COLOR_GET_PRIVATE (named_color);
}

/*
 * gcm_named_color_finalize:
 */
static void
gcm_named_color_finalize (GObject *object)
{
	GcmNamedColor *named_color;

	g_return_if_fail (GCM_IS_NAMED_COLOR (object));

	named_color = GCM_NAMED_COLOR (object);

	g_free (named_color->priv->title);
	cd_color_xyz_free (named_color->priv->value);

	G_OBJECT_CLASS (gcm_named_color_parent_class)->finalize (object);
}

/**
 * gcm_named_color_new:
 *
 * Creates a new #GcmNamedColor object.
 *
 * Return value: a new GcmNamedColor object.
 **/
GcmNamedColor *
gcm_named_color_new (void)
{
	GcmNamedColor *named_color;
	named_color = g_object_new (GCM_TYPE_NAMED_COLOR, NULL);
	return GCM_NAMED_COLOR (named_color);
}

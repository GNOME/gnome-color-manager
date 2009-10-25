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
 * GNU General Public License for more clut.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:gcm-clut
 * @short_description: Colour lookup table object
 *
 * This object represents a colour lookup table that is useful to manipulating
 * gamma values in a trivial RGB colour space.
 */

#include "config.h"

#include <glib-object.h>
#include <math.h>

#include "gcm-clut.h"
#include "gcm-profile.h"

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
	gfloat				 red;
	gfloat				 green;
	gfloat				 blue;
	gchar				*profile;
};

enum {
	PROP_0,
	PROP_SIZE,
	PROP_RED,
	PROP_GREEN,
	PROP_BLUE,
	PROP_LAST
};

G_DEFINE_TYPE (GcmClut, gcm_clut, G_TYPE_OBJECT)

/**
 * gcm_clut_set_from_data:
 **/
gboolean
gcm_clut_set_from_data (GcmClut *clut, const GcmClutData *data, guint size)
{
	guint i;
	GcmClutData *tmp;

	g_return_val_if_fail (GCM_IS_CLUT (clut), FALSE);

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

/**
 * gcm_clut_set_from_filename:
 **/
gboolean
gcm_clut_set_from_filename (GcmClut *clut, const gchar *filename, GError **error)
{
	gboolean ret;
	GcmProfile *profile;
	GcmClutData *data = NULL;

	g_return_val_if_fail (GCM_IS_CLUT (clut), FALSE);

	/* save profile name */
	g_free (clut->priv->profile);
	clut->priv->profile = g_strdup (filename);

	/* create new profile instance */
	profile = gcm_profile_new ();

	/* load the profile */
	ret = gcm_profile_load (profile, filename, error);
	if (!ret)
		goto out;

	/* generate the data */
	data = gcm_profile_generate (profile, clut->priv->size);
	gcm_clut_set_from_data (clut, data, clut->priv->size);
out:
	g_object_unref (profile);
	g_free (data);
	return ret;
}

/**
 * gcm_clut_load_from_config:
 **/
gboolean
gcm_clut_load_from_config (GcmClut *clut, const gchar *filename, const gchar *id, GError **error)
{
	gboolean ret;
	GKeyFile *file;

	g_return_val_if_fail (GCM_IS_CLUT (clut), FALSE);

	/* load existing file */
	file = g_key_file_new ();
	ret = g_key_file_load_from_file (file, filename, G_KEY_FILE_NONE, error);
	if (!ret)
		goto out;

	/* load data */
	g_free (clut->priv->profile);
	clut->priv->profile = g_strdup (g_key_file_get_string (file, id, "profile", NULL));
	clut->priv->red = g_key_file_get_double (file, id, "red", NULL);
	clut->priv->green = g_key_file_get_double (file, id, "green", NULL);
	clut->priv->blue = g_key_file_get_double (file, id, "blue", NULL);
out:
	g_key_file_free (file);
	return ret;
}

/**
 * gcm_clut_save_to_config:
 **/
gboolean
gcm_clut_save_to_config (GcmClut *clut, const gchar *filename, const gchar *id, GError **error)
{
	GKeyFile *file = NULL;
	gboolean ret;
	gchar *data;

	g_return_val_if_fail (GCM_IS_CLUT (clut), FALSE);

	/* if not ever created, then just create a dummy file */
	ret = g_file_test (filename, G_FILE_TEST_EXISTS);
	if (!ret) {
		ret = g_file_set_contents (filename, "#created today", -1, error);
		if (!ret)
			goto out;
	}

	/* load existing file */
	file = g_key_file_new ();
	ret = g_key_file_load_from_file (file, filename, G_KEY_FILE_KEEP_COMMENTS, error);
	if (!ret)
		goto out;

	/* save data */
	g_key_file_set_string (file, id, "profile", clut->priv->profile);
	g_key_file_set_double (file, id, "red", clut->priv->red);
	g_key_file_set_double (file, id, "green", clut->priv->green);
	g_key_file_set_double (file, id, "blue", clut->priv->blue);

	/* save contents */
	data = g_key_file_to_data (file, NULL, error);
	if (data == NULL)
		goto out;
	ret = g_file_set_contents (filename, data, -1, error);
	if (!ret)
		goto out;
out:
	if (file != NULL)
		g_key_file_free (file);
	return ret;
}

/**
 * gcm_clut_get_array:
 **/
GPtrArray *
gcm_clut_get_array (GcmClut *clut)
{
	g_return_val_if_fail (GCM_IS_CLUT (clut), FALSE);
	return g_ptr_array_ref (clut->priv->array);
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

/**	PROP_RED,

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
	case PROP_RED:
		g_value_set_float (value, priv->red);
		break;
	case PROP_GREEN:
		g_value_set_float (value, priv->green);
		break;
	case PROP_BLUE:
		g_value_set_float (value, priv->blue);
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
	case PROP_RED:
		priv->red = g_value_get_float (value);
		break;
	case PROP_GREEN:
		priv->green = g_value_get_float (value);
		break;
	case PROP_BLUE:
		priv->blue = g_value_get_float (value);
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
	 * GcmClut:red:
	 */
	pspec = g_param_spec_float ("red", NULL, NULL,
				    0.0, G_MAXFLOAT, 1.0,
				    G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_RED, pspec);

	/**
	 * GcmClut:green:
	 */
	pspec = g_param_spec_float ("green", NULL, NULL,
				    0.0, G_MAXFLOAT, 1.0,
				    G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_GREEN, pspec);

	/**
	 * GcmClut:blue:
	 */
	pspec = g_param_spec_float ("blue", NULL, NULL,
				    0.0, G_MAXFLOAT, 1.0,
				    G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_BLUE, pspec);

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
	clut->priv->profile = NULL;
	clut->priv->red = 1.0;
	clut->priv->green = 1.0;
	clut->priv->blue = 1.0;
}

/**
 * gcm_clut_finalize:
 **/
static void
gcm_clut_finalize (GObject *object)
{
	GcmClut *clut = GCM_CLUT (object);
	GcmClutPrivate *priv = clut->priv;

	g_free (clut->priv->profile);
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


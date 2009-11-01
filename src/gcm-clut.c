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
#include <gio/gio.h>
#include <gconf/gconf-client.h>

#include "gcm-clut.h"
#include "gcm-profile.h"

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
	gfloat				 gamma;
	gfloat				 brightness;
	gfloat				 contrast;
	gchar				*id;
	gchar				*profile;
	gchar				*description;
	gchar				*copyright;
	GConfClient			*gconf_client;
};

enum {
	PROP_0,
	PROP_SIZE,
	PROP_ID,
	PROP_GAMMA,
	PROP_BRIGHTNESS,
	PROP_CONTRAST,
	PROP_PROFILE,
	PROP_COPYRIGHT,
	PROP_DESCRIPTION,
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

/**
 * gcm_clut_load_from_profile:
 **/
gboolean
gcm_clut_load_from_profile (GcmClut *clut, GError **error)
{
	gboolean ret = TRUE;
	GcmProfile *profile = NULL;
	GcmClutData *data = NULL;
	GError *error_local = NULL;

	g_return_val_if_fail (GCM_IS_CLUT (clut), FALSE);

	/* no profile to load */
	if (clut->priv->profile == NULL) {
		g_free (clut->priv->copyright);
		g_free (clut->priv->description);
		clut->priv->copyright = NULL;
		clut->priv->description = NULL;
		g_ptr_array_set_size (clut->priv->array, 0);
		goto out;
	}

	/* create new profile instance */
	profile = gcm_profile_new ();

	/* load the profile */
	ret = gcm_profile_load (profile, clut->priv->profile, &error_local);
	if (!ret) {
		if (error != NULL)
			*error = g_error_new (1, 0, "failed to set from profile: %s", error_local->message);
		g_error_free (error_local);
		goto out;
	}

	/* generate the data */
	data = gcm_profile_generate (profile, clut->priv->size);
	if (data != NULL)
		gcm_clut_set_from_data (clut, data, clut->priv->size);

	/* copy the description */
	g_free (clut->priv->copyright);
	g_free (clut->priv->description);
	g_object_get (profile,
		      "copyright", &clut->priv->copyright,
		      "description", &clut->priv->description,
		      NULL);
out:
	if (profile != NULL)
		g_object_unref (profile);
	g_free (data);
	return ret;
}

/**
 * gcm_clut_get_default_config_location:
 **/
static gchar *
gcm_clut_get_default_config_location (void)
{
	gchar *filename;

	/* create default path */
	filename = g_build_filename (g_get_user_config_dir (), "gnome-color-manager", "config.dat", NULL);

	return filename;
}

/**
 * gcm_clut_load_from_config:
 **/
gboolean
gcm_clut_load_from_config (GcmClut *clut, GError **error)
{
	gboolean ret;
	GKeyFile *file;
	GError *error_local = NULL;
	gchar *filename = NULL;

	g_return_val_if_fail (GCM_IS_CLUT (clut), FALSE);
	g_return_val_if_fail (clut->priv->id != NULL, FALSE);

	/* get default config */
	filename = gcm_clut_get_default_config_location ();

	/* load existing file */
	file = g_key_file_new ();
	ret = g_key_file_load_from_file (file, filename, G_KEY_FILE_NONE, &error_local);
	if (!ret) {
		if (error != NULL)
			*error = g_error_new (1, 0, "failed to load from file: %s", error_local->message);
		g_error_free (error_local);
		goto out;
	}

	/* load data */
	g_free (clut->priv->profile);
	clut->priv->profile = g_key_file_get_string (file, clut->priv->id, "profile", NULL);
	clut->priv->gamma = g_key_file_get_double (file, clut->priv->id, "gamma", &error_local);
	if (error_local != NULL) {
		clut->priv->gamma = gconf_client_get_float (clut->priv->gconf_client, "/apps/gnome-color-manager/default_gamma", NULL);
		if (clut->priv->gamma < 0.1f)
			clut->priv->gamma = 1.0f;
		g_clear_error (&error_local);
	}
	clut->priv->brightness = g_key_file_get_double (file, clut->priv->id, "brightness", &error_local);
	if (error_local != NULL) {
		clut->priv->brightness = 0.0f;
		g_clear_error (&error_local);
	}
	clut->priv->contrast = g_key_file_get_double (file, clut->priv->id, "contrast", &error_local);
	if (error_local != NULL) {
		clut->priv->contrast = 100.0f;
		g_clear_error (&error_local);
	}

	/* load this */
	ret = gcm_clut_load_from_profile (clut, &error_local);
	if (!ret) {
		if (error != NULL)
			*error = g_error_new (1, 0, "failed to load from config: %s", error_local->message);
		g_error_free (error_local);
		goto out;
	}
out:
	g_free (filename);
	g_key_file_free (file);
	return ret;
}

/**
 * gcm_clut_save_to_config:
 **/
gboolean
gcm_clut_save_to_config (GcmClut *clut, GError **error)
{
	GKeyFile *keyfile = NULL;
	gboolean ret;
	gchar *data;
	gchar *dirname;
	GFile *file = NULL;
	gchar *filename = NULL;

	g_return_val_if_fail (GCM_IS_CLUT (clut), FALSE);
	g_return_val_if_fail (clut->priv->id != NULL, FALSE);

	/* get default config */
	filename = gcm_clut_get_default_config_location ();

	/* directory exists? */
	dirname = g_path_get_dirname (filename);
	ret = g_file_test (dirname, G_FILE_TEST_IS_DIR);
	if (!ret) {
		file = g_file_new_for_path (dirname);
		ret = g_file_make_directory_with_parents (file, NULL, error);
		if (!ret)
			goto out;
	}

	/* if not ever created, then just create a dummy file */
	ret = g_file_test (filename, G_FILE_TEST_EXISTS);
	if (!ret) {
		ret = g_file_set_contents (filename, "#created today", -1, error);
		if (!ret)
			goto out;
	}

	/* load existing file */
	keyfile = g_key_file_new ();
	ret = g_key_file_load_from_file (keyfile, filename, G_KEY_FILE_NONE, error);
	if (!ret)
		goto out;

	/* save data */
	if (clut->priv->profile == NULL)
		g_key_file_remove_key (keyfile, clut->priv->id, "profile", NULL);
	else
		g_key_file_set_string (keyfile, clut->priv->id, "profile", clut->priv->profile);
	g_key_file_set_double (keyfile, clut->priv->id, "gamma", clut->priv->gamma);
	g_key_file_set_double (keyfile, clut->priv->id, "brightness", clut->priv->brightness);
	g_key_file_set_double (keyfile, clut->priv->id, "contrast", clut->priv->contrast);

	/* save contents */
	data = g_key_file_to_data (keyfile, NULL, error);
	if (data == NULL)
		goto out;
	ret = g_file_set_contents (filename, data, -1, error);
	if (!ret)
		goto out;
out:
	g_free (filename);
	g_free (dirname);
	if (file != NULL)
		g_object_unref (file);
	if (keyfile != NULL)
		g_key_file_free (keyfile);
	return ret;
}

static guint
gcm_clut_get_adjusted_value (guint value, gfloat min, gfloat max, gfloat gamma)
{
	guint retval;
	retval = 65536.0f * ((powf (((gfloat)value/65536.0f), gamma) * (max - min)) + min);
	return retval;
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
	gfloat min;
	gfloat max;
	gfloat gamma;

	g_return_val_if_fail (GCM_IS_CLUT (clut), FALSE);
	g_return_val_if_fail (clut->priv->size != 0, FALSE);
	g_return_val_if_fail (clut->priv->gamma != 0, FALSE);

	min = clut->priv->brightness / 100.0f;
	max = (1.0f - min) * (clut->priv->contrast / 100.0f) + min;
	gamma = clut->priv->gamma;

	array = g_ptr_array_new_with_free_func (g_free);
	if (clut->priv->array->len == 0) {
		/* generate a dummy gamma */
		egg_debug ("falling back to dummy gamma");
		for (i=0; i<clut->priv->size; i++) {
			value = (i * 0xffff) / clut->priv->size;
			data = g_new0 (GcmClutData, 1);
			data->red = gcm_clut_get_adjusted_value (value, min, max, gamma);
			data->green = gcm_clut_get_adjusted_value (value, min, max, gamma);
			data->blue = gcm_clut_get_adjusted_value (value, min, max, gamma);
			g_ptr_array_add (array, data);
		}
	} else {
		/* just make a 1:1 copy */
		for (i=0; i<clut->priv->size; i++) {
			tmp = g_ptr_array_index (clut->priv->array, i);
			data = g_new0 (GcmClutData, 1);
			data->red = gcm_clut_get_adjusted_value (tmp->red, min, max, gamma);
			data->green = gcm_clut_get_adjusted_value (tmp->green, min, max, gamma);
			data->blue = gcm_clut_get_adjusted_value (tmp->blue, min, max, gamma);
			g_ptr_array_add (array, data);
		}
	}

out:
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
	case PROP_ID:
		g_value_set_string (value, priv->id);
		break;
	case PROP_GAMMA:
		g_value_set_float (value, priv->gamma);
		break;
	case PROP_BRIGHTNESS:
		g_value_set_float (value, priv->brightness);
		break;
	case PROP_CONTRAST:
		g_value_set_float (value, priv->contrast);
		break;
	case PROP_PROFILE:
		g_value_set_string (value, priv->profile);
		break;
	case PROP_COPYRIGHT:
		g_value_set_string (value, priv->copyright);
		break;
	case PROP_DESCRIPTION:
		g_value_set_string (value, priv->description);
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
	case PROP_ID:
		g_free (priv->id);
		priv->id = g_strdup (g_value_get_string (value));
		break;
	case PROP_PROFILE:
		g_free (priv->profile);
		priv->profile = g_strdup (g_value_get_string (value));
		break;
	case PROP_GAMMA:
		priv->gamma = g_value_get_float (value);
		break;
	case PROP_BRIGHTNESS:
		priv->brightness = g_value_get_float (value);
		break;
	case PROP_CONTRAST:
		priv->contrast = g_value_get_float (value);
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
	 * GcmClut:id:
	 */
	pspec = g_param_spec_string ("id", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_ID, pspec);

	/**
	 * GcmClut:gamma:
	 */
	pspec = g_param_spec_float ("gamma", NULL, NULL,
				    0.0, G_MAXFLOAT, 1.01,
				    G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_GAMMA, pspec);

	/**
	 * GcmClut:brightness:
	 */
	pspec = g_param_spec_float ("brightness", NULL, NULL,
				    0.0, G_MAXFLOAT, 1.02,
				    G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_BRIGHTNESS, pspec);

	/**
	 * GcmClut:contrast:
	 */
	pspec = g_param_spec_float ("contrast", NULL, NULL,
				    0.0, G_MAXFLOAT, 1.03,
				    G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_CONTRAST, pspec);

	/**
	 * GcmClut:copyright:
	 */
	pspec = g_param_spec_string ("copyright", NULL, NULL,
				     NULL,
				     G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_COPYRIGHT, pspec);

	/**
	 * GcmClut:description:
	 */
	pspec = g_param_spec_string ("description", NULL, NULL,
				     NULL,
				     G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_DESCRIPTION, pspec);

	/**
	 * GcmClut:profile:
	 */
	pspec = g_param_spec_string ("profile", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_PROFILE, pspec);

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
	clut->priv->gconf_client = gconf_client_get_default ();
	clut->priv->gamma = gconf_client_get_float (clut->priv->gconf_client, "/apps/gnome-color-manager/default_gamma", NULL);
	if (clut->priv->gamma < 0.1f) {
		egg_warning ("failed to get setup parameters");
		clut->priv->gamma = 1.0f;
	}
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

	g_free (clut->priv->profile);
	g_free (clut->priv->id);
	g_ptr_array_unref (priv->array);
	g_object_unref (clut->priv->gconf_client);

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


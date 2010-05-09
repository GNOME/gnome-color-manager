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
 * SECTION:gcm-profile
 * @short_description: A parser object that understands the ICC profile data format.
 *
 * This object is a simple parser for the ICC binary profile data. If only understands
 * a subset of the ICC profile, just enought to get some metadata and the LUT.
 */

#include "config.h"

#include <glib-object.h>
#include <glib/gi18n.h>
#include <gio/gio.h>

#include "egg-debug.h"

#include "gcm-profile.h"
#include "gcm-utils.h"
#include "gcm-xyz.h"
#include "gcm-profile-lcms1.h"

static void     gcm_profile_finalize	(GObject     *object);

#define GCM_PROFILE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GCM_TYPE_PROFILE, GcmProfilePrivate))

/**
 * GcmProfilePrivate:
 *
 * Private #GcmProfile data
 **/
struct _GcmProfilePrivate
{
	GcmProfileKind		 kind;
	GcmColorspace		 colorspace;
	guint			 size;
	gboolean		 has_vcgt;
	gchar			*description;
	gchar			*filename;
	gchar			*copyright;
	gchar			*manufacturer;
	gchar			*model;
	gchar			*datetime;
	GcmXyz			*white;
	GcmXyz			*black;
	GcmXyz			*red;
	GcmXyz			*green;
	GcmXyz			*blue;
	GFileMonitor		*monitor;
};

enum {
	PROP_0,
	PROP_COPYRIGHT,
	PROP_MANUFACTURER,
	PROP_MODEL,
	PROP_DATETIME,
	PROP_DESCRIPTION,
	PROP_FILENAME,
	PROP_KIND,
	PROP_COLORSPACE,
	PROP_SIZE,
	PROP_HAS_VCGT,
	PROP_WHITE,
	PROP_BLACK,
	PROP_RED,
	PROP_GREEN,
	PROP_BLUE,
	PROP_LAST
};

G_DEFINE_TYPE (GcmProfile, gcm_profile, G_TYPE_OBJECT)

static void gcm_profile_file_monitor_changed_cb (GFileMonitor *monitor, GFile *file, GFile *other_file, GFileMonitorEvent event_type, GcmProfile *profile);

/**
 * gcm_profile_get_description:
 **/
const gchar *
gcm_profile_get_description (GcmProfile *profile)
{
	g_return_val_if_fail (GCM_IS_PROFILE (profile), NULL);
	return profile->priv->description;
}

/**
 * gcm_profile_set_description:
 **/
void
gcm_profile_set_description (GcmProfile *profile, const gchar *description)
{
	GcmProfilePrivate *priv = profile->priv;
	g_return_if_fail (GCM_IS_PROFILE (profile));

	g_free (priv->description);
	priv->description = g_strdup (description);

	if (priv->description != NULL)
		gcm_utils_ensure_printable (priv->description);

	/* there's nothing sensible to display */
	if (priv->description == NULL || priv->description[0] == '\0') {
		g_free (priv->description);
		if (priv->filename != NULL) {
			priv->description = g_path_get_basename (priv->filename);
		} else {
			/* TRANSLATORS: this is where the ICC profile_lcms1 has no description */
			priv->description = g_strdup (_("Missing description"));
		}
	}
	g_object_notify (G_OBJECT (profile), "description");
}


/**
 * gcm_profile_get_filename:
 **/
const gchar *
gcm_profile_get_filename (GcmProfile *profile)
{
	g_return_val_if_fail (GCM_IS_PROFILE (profile), NULL);
	return profile->priv->filename;
}

/**
 * gcm_profile_has_colorspace_description:
 *
 * Return value: if the description mentions the profile colorspace explicity,
 * e.g. "Adobe RGB" for %GCM_COLORSPACE_RGB.
 **/
gboolean
gcm_profile_has_colorspace_description (GcmProfile *profile)
{
	GcmProfilePrivate *priv = profile->priv;
	g_return_val_if_fail (GCM_IS_PROFILE (profile), FALSE);

	/* for each profile type */
	if (priv->colorspace == GCM_COLORSPACE_RGB)
		return (g_strstr_len (priv->description, -1, "RGB") != NULL);
	if (priv->colorspace == GCM_COLORSPACE_CMYK)
		return (g_strstr_len (priv->description, -1, "CMYK") != NULL);

	/* nothing */
	return FALSE;
}

/**
 * gcm_profile_set_filename:
 **/
void
gcm_profile_set_filename (GcmProfile *profile, const gchar *filename)
{
	GcmProfilePrivate *priv = profile->priv;
	GFile *file;

	g_return_if_fail (GCM_IS_PROFILE (profile));

	g_free (priv->filename);
	priv->filename = g_strdup (filename);

	/* unref old instance */
	if (priv->monitor != NULL) {
		g_object_unref (priv->monitor);
		priv->monitor = NULL;
	}

	/* setup watch on new profile */
	if (priv->filename != NULL) {
		file = g_file_new_for_path (priv->filename);
		priv->monitor = g_file_monitor_file (file, G_FILE_MONITOR_NONE, NULL, NULL);
		if (priv->monitor != NULL)
			g_signal_connect (priv->monitor, "changed", G_CALLBACK(gcm_profile_file_monitor_changed_cb), profile);
		g_object_unref (file);
	}
	g_object_notify (G_OBJECT (profile), "filename");
}


/**
 * gcm_profile_get_copyright:
 **/
const gchar *
gcm_profile_get_copyright (GcmProfile *profile)
{
	g_return_val_if_fail (GCM_IS_PROFILE (profile), NULL);
	return profile->priv->copyright;
}

/**
 * gcm_profile_set_copyright:
 **/
void
gcm_profile_set_copyright (GcmProfile *profile, const gchar *copyright)
{
	GcmProfilePrivate *priv = profile->priv;

	g_return_if_fail (GCM_IS_PROFILE (profile));

	g_free (priv->copyright);
	priv->copyright = g_strdup (copyright);
	if (priv->copyright != NULL)
		gcm_utils_ensure_printable (priv->copyright);
	g_object_notify (G_OBJECT (profile), "copyright");
}


/**
 * gcm_profile_get_model:
 **/
const gchar *
gcm_profile_get_model (GcmProfile *profile)
{
	g_return_val_if_fail (GCM_IS_PROFILE (profile), NULL);
	return profile->priv->model;
}

/**
 * gcm_profile_set_model:
 **/
void
gcm_profile_set_model (GcmProfile *profile, const gchar *model)
{
	GcmProfilePrivate *priv = profile->priv;

	g_return_if_fail (GCM_IS_PROFILE (profile));

	g_free (priv->model);
	priv->model = g_strdup (model);
	if (priv->model != NULL)
		gcm_utils_ensure_printable (priv->model);
	g_object_notify (G_OBJECT (profile), "model");
}

/**
 * gcm_profile_get_manufacturer:
 **/
const gchar *
gcm_profile_get_manufacturer (GcmProfile *profile)
{
	g_return_val_if_fail (GCM_IS_PROFILE (profile), NULL);
	return profile->priv->manufacturer;
}

/**
 * gcm_profile_set_manufacturer:
 **/
void
gcm_profile_set_manufacturer (GcmProfile *profile, const gchar *manufacturer)
{
	GcmProfilePrivate *priv = profile->priv;

	g_return_if_fail (GCM_IS_PROFILE (profile));

	g_free (priv->manufacturer);
	priv->manufacturer = g_strdup (manufacturer);
	if (priv->manufacturer != NULL)
		gcm_utils_ensure_printable (priv->manufacturer);
	g_object_notify (G_OBJECT (profile), "manufacturer");
}


/**
 * gcm_profile_get_datetime:
 **/
const gchar *
gcm_profile_get_datetime (GcmProfile *profile)
{
	g_return_val_if_fail (GCM_IS_PROFILE (profile), NULL);
	return profile->priv->datetime;
}

/**
 * gcm_profile_set_datetime:
 **/
void
gcm_profile_set_datetime (GcmProfile *profile, const gchar *datetime)
{
	GcmProfilePrivate *priv = profile->priv;

	g_return_if_fail (GCM_IS_PROFILE (profile));

	g_free (priv->datetime);
	priv->datetime = g_strdup (datetime);
	g_object_notify (G_OBJECT (profile), "datetime");
}


/**
 * gcm_profile_get_size:
 **/
guint
gcm_profile_get_size (GcmProfile *profile)
{
	g_return_val_if_fail (GCM_IS_PROFILE (profile), 0);
	return profile->priv->size;
}

/**
 * gcm_profile_set_size:
 **/
void
gcm_profile_set_size (GcmProfile *profile, guint size)
{
	g_return_if_fail (GCM_IS_PROFILE (profile));
	profile->priv->size = size;
	g_object_notify (G_OBJECT (profile), "size");
}


/**
 * gcm_profile_get_kind:
 **/
GcmProfileKind
gcm_profile_get_kind (GcmProfile *profile)
{
	g_return_val_if_fail (GCM_IS_PROFILE (profile), GCM_PROFILE_KIND_UNKNOWN);
	return profile->priv->kind;
}

/**
 * gcm_profile_set_kind:
 **/
void
gcm_profile_set_kind (GcmProfile *profile, GcmProfileKind kind)
{
	g_return_if_fail (GCM_IS_PROFILE (profile));
	profile->priv->kind = kind;
	g_object_notify (G_OBJECT (profile), "kind");
}


/**
 * gcm_profile_get_colorspace:
 **/
GcmColorspace
gcm_profile_get_colorspace (GcmProfile *profile)
{
	g_return_val_if_fail (GCM_IS_PROFILE (profile), GCM_COLORSPACE_UNKNOWN);
	return profile->priv->colorspace;
}

/**
 * gcm_profile_set_colorspace:
 **/
void
gcm_profile_set_colorspace (GcmProfile *profile, GcmColorspace colorspace)
{
	g_return_if_fail (GCM_IS_PROFILE (profile));
	profile->priv->colorspace = colorspace;
	g_object_notify (G_OBJECT (profile), "colorspace");
}


/**
 * gcm_profile_get_has_vcgt:
 **/
gboolean
gcm_profile_get_has_vcgt (GcmProfile *profile)
{
	g_return_val_if_fail (GCM_IS_PROFILE (profile), FALSE);
	return profile->priv->has_vcgt;
}

/**
 * gcm_profile_set_has_vcgt:
 **/
void
gcm_profile_set_has_vcgt (GcmProfile *profile, gboolean has_vcgt)
{
	g_return_if_fail (GCM_IS_PROFILE (profile));
	profile->priv->has_vcgt = has_vcgt;
	g_object_notify (G_OBJECT (profile), "has_vcgt");
}

/**
 * gcm_profile_parse_data:
 **/
gboolean
gcm_profile_parse_data (GcmProfile *profile, const guint8 *data, gsize length, GError **error)
{
	gboolean ret = FALSE;
	GcmProfilePrivate *priv = profile->priv;
	GcmProfileClass *klass = GCM_PROFILE_GET_CLASS (profile);

	/* save the length */
	priv->size = length;

	/* do we have support */
	if (klass->parse_data == NULL) {
		g_set_error_literal (error, 1, 0, "no support");
		goto out;
	}

	/* proxy */
	ret = klass->parse_data (profile, data, length, error);
out:
	return ret;
}

/**
 * gcm_profile_parse:
 **/
gboolean
gcm_profile_parse (GcmProfile *profile, GFile *file, GError **error)
{
	gchar *data = NULL;
	gboolean ret;
	gsize length;
	gchar *filename = NULL;
	GError *error_local = NULL;

	g_return_val_if_fail (GCM_IS_PROFILE (profile), FALSE);
	g_return_val_if_fail (file != NULL, FALSE);

	/* load files */
	ret = g_file_load_contents (file, NULL, &data, &length, NULL, &error_local);
	if (!ret) {
		g_set_error (error, 1, 0, "failed to load profile: %s", error_local->message);
		g_error_free (error_local);
		goto out;
	}

	/* parse the data */
	ret = gcm_profile_parse_data (profile, (const guint8*)data, length, error);
	if (!ret)
		goto out;

	/* save */
	filename = g_file_get_path (file);
	gcm_profile_set_filename (profile, filename);
out:
	g_free (filename);
	g_free (data);
	return ret;
}

/**
 * gcm_profile_save:
 **/
gboolean
gcm_profile_save (GcmProfile *profile, const gchar *filename, GError **error)
{
	gboolean ret = FALSE;
	GcmProfilePrivate *priv = profile->priv;
	GcmProfileClass *klass = GCM_PROFILE_GET_CLASS (profile);

	/* not loaded */
	if (priv->size == 0) {
		g_set_error_literal (error, 1, 0, "not loaded");
		goto out;
	}

	/* do we have support */
	if (klass->save == NULL) {
		g_set_error_literal (error, 1, 0, "no support");
		goto out;
	}

	/* proxy */
	ret = klass->save (profile, filename, error);
out:
	return ret;
}

/**
 * gcm_profile_generate_vcgt:
 *
 * Free with g_object_unref();
 **/
GcmClut *
gcm_profile_generate_vcgt (GcmProfile *profile, guint size)
{
	GcmClut *clut = NULL;
	GcmProfileClass *klass = GCM_PROFILE_GET_CLASS (profile);

	/* do we have support */
	if (klass->generate_vcgt == NULL)
		goto out;

	/* proxy */
	clut = klass->generate_vcgt (profile, size);
out:
	return clut;
}

/**
 * gcm_profile_generate_curve:
 *
 * Free with g_object_unref();
 **/
GcmClut *
gcm_profile_generate_curve (GcmProfile *profile, guint size)
{
	GcmClut *clut = NULL;
	GcmProfileClass *klass = GCM_PROFILE_GET_CLASS (profile);

	/* do we have support */
	if (klass->generate_curve == NULL)
		goto out;

	/* proxy */
	clut = klass->generate_curve (profile, size);
out:
	return clut;
}

/**
 * gcm_profile_file_monitor_changed_cb:
 **/
static void
gcm_profile_file_monitor_changed_cb (GFileMonitor *monitor, GFile *file, GFile *other_file, GFileMonitorEvent event_type, GcmProfile *profile)
{
	GcmProfilePrivate *priv = profile->priv;

	/* ony care about deleted events */
	if (event_type != G_FILE_MONITOR_EVENT_DELETED)
		goto out;

	/* just rescan everything */
	egg_debug ("%s deleted, clearing filename", priv->filename);
	gcm_profile_set_filename (profile, NULL);
out:
	return;
}

/**
 * gcm_profile_get_property:
 **/
static void
gcm_profile_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GcmProfile *profile = GCM_PROFILE (object);
	GcmProfilePrivate *priv = profile->priv;

	switch (prop_id) {
	case PROP_COPYRIGHT:
		g_value_set_string (value, priv->copyright);
		break;
	case PROP_MANUFACTURER:
		g_value_set_string (value, priv->manufacturer);
		break;
	case PROP_MODEL:
		g_value_set_string (value, priv->model);
		break;
	case PROP_DATETIME:
		g_value_set_string (value, priv->datetime);
		break;
	case PROP_DESCRIPTION:
		g_value_set_string (value, priv->description);
		break;
	case PROP_FILENAME:
		g_value_set_string (value, priv->filename);
		break;
	case PROP_KIND:
		g_value_set_uint (value, priv->kind);
		break;
	case PROP_COLORSPACE:
		g_value_set_uint (value, priv->colorspace);
		break;
	case PROP_SIZE:
		g_value_set_uint (value, priv->size);
		break;
	case PROP_HAS_VCGT:
		g_value_set_boolean (value, priv->has_vcgt);
		break;
	case PROP_WHITE:
		g_value_set_object (value, priv->white);
		break;
	case PROP_BLACK:
		g_value_set_object (value, priv->black);
		break;
	case PROP_RED:
		g_value_set_object (value, priv->red);
		break;
	case PROP_GREEN:
		g_value_set_object (value, priv->green);
		break;
	case PROP_BLUE:
		g_value_set_object (value, priv->blue);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gcm_profile_set_property:
 **/
static void
gcm_profile_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	GcmProfile *profile = GCM_PROFILE (object);
	GcmProfilePrivate *priv = profile->priv;

	switch (prop_id) {
	case PROP_COPYRIGHT:
		gcm_profile_set_copyright (profile, g_value_get_string (value));
		break;
	case PROP_MANUFACTURER:
		gcm_profile_set_manufacturer (profile, g_value_get_string (value));
		break;
	case PROP_MODEL:
		gcm_profile_set_model (profile, g_value_get_string (value));
		break;
	case PROP_DATETIME:
		gcm_profile_set_datetime (profile, g_value_get_string (value));
		break;
	case PROP_DESCRIPTION:
		gcm_profile_set_description (profile, g_value_get_string (value));
		break;
	case PROP_FILENAME:
		gcm_profile_set_filename (profile, g_value_get_string (value));
		break;
	case PROP_KIND:
		gcm_profile_set_kind (profile, g_value_get_uint (value));
		break;
	case PROP_COLORSPACE:
		gcm_profile_set_colorspace (profile, g_value_get_uint (value));
		break;
	case PROP_SIZE:
		gcm_profile_set_size (profile, g_value_get_uint (value));
		break;
	case PROP_HAS_VCGT:
		gcm_profile_set_has_vcgt (profile, g_value_get_boolean (value));
		break;
	case PROP_WHITE:
		priv->white = g_value_dup_object (value);
		break;
	case PROP_BLACK:
		priv->black = g_value_dup_object (value);
		break;
	case PROP_RED:
		priv->red = g_value_dup_object (value);
		break;
	case PROP_GREEN:
		priv->green = g_value_dup_object (value);
		break;
	case PROP_BLUE:
		priv->blue = g_value_dup_object (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gcm_profile_class_init:
 **/
static void
gcm_profile_class_init (GcmProfileClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gcm_profile_finalize;
	object_class->get_property = gcm_profile_get_property;
	object_class->set_property = gcm_profile_set_property;

	/**
	 * GcmProfile:copyright:
	 */
	pspec = g_param_spec_string ("copyright", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_COPYRIGHT, pspec);

	/**
	 * GcmProfile:manufacturer:
	 */
	pspec = g_param_spec_string ("manufacturer", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_MANUFACTURER, pspec);

	/**
	 * GcmProfile:model:
	 */
	pspec = g_param_spec_string ("model", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_MODEL, pspec);

	/**
	 * GcmProfile:datetime:
	 */
	pspec = g_param_spec_string ("datetime", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_DATETIME, pspec);

	/**
	 * GcmProfile:description:
	 */
	pspec = g_param_spec_string ("description", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_DESCRIPTION, pspec);

	/**
	 * GcmProfile:filename:
	 */
	pspec = g_param_spec_string ("filename", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_FILENAME, pspec);

	/**
	 * GcmProfile:kind:
	 */
	pspec = g_param_spec_uint ("kind", NULL, NULL,
				   0, G_MAXUINT, 0,
				   G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_KIND, pspec);

	/**
	 * GcmProfile:colorspace:
	 */
	pspec = g_param_spec_uint ("colorspace", NULL, NULL,
				   0, G_MAXUINT, 0,
				   G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_COLORSPACE, pspec);

	/**
	 * GcmProfile:size:
	 */
	pspec = g_param_spec_uint ("size", NULL, NULL,
				   0, G_MAXUINT, 0,
				   G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_SIZE, pspec);

	/**
	 * GcmProfile:has-vcgt:
	 */
	pspec = g_param_spec_boolean ("has-vcgt", NULL, NULL,
				      FALSE,
				      G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_HAS_VCGT, pspec);

	/**
	 * GcmProfile:white:
	 */
	pspec = g_param_spec_object ("white", NULL, NULL,
				     GCM_TYPE_XYZ,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_WHITE, pspec);

	/**
	 * GcmProfile:black:
	 */
	pspec = g_param_spec_object ("black", NULL, NULL,
				     GCM_TYPE_XYZ,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_BLACK, pspec);

	/**
	 * GcmProfile:red:
	 */
	pspec = g_param_spec_object ("red", NULL, NULL,
				     GCM_TYPE_XYZ,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_RED, pspec);

	/**
	 * GcmProfile:green:
	 */
	pspec = g_param_spec_object ("green", NULL, NULL,
				     GCM_TYPE_XYZ,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_GREEN, pspec);

	/**
	 * GcmProfile:blue:
	 */
	pspec = g_param_spec_object ("blue", NULL, NULL,
				     GCM_TYPE_XYZ,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_BLUE, pspec);

	g_type_class_add_private (klass, sizeof (GcmProfilePrivate));
}

/**
 * gcm_profile_init:
 **/
static void
gcm_profile_init (GcmProfile *profile)
{
	profile->priv = GCM_PROFILE_GET_PRIVATE (profile);
	profile->priv->monitor = NULL;
	profile->priv->kind = GCM_PROFILE_KIND_UNKNOWN;
	profile->priv->colorspace = GCM_COLORSPACE_UNKNOWN;
	profile->priv->white = gcm_xyz_new ();
	profile->priv->black = gcm_xyz_new ();
	profile->priv->red = gcm_xyz_new ();
	profile->priv->green = gcm_xyz_new ();
	profile->priv->blue = gcm_xyz_new ();
}

/**
 * gcm_profile_finalize:
 **/
static void
gcm_profile_finalize (GObject *object)
{
	GcmProfile *profile = GCM_PROFILE (object);
	GcmProfilePrivate *priv = profile->priv;

	g_free (priv->copyright);
	g_free (priv->description);
	g_free (priv->filename);
	g_free (priv->manufacturer);
	g_free (priv->model);
	g_free (priv->datetime);
	g_object_unref (priv->white);
	g_object_unref (priv->black);
	g_object_unref (priv->red);
	g_object_unref (priv->green);
	g_object_unref (priv->blue);
	if (priv->monitor != NULL)
		g_object_unref (priv->monitor);

	G_OBJECT_CLASS (gcm_profile_parent_class)->finalize (object);
}

/**
 * gcm_profile_new:
 *
 * Return value: a new GcmProfile object.
 **/
GcmProfile *
gcm_profile_new (void)
{
	GcmProfile *profile;
	profile = g_object_new (GCM_TYPE_PROFILE, NULL);
	return GCM_PROFILE (profile);
}

/**
 * gcm_profile_default_new:
 *
 * Return value: a new GcmProfile object.
 **/
GcmProfile *
gcm_profile_default_new (void)
{
	GcmProfile *profile = NULL;
	profile = GCM_PROFILE (gcm_profile_lcms1_new ());
	return profile;
}


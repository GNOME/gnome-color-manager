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
	GcmProfileTypeEnum		 profile_type;
	GcmColorspaceEnum		 colorspace;
	guint				 size;
	gboolean			 has_vcgt;
	gchar				*description;
	gchar				*filename;
	GFileMonitor			*monitor;
	gchar				*copyright;
	gchar				*manufacturer;
	gchar				*model;
	gchar				*datetime;
	GcmXyz				*white;
	GcmXyz				*black;
	GcmXyz				*red;
	GcmXyz				*green;
	GcmXyz				*blue;
};

enum {
	PROP_0,
	PROP_COPYRIGHT,
	PROP_MANUFACTURER,
	PROP_MODEL,
	PROP_DATETIME,
	PROP_DESCRIPTION,
	PROP_FILENAME,
	PROP_TYPE,
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
	guint length;
	gchar *filename = NULL;
	GError *error_local = NULL;

	g_return_val_if_fail (GCM_IS_PROFILE (profile), FALSE);
	g_return_val_if_fail (file != NULL, FALSE);

	/* load files */
	ret = g_file_load_contents (file, NULL, &data, (gsize *) &length, NULL, &error_local);
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
	g_object_set (profile,
		      "filename", filename,
		      NULL);
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
	g_object_set (profile,
		      "filename", NULL,
		      NULL);
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
	case PROP_TYPE:
		g_value_set_uint (value, priv->profile_type);
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
	GFile *file;

	switch (prop_id) {
	case PROP_COPYRIGHT:
		g_free (priv->copyright);
		priv->copyright = g_strdup (g_value_get_string (value));
		if (priv->copyright != NULL)
			gcm_utils_ensure_printable (priv->copyright);
		break;
	case PROP_MANUFACTURER:
		g_free (priv->manufacturer);
		priv->manufacturer = g_strdup (g_value_get_string (value));
		if (priv->manufacturer != NULL)
			gcm_utils_ensure_printable (priv->manufacturer);
		break;
	case PROP_MODEL:
		g_free (priv->model);
		priv->model = g_strdup (g_value_get_string (value));
		if (priv->model != NULL)
			gcm_utils_ensure_printable (priv->model);
		break;
	case PROP_DATETIME:
		g_free (priv->datetime);
		priv->datetime = g_strdup (g_value_get_string (value));
		break;
	case PROP_DESCRIPTION:
		g_free (priv->description);
		priv->description = g_strdup (g_value_get_string (value));
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
		break;
	case PROP_FILENAME:
		g_free (priv->filename);
		priv->filename = g_strdup (g_value_get_string (value));

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
		break;
	case PROP_TYPE:
		priv->profile_type = g_value_get_uint (value);
		break;
	case PROP_COLORSPACE:
		priv->colorspace = g_value_get_uint (value);
		break;
	case PROP_SIZE:
		priv->size = g_value_get_uint (value);
		break;
	case PROP_HAS_VCGT:
		priv->has_vcgt = g_value_get_boolean (value);
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
	 * GcmProfile:type:
	 */
	pspec = g_param_spec_uint ("type", NULL, NULL,
				   0, G_MAXUINT, 0,
				   G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_TYPE, pspec);

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
	profile->priv->profile_type = GCM_PROFILE_TYPE_ENUM_UNKNOWN;
	profile->priv->colorspace = GCM_COLORSPACE_ENUM_UNKNOWN;
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
#ifdef GCM_HAVE_LCMS
	profile = GCM_PROFILE (gcm_profile_lcms1_new ());
#endif
	return profile;
}

/***************************************************************************
 ***                          MAKE CHECK TESTS                           ***
 ***************************************************************************/
#ifdef EGG_TEST
#include <math.h>
#include "egg-test.h"

typedef struct {
	const gchar *copyright;
	const gchar *manufacturer;
	const gchar *model;
	const gchar *datetime;
	const gchar *description;
	GcmProfileTypeEnum type;
	GcmColorspaceEnum colorspace;
	gfloat luminance;
} GcmProfileTestData;

void
gcm_profile_test_parse_file (EggTest *test, const guint8 *datafile, GcmProfileTestData *test_data)
{
	gchar *filename;
	gchar *filename_tmp;
	gchar *copyright;
	gchar *manufacturer;
	gchar *model;
	gchar *datetime;
	gchar *description;
	gchar *ascii_string;
	gchar *pnp_id;
	gchar *data;
	guint width;
	guint type;
	guint colorspace;
	gfloat gamma;
	gboolean ret;
	GError *error = NULL;
	GcmProfile *profile_lcms1;
	GcmXyz *xyz;
	gfloat luminance;
	GFile *file;

	/************************************************************/
	egg_test_title (test, "get a profile_lcms1 object");
	profile_lcms1 = GCM_PROFILE(gcm_profile_lcms1_new ());
	egg_test_assert (test, profile_lcms1 != NULL);

	/************************************************************/
	egg_test_title (test, "get filename of data file");
	filename = egg_test_get_data_file (datafile);
	egg_test_assert (test, (filename != NULL));

	/************************************************************/
	egg_test_title (test, "load ICC file");
	file = g_file_new_for_path (filename);
	ret = gcm_profile_parse (profile_lcms1, file, &error);
	if (ret)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "failed to parse: %s", error->message);
	g_object_unref (file);

	/* get some properties */
	g_object_get (profile_lcms1,
		      "copyright", &copyright,
		      "manufacturer", &manufacturer,
		      "model", &model,
		      "datetime", &datetime,
		      "description", &description,
		      "filename", &filename_tmp,
		      "type", &type,
		      "colorspace", &colorspace,
		      NULL);

	/************************************************************/
	egg_test_title (test, "check copyright for %s", datafile);
	if (g_strcmp0 (copyright, test_data->copyright) == 0)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "invalid value: %s, expecting: %s", copyright, test_data->copyright);

	/************************************************************/
	egg_test_title (test, "check manufacturer for %s", datafile);
	if (g_strcmp0 (manufacturer, test_data->manufacturer) == 0)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "invalid value: %s, expecting: %s", manufacturer, test_data->manufacturer);

	/************************************************************/
	egg_test_title (test, "check model for %s", datafile);
	if (g_strcmp0 (model, test_data->model) == 0)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "invalid value: %s, expecting: %s", model, test_data->model);

	/************************************************************/
	egg_test_title (test, "check datetime for %s", datafile);
	if (g_strcmp0 (datetime, test_data->datetime) == 0)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "invalid value: %s, expecting: %s", datetime, test_data->datetime);

	/************************************************************/
	egg_test_title (test, "check description for %s", datafile);
	if (g_strcmp0 (description, test_data->description) == 0)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "invalid value: %s, expecting: %s", description, test_data->description);

	/************************************************************/
	egg_test_title (test, "check type for %s", datafile);
	if (type == test_data->type)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "invalid value: %i, expecting: %i", type, test_data->type);

	/************************************************************/
	egg_test_title (test, "check colorspace for %s", datafile);
	if (colorspace == test_data->colorspace)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "invalid value: %i, expecting: %i", colorspace, test_data->colorspace);

	/************************************************************/
	egg_test_title (test, "check luminance red %s", datafile);
	g_object_get (profile_lcms1,
		      "red", &xyz,
		      NULL);
	luminance = gcm_xyz_get_x (xyz);
	if (fabs (luminance - test_data->luminance) < 0.001)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "invalid value: %f, expecting: %f", luminance, test_data->luminance);

	g_object_unref (xyz);
	g_object_unref (profile_lcms1);
	g_free (copyright);
	g_free (manufacturer);
	g_free (model);
	g_free (datetime);
	g_free (description);
	g_free (data);
	g_free (filename);
	g_free (filename_tmp);
}

void
gcm_profile_test (EggTest *test)
{
	GcmProfileTestData test_data;

	if (!egg_test_start (test, "GcmProfile"))
		return;

	/* bluish test */
	test_data.copyright = "Copyright (c) 1998 Hewlett-Packard Company";
	test_data.manufacturer = "IEC http://www.iec.ch";
	test_data.model = "IEC 61966-2.1 Default RGB colour space - sRGB";
	test_data.description = "Blueish Test";
	test_data.type = GCM_PROFILE_TYPE_ENUM_DISPLAY_DEVICE;
	test_data.colorspace = GCM_COLORSPACE_ENUM_RGB;
	test_data.luminance = 0.648454;
	test_data.datetime = "February  9 1998, 06:49:00 AM";
	gcm_profile_test_parse_file (test, "bluish.icc", &test_data);

	/* Adobe test */
	test_data.copyright = "Copyright (c) 1998 Hewlett-Packard Company Modified using Adobe Gamma";
	test_data.manufacturer = "IEC http://www.iec.ch";
	test_data.model = "IEC 61966-2.1 Default RGB colour space - sRGB";
	test_data.description = "ADOBEGAMMA-Test";
	test_data.type = GCM_PROFILE_TYPE_ENUM_DISPLAY_DEVICE;
	test_data.colorspace = GCM_COLORSPACE_ENUM_RGB;
	test_data.luminance = 0.648446;
	test_data.datetime = "August 16 2005, 09:49:54 PM";
	gcm_profile_test_parse_file (test, "AdobeGammaTest.icm", &test_data);

	egg_test_end (test);
}
#endif


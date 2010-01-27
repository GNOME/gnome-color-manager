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
 * GNU General Public License for more xyz.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/**
 * SECTION:gcm-xyz
 * @short_description: XYZ color object
 *
 * This object represents a XYZ color block.
 */

#include "config.h"

#include <glib-object.h>
#include <math.h>

#include "gcm-xyz.h"

#include "egg-debug.h"

static void     gcm_xyz_finalize	(GObject     *object);

#define GCM_XYZ_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GCM_TYPE_XYZ, GcmXyzPrivate))

/**
 * GcmXyzPrivate:
 *
 * Private #GcmXyz data
 **/
struct _GcmXyzPrivate
{
	gfloat				 cie_x;
	gfloat				 cie_y;
	gfloat				 cie_z;
};

enum {
	PROP_0,
	PROP_CIE_X,
	PROP_CIE_Y,
	PROP_CIE_Z,
	PROP_LAST
};

G_DEFINE_TYPE (GcmXyz, gcm_xyz, G_TYPE_OBJECT)

/**
 * gcm_xyz_print:
 **/
void
gcm_xyz_print (GcmXyz *xyz)
{
	GcmXyzPrivate *priv = xyz->priv;
	g_print ("xyz: %f,%f,%f\n", priv->cie_x, priv->cie_y, priv->cie_z);
}

/**
 * gcm_xyz_clear:
 **/
void
gcm_xyz_clear (GcmXyz *xyz)
{
	GcmXyzPrivate *priv = xyz->priv;
	priv->cie_x = 0.0f;
	priv->cie_y = 0.0f;
	priv->cie_z = 0.0f;
}

/**
 * gcm_xyz_get_x:
 **/
gfloat
gcm_xyz_get_x (GcmXyz *xyz)
{
	GcmXyzPrivate *priv = xyz->priv;
	if (priv->cie_x + priv->cie_y + priv->cie_z == 0)
		return 0.0f;
	return priv->cie_x / (priv->cie_x + priv->cie_y + priv->cie_z);
}

/**
 * gcm_xyz_get_y:
 **/
gfloat
gcm_xyz_get_y (GcmXyz *xyz)
{
	GcmXyzPrivate *priv = xyz->priv;
	if (priv->cie_x + priv->cie_y + priv->cie_z == 0)
		return 0.0f;
	return priv->cie_y / (priv->cie_x + priv->cie_y + priv->cie_z);
}

/**
 * gcm_xyz_get_z:
 **/
gfloat
gcm_xyz_get_z (GcmXyz *xyz)
{
	GcmXyzPrivate *priv = xyz->priv;
	if (priv->cie_x + priv->cie_y + priv->cie_z == 0)
		return 0.0f;
	return priv->cie_z / (priv->cie_x + priv->cie_y + priv->cie_z);
}

/**
 * gcm_xyz_get_property:
 **/
static void
gcm_xyz_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GcmXyz *xyz = GCM_XYZ (object);
	GcmXyzPrivate *priv = xyz->priv;

	switch (prop_id) {
	case PROP_CIE_X:
		g_value_set_float (value, priv->cie_x);
		break;
	case PROP_CIE_Y:
		g_value_set_float (value, priv->cie_y);
		break;
	case PROP_CIE_Z:
		g_value_set_float (value, priv->cie_z);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gcm_xyz_set_property:
 **/
static void
gcm_xyz_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	GcmXyz *xyz = GCM_XYZ (object);
	GcmXyzPrivate *priv = xyz->priv;

	switch (prop_id) {
	case PROP_CIE_X:
		priv->cie_x = g_value_get_float (value);
		break;
	case PROP_CIE_Y:
		priv->cie_y = g_value_get_float (value);
		break;
	case PROP_CIE_Z:
		priv->cie_z = g_value_get_float (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gcm_xyz_class_init:
 **/
static void
gcm_xyz_class_init (GcmXyzClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gcm_xyz_finalize;
	object_class->get_property = gcm_xyz_get_property;
	object_class->set_property = gcm_xyz_set_property;

	/**
	 * GcmXyz:cie-x:
	 */
	pspec = g_param_spec_float ("cie-x", NULL, NULL,
				    -G_MAXFLOAT, G_MAXFLOAT, 0.0f,
				    G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_CIE_X, pspec);

	/**
	 * GcmXyz:cie-y:
	 */
	pspec = g_param_spec_float ("cie-y", NULL, NULL,
				    -G_MAXFLOAT, G_MAXFLOAT, 0.0f,
				    G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_CIE_Y, pspec);

	/**
	 * GcmXyz:cie-z:
	 */
	pspec = g_param_spec_float ("cie-z", NULL, NULL,
				    -G_MAXFLOAT, G_MAXFLOAT, 0.0f,
				    G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_CIE_Z, pspec);

	g_type_class_add_private (klass, sizeof (GcmXyzPrivate));
}

/**
 * gcm_xyz_init:
 **/
static void
gcm_xyz_init (GcmXyz *xyz)
{
	xyz->priv = GCM_XYZ_GET_PRIVATE (xyz);
}

/**
 * gcm_xyz_finalize:
 **/
static void
gcm_xyz_finalize (GObject *object)
{
//	GcmXyz *xyz = GCM_XYZ (object);
//	GcmXyzPrivate *priv = xyz->priv;

	G_OBJECT_CLASS (gcm_xyz_parent_class)->finalize (object);
}

/**
 * gcm_xyz_new:
 *
 * Return value: a new GcmXyz object.
 **/
GcmXyz *
gcm_xyz_new (void)
{
	GcmXyz *xyz;
	xyz = g_object_new (GCM_TYPE_XYZ, NULL);
	return GCM_XYZ (xyz);
}

/***************************************************************************
 ***                          MAKE CHECK TESTS                           ***
 ***************************************************************************/
#ifdef EGG_TEST
#include "egg-test.h"

void
gcm_xyz_test (EggTest *test)
{
	GcmXyz *xyz;
	gfloat value;

	if (!egg_test_start (test, "GcmXyz"))
		return;

	/************************************************************/
	egg_test_title (test, "get a xyz object");
	xyz = gcm_xyz_new ();
	egg_test_assert (test, xyz != NULL);

	/************************************************************/
	egg_test_title (test, "get x value (when nothing set)");
	value = gcm_xyz_get_x (xyz);
	if (fabs (value - 0.0f) < 0.001f)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "failed to get value, got: %f", value);

	/* set dummy values */
	g_object_set (xyz,
		      "cie-x", 0.125,
		      "cie-y", 0.25,
		      "cie-z", 0.5,
		      NULL);

	/************************************************************/
	egg_test_title (test, "get x value");
	value = gcm_xyz_get_x (xyz);
	if (fabs (value - 0.142857143f) < 0.001f)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "failed to get value, got: %f", value);

	/************************************************************/
	egg_test_title (test, "get y value");
	value = gcm_xyz_get_y (xyz);
	if (fabs (value - 0.285714286f) < 0.001f)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "failed to get value, got: %f", value);

	/************************************************************/
	egg_test_title (test, "get z value");
	value = gcm_xyz_get_z (xyz);
	if (fabs (value - 0.571428571f) < 0.001f)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "failed to get value, got: %f", value);

	g_object_unref (xyz);

	egg_test_end (test);
}
#endif


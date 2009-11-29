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
 * GNU General Public License for more dmi.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:gcm-dmi
 * @short_description: DMI parsing object
 *
 * This object parses DMI data blocks.
 */

#include "config.h"

#include <glib-object.h>
#include <math.h>
#include <string.h>
#include <gio/gio.h>
#include <stdlib.h>

#include "gcm-dmi.h"
#include "gcm-tables.h"

#include "egg-debug.h"

static void     gcm_dmi_finalize	(GObject     *object);

#define GCM_DMI_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GCM_TYPE_DMI, GcmDmiPrivate))

/**
 * GcmDmiPrivate:
 *
 * Private #GcmDmi data
 **/
struct _GcmDmiPrivate
{
	gchar				*product_name;
	gchar				*product_version;
};

enum {
	PROP_0,
	PROP_PRODUCT_NAME,
	PROP_PRODUCT_VERSION,
	PROP_LAST
};

G_DEFINE_TYPE (GcmDmi, gcm_dmi, G_TYPE_OBJECT)

/**
 * gcm_dmi_get_data:
 **/
static gchar *
gcm_dmi_get_data (const gchar *filename)
{
	gboolean ret;
	GError *error = NULL;
	gchar *data = NULL;

	/* get the contents */
	ret = g_file_get_contents (filename, &data, NULL, &error);
	if (!ret) {
		egg_warning ("failed to get contents of %s: %s", filename, error->message);
		g_error_free (error);
	}

	/* get rid of the newline */
	if (data != NULL)
		g_strdelimit (data, "\n", '\0');

	return data;
}

/**
 * gcm_dmi_get_property:
 **/
static void
gcm_dmi_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GcmDmi *dmi = GCM_DMI (object);
	GcmDmiPrivate *priv = dmi->priv;

	switch (prop_id) {
	case PROP_PRODUCT_NAME:
		if (priv->product_name == NULL)
			priv->product_name = gcm_dmi_get_data ("/sys/class/dmi/id/product_name");
		g_value_set_string (value, priv->product_name);
		break;
	case PROP_PRODUCT_VERSION:
		if (priv->product_version == NULL)
			priv->product_version = gcm_dmi_get_data ("/sys/class/dmi/id/product_version");
		g_value_set_string (value, priv->product_version);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gcm_dmi_set_property:
 **/
static void
gcm_dmi_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gcm_dmi_class_init:
 **/
static void
gcm_dmi_class_init (GcmDmiClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gcm_dmi_finalize;
	object_class->get_property = gcm_dmi_get_property;
	object_class->set_property = gcm_dmi_set_property;

	/**
	 * GcmDmi:product-name:
	 */
	pspec = g_param_spec_string ("product-name", NULL, NULL,
				     NULL,
				     G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_PRODUCT_NAME, pspec);

	/**
	 * GcmDmi:product-version:
	 */
	pspec = g_param_spec_string ("product-version", NULL, NULL,
				     NULL,
				     G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_PRODUCT_VERSION, pspec);

	g_type_class_add_private (klass, sizeof (GcmDmiPrivate));
}

/**
 * gcm_dmi_init:
 **/
static void
gcm_dmi_init (GcmDmi *dmi)
{
	dmi->priv = GCM_DMI_GET_PRIVATE (dmi);
	dmi->priv->product_name = NULL;
	dmi->priv->product_version = NULL;
}

/**
 * gcm_dmi_finalize:
 **/
static void
gcm_dmi_finalize (GObject *object)
{
	GcmDmi *dmi = GCM_DMI (object);
	GcmDmiPrivate *priv = dmi->priv;

	g_free (priv->product_name);
	g_free (priv->product_version);

	G_OBJECT_CLASS (gcm_dmi_parent_class)->finalize (object);
}

/**
 * gcm_dmi_new:
 *
 * Return value: a new GcmDmi object.
 **/
GcmDmi *
gcm_dmi_new (void)
{
	GcmDmi *dmi;
	dmi = g_object_new (GCM_TYPE_DMI, NULL);
	return GCM_DMI (dmi);
}

/***************************************************************************
 ***                          MAKE CHECK TESTS                           ***
 ***************************************************************************/
#ifdef EGG_TEST
#include "egg-test.h"

void
gcm_dmi_test (EggTest *test)
{
	GcmDmi *dmi;
	gchar *product_name = NULL;
	gchar *product_version = NULL;

	if (!egg_test_start (test, "GcmDmi"))
		return;

	/************************************************************/
	egg_test_title (test, "get a dmi object");
	dmi = gcm_dmi_new ();
	egg_test_assert (test, dmi != NULL);

	/* get data */
	g_object_get (dmi,
		      "product-name", &product_name,
		      "product-version", &product_version,
		      NULL);

	/************************************************************/
	egg_test_title (test, "got name");
	egg_test_assert (test, product_name != NULL);

	/************************************************************/
	egg_test_title (test, "got version");
	egg_test_assert (test, product_version != NULL);

	g_free (product_name);
	g_free (product_version);

	g_object_unref (dmi);

	egg_test_end (test);
}
#endif


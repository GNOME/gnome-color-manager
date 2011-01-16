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
#include <sane/sane.h>

#include "gcm-device-sane.h"
#include "gcm-enum.h"
#include "gcm-utils.h"

static void     gcm_device_sane_finalize	(GObject     *object);

#define GCM_DEVICE_SANE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GCM_TYPE_DEVICE_SANE, GcmDeviceSanePrivate))

/**
 * GcmDeviceSanePrivate:
 *
 * Private #GcmDeviceSane data
 **/
struct _GcmDeviceSanePrivate
{
	gchar				*native_device;
};

enum {
	PROP_0,
	PROP_NATIVE_DEVICE,
	PROP_LAST
};

G_DEFINE_TYPE (GcmDeviceSane, gcm_device_sane, GCM_TYPE_DEVICE)

/**
 * gcm_device_sane_set_from_device:
 **/
gboolean
gcm_device_sane_set_from_device (GcmDevice *device, const SANE_Device *sane_device, GError **error)
{
	gchar *id = NULL;
	gchar *manufacturer = NULL;
	gchar *model = NULL;
	gchar *title = NULL;

	g_debug ("name=%s", sane_device->name);
	g_debug ("vendor=%s", sane_device->vendor);
	g_debug ("model=%s", sane_device->model);
	g_debug ("type=%s", sane_device->type);

	/* convert device_id 'plustek:libusb:004:002' to suitable id */
	id = g_strdup_printf ("sane_%s", sane_device->model);
	gcm_utils_alphanum_lcase (id);

	/* make safe strings */
	manufacturer = g_strdup (sane_device->vendor);
	model = g_strdup (sane_device->model);
	title = g_strdup_printf ("%s - %s", manufacturer, model);

	/* set properties on device */
	g_object_set (device,
		      "kind", CD_DEVICE_KIND_SCANNER,
		      "colorspace", GCM_COLORSPACE_RGB,
		      "id", id,
		      "connected", TRUE,
//		      "serial", serial,
		      "model", model,
		      "manufacturer", manufacturer,
		      "title", title,
		      "native-device", sane_device->name,
		      NULL);

	g_free (manufacturer);
	g_free (model);
	g_free (id);
	g_free (title);
	return TRUE;
}

/**
 * gcm_device_sane_get_property:
 **/
static void
gcm_device_sane_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GcmDeviceSane *device_sane = GCM_DEVICE_SANE (object);
	GcmDeviceSanePrivate *priv = device_sane->priv;

	switch (prop_id) {
	case PROP_NATIVE_DEVICE:
		g_value_set_string (value, priv->native_device);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gcm_device_sane_set_property:
 **/
static void
gcm_device_sane_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	GcmDeviceSane *device_sane = GCM_DEVICE_SANE (object);
	GcmDeviceSanePrivate *priv = device_sane->priv;

	switch (prop_id) {
	case PROP_NATIVE_DEVICE:
		g_free (priv->native_device);
		priv->native_device = g_strdup (g_value_get_string (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gcm_device_sane_class_init:
 **/
static void
gcm_device_sane_class_init (GcmDeviceSaneClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = gcm_device_sane_finalize;
	object_class->get_property = gcm_device_sane_get_property;
	object_class->set_property = gcm_device_sane_set_property;

	/**
	 * GcmDeviceSane:native-device:
	 */
	pspec = g_param_spec_string ("native-device", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_NATIVE_DEVICE, pspec);

	g_type_class_add_private (klass, sizeof (GcmDeviceSanePrivate));
}

/**
 * gcm_device_sane_init:
 **/
static void
gcm_device_sane_init (GcmDeviceSane *device_sane)
{
	device_sane->priv = GCM_DEVICE_SANE_GET_PRIVATE (device_sane);
	device_sane->priv->native_device = NULL;
}

/**
 * gcm_device_sane_finalize:
 **/
static void
gcm_device_sane_finalize (GObject *object)
{
	GcmDeviceSane *device_sane = GCM_DEVICE_SANE (object);
	GcmDeviceSanePrivate *priv = device_sane->priv;

	g_free (priv->native_device);

	G_OBJECT_CLASS (gcm_device_sane_parent_class)->finalize (object);
}

/**
 * gcm_device_sane_new:
 *
 * Return value: a new #GcmDevice object.
 **/
GcmDevice *
gcm_device_sane_new (void)
{
	GcmDevice *device;
	device = g_object_new (GCM_TYPE_DEVICE_SANE, NULL);
	return GCM_DEVICE (device);
}


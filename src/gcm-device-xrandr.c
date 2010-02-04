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

#include "gcm-device-xrandr.h"

#include "egg-debug.h"

static void     gcm_device_xrandr_finalize	(GObject     *object);

#define GCM_DEVICE_XRANDR_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GCM_TYPE_DEVICE_XRANDR, GcmDeviceXrandrPrivate))

/**
 * GcmDeviceXrandrPrivate:
 *
 * Private #GcmDeviceXrandr data
 **/
struct _GcmDeviceXrandrPrivate
{
	gchar				*native_device;
};

enum {
	PROP_0,
	PROP_NATIVE_DEVICE,
	PROP_LAST
};

G_DEFINE_TYPE (GcmDeviceXrandr, gcm_device_xrandr, GCM_TYPE_DEVICE)

/**
 * gcm_device_xrandr_set_from_instance:
 **/
gboolean
gcm_device_xrandr_set_from_instance (GcmDevice *device, gpointer instance, GError **error)
{
	return TRUE;
}

/**
 * gcm_device_xrandr_get_property:
 **/
static void
gcm_device_xrandr_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GcmDeviceXrandr *device_xrandr = GCM_DEVICE_XRANDR (object);
	GcmDeviceXrandrPrivate *priv = device_xrandr->priv;

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
 * gcm_device_xrandr_set_property:
 **/
static void
gcm_device_xrandr_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	GcmDeviceXrandr *device_xrandr = GCM_DEVICE_XRANDR (object);
	GcmDeviceXrandrPrivate *priv = device_xrandr->priv;

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
 * gcm_device_xrandr_class_init:
 **/
static void
gcm_device_xrandr_class_init (GcmDeviceXrandrClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gcm_device_xrandr_finalize;
	object_class->get_property = gcm_device_xrandr_get_property;
	object_class->set_property = gcm_device_xrandr_set_property;


	/**
	 * GcmDeviceXrandr:native-device:
	 */
	pspec = g_param_spec_string ("native-device", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_NATIVE_DEVICE, pspec);

	g_type_class_add_private (klass, sizeof (GcmDeviceXrandrPrivate));
}

/**
 * gcm_device_xrandr_init:
 **/
static void
gcm_device_xrandr_init (GcmDeviceXrandr *device_xrandr)
{
	device_xrandr->priv = GCM_DEVICE_XRANDR_GET_PRIVATE (device_xrandr);
	device_xrandr->priv->native_device = NULL;
}

/**
 * gcm_device_xrandr_finalize:
 **/
static void
gcm_device_xrandr_finalize (GObject *object)
{
	GcmDeviceXrandr *device_xrandr = GCM_DEVICE_XRANDR (object);
	GcmDeviceXrandrPrivate *priv = device_xrandr->priv;

	g_free (priv->native_device);

	G_OBJECT_CLASS (gcm_device_xrandr_parent_class)->finalize (object);
}

/**
 * gcm_device_xrandr_new:
 *
 * Return value: a new #GcmDevice object.
 **/
GcmDevice *
gcm_device_xrandr_new (void)
{
	GcmDevice *device;
	device = g_object_new (GCM_TYPE_DEVICE_XRANDR, NULL);
	return GCM_DEVICE (device);
}


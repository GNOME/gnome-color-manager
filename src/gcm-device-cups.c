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
#include <cups/cups.h>
#include <cups/ppd.h>

#include "gcm-device-cups.h"

#include "egg-debug.h"

static void     gcm_device_cups_finalize	(GObject     *object);

#define GCM_DEVICE_CUPS_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GCM_TYPE_DEVICE_CUPS, GcmDeviceCupsPrivate))

/**
 * GcmDeviceCupsPrivate:
 *
 * Private #GcmDeviceCups data
 **/
struct _GcmDeviceCupsPrivate
{
	gchar				*native_device;
};

enum {
	PROP_0,
	PROP_NATIVE_DEVICE,
	PROP_LAST
};

G_DEFINE_TYPE (GcmDeviceCups, gcm_device_cups, GCM_TYPE_DEVICE)

/**
 * gcm_device_cups_set_from_instance:
 **/
gboolean
gcm_device_cups_set_from_instance (GcmDevice *device, gpointer instance, GError **error)
{
	return TRUE;
}

/**
 * gcm_device_cups_get_property:
 **/
static void
gcm_device_cups_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GcmDeviceCups *device_cups = GCM_DEVICE_CUPS (object);
	GcmDeviceCupsPrivate *priv = device_cups->priv;

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
 * gcm_device_cups_set_property:
 **/
static void
gcm_device_cups_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	GcmDeviceCups *device_cups = GCM_DEVICE_CUPS (object);
	GcmDeviceCupsPrivate *priv = device_cups->priv;

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
 * gcm_device_cups_class_init:
 **/
static void
gcm_device_cups_class_init (GcmDeviceCupsClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gcm_device_cups_finalize;
	object_class->get_property = gcm_device_cups_get_property;
	object_class->set_property = gcm_device_cups_set_property;


	/**
	 * GcmDeviceCups:native-device:
	 */
	pspec = g_param_spec_string ("native-device", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_NATIVE_DEVICE, pspec);

	g_type_class_add_private (klass, sizeof (GcmDeviceCupsPrivate));
}

/**
 * gcm_device_cups_init:
 **/
static void
gcm_device_cups_init (GcmDeviceCups *device_cups)
{
	device_cups->priv = GCM_DEVICE_CUPS_GET_PRIVATE (device_cups);
	device_cups->priv->native_device = NULL;
}

/**
 * gcm_device_cups_finalize:
 **/
static void
gcm_device_cups_finalize (GObject *object)
{
	GcmDeviceCups *device_cups = GCM_DEVICE_CUPS (object);
	GcmDeviceCupsPrivate *priv = device_cups->priv;

	g_free (priv->native_device);

	G_OBJECT_CLASS (gcm_device_cups_parent_class)->finalize (object);
}

/**
 * gcm_device_cups_new:
 *
 * Return value: a new #GcmDevice object.
 **/
GcmDevice *
gcm_device_cups_new (void)
{
	GcmDevice *device;
	device = g_object_new (GCM_TYPE_DEVICE_CUPS, NULL);
	return GCM_DEVICE (device);
}


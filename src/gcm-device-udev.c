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

#include "gcm-device-udev.h"
#include "gcm-utils.h"

#include "egg-debug.h"

static void     gcm_device_udev_finalize	(GObject     *object);

#define GCM_DEVICE_UDEV_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GCM_TYPE_DEVICE_UDEV, GcmDeviceUdevPrivate))

/**
 * GcmDeviceUdevPrivate:
 *
 * Private #GcmDeviceUdev data
 **/
struct _GcmDeviceUdevPrivate
{
	gchar				*native_device;
};

enum {
	PROP_0,
	PROP_NATIVE_DEVICE,
	PROP_LAST
};

G_DEFINE_TYPE (GcmDeviceUdev, gcm_device_udev, GCM_TYPE_DEVICE)

/**
 * gcm_device_udev_get_id_for_udev_device:
 **/
static gchar *
gcm_device_udev_get_id_for_udev_device (GUdevDevice *udev_device)
{
	gchar *id;

	/* get id */
	id = g_strdup_printf ("sysfs_%s_%s",
			      g_udev_device_get_property (udev_device, "ID_VENDOR"),
			      g_udev_device_get_property (udev_device, "ID_MODEL"));

	/* replace unsafe chars */
	gcm_utils_alphanum_lcase (id);
	return id;
}

/**
 * gcm_device_udev_set_from_device:
 **/
gboolean
gcm_device_udev_set_from_device (GcmDevice *device, GUdevDevice *udev_device, GError **error)
{
	gchar *id = NULL;
	gchar *title;
	gchar *serial = NULL;
	gchar *manufacturer = NULL;
	gchar *model = NULL;
	GcmDeviceTypeEnum type;
	const gchar *value;
	const gchar *sysfs_path;

	/* matches our udev rules, which we can change without recompiling */
	value = g_udev_device_get_property (udev_device, "GCM_TYPE");
	type = gcm_device_type_enum_from_string (value);

	/* add new device */
	id = gcm_device_udev_get_id_for_udev_device (udev_device);
	title = g_strdup_printf ("%s - %s",
				g_udev_device_get_property (udev_device, "ID_VENDOR"),
				g_udev_device_get_property (udev_device, "ID_MODEL"));

	/* turn space delimiters into spaces */
	g_strdelimit (title, "_", ' ');

	/* get sysfs path */
	sysfs_path = g_udev_device_get_sysfs_path (udev_device);

	/* create device */
	manufacturer = g_strdup (g_udev_device_get_property (udev_device, "ID_VENDOR"));
	model = g_strdup (g_udev_device_get_property (udev_device, "ID_MODEL"));
	serial = g_strdup (g_udev_device_get_property (udev_device, "ID_SERIAL"));

	/* get rid of underscores */
	if (manufacturer != NULL) {
		g_strdelimit (manufacturer, "_", ' ');
		g_strchomp (manufacturer);
	}
	if (model != NULL) {
		g_strdelimit (model, "_", ' ');
		g_strchomp (model);
	}
	if (serial != NULL) {
		g_strdelimit (serial, "_", ' ');
		g_strchomp (serial);
	}

	g_object_set (device,
		      "type", type,
		      "id", id,
		      "connected", TRUE,
		      "serial", serial,
		      "model", model,
		      "manufacturer", manufacturer,
		      "title", title,
		      "native-device", sysfs_path,
		      NULL);

	g_free (serial);
	g_free (manufacturer);
	g_free (model);
	g_free (id);
	g_free (title);
	return TRUE;
}

/**
 * gcm_device_udev_get_property:
 **/
static void
gcm_device_udev_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GcmDeviceUdev *device_udev = GCM_DEVICE_UDEV (object);
	GcmDeviceUdevPrivate *priv = device_udev->priv;

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
 * gcm_device_udev_set_property:
 **/
static void
gcm_device_udev_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	GcmDeviceUdev *device_udev = GCM_DEVICE_UDEV (object);
	GcmDeviceUdevPrivate *priv = device_udev->priv;

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
 * gcm_device_udev_class_init:
 **/
static void
gcm_device_udev_class_init (GcmDeviceUdevClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gcm_device_udev_finalize;
	object_class->get_property = gcm_device_udev_get_property;
	object_class->set_property = gcm_device_udev_set_property;


	/**
	 * GcmDeviceUdev:native-device:
	 */
	pspec = g_param_spec_string ("native-device", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_NATIVE_DEVICE, pspec);

	g_type_class_add_private (klass, sizeof (GcmDeviceUdevPrivate));
}

/**
 * gcm_device_udev_init:
 **/
static void
gcm_device_udev_init (GcmDeviceUdev *device_udev)
{
	device_udev->priv = GCM_DEVICE_UDEV_GET_PRIVATE (device_udev);
	device_udev->priv->native_device = NULL;
}

/**
 * gcm_device_udev_finalize:
 **/
static void
gcm_device_udev_finalize (GObject *object)
{
	GcmDeviceUdev *device_udev = GCM_DEVICE_UDEV (object);
	GcmDeviceUdevPrivate *priv = device_udev->priv;

	g_free (priv->native_device);

	G_OBJECT_CLASS (gcm_device_udev_parent_class)->finalize (object);
}

/**
 * gcm_device_udev_new:
 *
 * Return value: a new #GcmDevice object.
 **/
GcmDevice *
gcm_device_udev_new (void)
{
	GcmDevice *device;
	device = g_object_new (GCM_TYPE_DEVICE_UDEV, NULL);
	return GCM_DEVICE (device);
}


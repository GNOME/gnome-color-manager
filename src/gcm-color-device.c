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
 * SECTION:gcm-color-device
 * @short_description: Colorimeter device abstraction
 *
 * This object allows the programmer to detect a color sensor device.
 */

#include "config.h"

#include <glib/gi18n.h>
#include <gudev/gudev.h>

#include "gcm-color-device.h"

#include "egg-debug.h"

static void     gcm_color_device_finalize	(GObject     *object);

#define GCM_COLOR_DEVICE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GCM_TYPE_COLOR_DEVICE, GcmColorDevicePrivate))

/**
 * GcmColorDevicePrivate:
 *
 * Private #GcmColorDevice data
 **/
struct _GcmColorDevicePrivate
{
	gboolean			 present;
	gchar				*vendor;
	gchar				*model;
	GUdevClient			*client;
};

enum {
	PROP_0,
	PROP_PRESENT,
	PROP_VENDOR,
	PROP_MODEL,
	PROP_LAST
};

enum {
	SIGNAL_CHANGED,
	SIGNAL_LAST
};

static guint signals[SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE (GcmColorDevice, gcm_color_device, G_TYPE_OBJECT)

/**
 * gcm_color_device_get_property:
 **/
static void
gcm_color_device_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GcmColorDevice *color_device = GCM_COLOR_DEVICE (object);
	GcmColorDevicePrivate *priv = color_device->priv;

	switch (prop_id) {
	case PROP_PRESENT:
		g_value_set_boolean (value, priv->present);
		break;
	case PROP_VENDOR:
		g_value_set_string (value, priv->vendor);
		break;
	case PROP_MODEL:
		g_value_set_string (value, priv->model);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gcm_color_device_set_property:
 **/
static void
gcm_color_device_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	GcmColorDevice *color_device = GCM_COLOR_DEVICE (object);
	GcmColorDevicePrivate *priv = color_device->priv;

	switch (prop_id) {
	case PROP_PRESENT:
		priv->present = g_value_get_boolean (value);
		break;
	case PROP_VENDOR:
		g_free (priv->vendor);
		priv->vendor = g_strdup (g_value_get_string (value));
		break;
	case PROP_MODEL:
		g_free (priv->model);
		priv->model = g_strdup (g_value_get_string (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gcm_color_device_class_init:
 **/
static void
gcm_color_device_class_init (GcmColorDeviceClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gcm_color_device_finalize;
	object_class->get_property = gcm_color_device_get_property;
	object_class->set_property = gcm_color_device_set_property;

	/**
	 * GcmColorDevice:present:
	 */
	pspec = g_param_spec_boolean ("present", NULL, NULL,
				      FALSE,
				      G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_PRESENT, pspec);

	/**
	 * GcmColorDevice:vendor:
	 */
	pspec = g_param_spec_string ("vendor", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_VENDOR, pspec);

	/**
	 * GcmColorDevice:model:
	 */
	pspec = g_param_spec_string ("model", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_MODEL, pspec);

	/**
	 * GcmColorDevice::added:
	 **/
	signals[SIGNAL_CHANGED] =
		g_signal_new ("changed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GcmColorDeviceClass, changed),
			      NULL, NULL, g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	g_type_class_add_private (klass, sizeof (GcmColorDevicePrivate));
}


/**
 * gcm_color_device_device_add:
 **/
static gboolean
gcm_color_device_device_add (GcmColorDevice *color_device, GUdevDevice *device)
{
	gboolean ret;
	GcmColorDevicePrivate *priv = color_device->priv;

	/* interesting device? */
	ret = g_udev_device_get_property_as_boolean (device, "COLOR_MEASUREMENT_DEVICE");
	if (!ret)
		goto out;

	/* get data */
	egg_debug ("adding color management device: %s", g_udev_device_get_sysfs_path (device));
	priv->present = TRUE;

	/* vendor */
	g_free (priv->vendor);
	priv->vendor = g_strdup (g_udev_device_get_property (device, "ID_VENDOR_FROM_DATABASE"));

	/* model */
	g_free (priv->model);
	priv->model = g_strdup (g_udev_device_get_property (device, "ID_MODEL_FROM_DATABASE"));

	/* signal the addition */
	egg_debug ("emit: changed");
	g_signal_emit (color_device, signals[SIGNAL_CHANGED], 0);
out:
	return ret;
}

/**
 * gcm_color_device_device_remove:
 **/
static gboolean
gcm_color_device_device_remove (GcmColorDevice *color_device, GUdevDevice *device)
{
	gboolean ret;
	GcmColorDevicePrivate *priv = color_device->priv;

	/* interesting device? */
	ret = g_udev_device_get_property_as_boolean (device, "COLOR_MEASUREMENT_DEVICE");
	if (!ret)
		goto out;

	/* get data */
	egg_debug ("removing color management device: %s", g_udev_device_get_sysfs_path (device));
	priv->present = FALSE;

	/* vendor */
	g_free (priv->vendor);
	priv->vendor = NULL;

	/* model */
	g_free (priv->model);
	priv->model = NULL;

	/* signal the removal */
	egg_debug ("emit: changed");
	g_signal_emit (color_device, signals[SIGNAL_CHANGED], 0);
out:
	return ret;
}

/**
 * gcm_color_device_coldplug:
 **/
static gboolean
gcm_color_device_coldplug (GcmColorDevice *color_device)
{
	GList *devices;
	GList *l;
	gboolean ret = FALSE;
	GcmColorDevicePrivate *priv = color_device->priv;

	/* get all USB devices */
	devices = g_udev_client_query_by_subsystem (priv->client, "usb");
	for (l = devices; l != NULL; l = l->next) {
		ret = gcm_color_device_device_add (color_device, l->data);
		if (ret) {
			egg_debug ("found color management device");
			break;
		}
	}

	g_list_foreach (devices, (GFunc) g_object_unref, NULL);
	g_list_free (devices);
	return ret;
}

/**
 * gcm_prefs_uevent_cb:
 **/
static void
gcm_prefs_uevent_cb (GUdevClient *client, const gchar *action, GUdevDevice *device, GcmColorDevice *color_device)
{
	egg_debug ("uevent %s", action);
	if (g_strcmp0 (action, "add") == 0) {
		gcm_color_device_device_add (color_device, device);
	} else if (g_strcmp0 (action, "remove") == 0) {
		gcm_color_device_device_remove (color_device, device);
	}
}

/**
 * gcm_color_device_init:
 **/
static void
gcm_color_device_init (GcmColorDevice *color_device)
{
	const gchar *subsystems[] = {"usb", NULL};

	color_device->priv = GCM_COLOR_DEVICE_GET_PRIVATE (color_device);
	color_device->priv->vendor = NULL;
	color_device->priv->model = NULL;

	/* use GUdev to find the calibration device */
	color_device->priv->client = g_udev_client_new (subsystems);
	g_signal_connect (color_device->priv->client, "uevent",
			  G_CALLBACK (gcm_prefs_uevent_cb), color_device);

	/* coldplug */
	gcm_color_device_coldplug (color_device);
}

/**
 * gcm_color_device_finalize:
 **/
static void
gcm_color_device_finalize (GObject *object)
{
	GcmColorDevice *color_device = GCM_COLOR_DEVICE (object);
	GcmColorDevicePrivate *priv = color_device->priv;

	g_object_unref (priv->client);
	g_free (priv->vendor);
	g_free (priv->model);

	G_OBJECT_CLASS (gcm_color_device_parent_class)->finalize (object);
}

/**
 * gcm_color_device_new:
 *
 * Return value: a new GcmColorDevice object.
 **/
GcmColorDevice *
gcm_color_device_new (void)
{
	GcmColorDevice *color_device;
	color_device = g_object_new (GCM_TYPE_COLOR_DEVICE, NULL);
	return GCM_COLOR_DEVICE (color_device);
}


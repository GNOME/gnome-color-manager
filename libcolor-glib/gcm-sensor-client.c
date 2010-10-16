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
 * SECTION:gcm-sensor-client
 * @short_description: SensorClient device abstraction
 *
 * This object allows the programmer to detect a color sensor device.
 */

#include "config.h"

#include <glib/gi18n.h>
#include <gudev/gudev.h>
#include <gtk/gtk.h>

#include "gcm-sensor-client.h"
#include "gcm-sensor-huey.h"

#include "egg-debug.h"

static void     gcm_sensor_client_finalize	(GObject     *object);

#define GCM_SENSOR_CLIENT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GCM_TYPE_SENSOR_CLIENT, GcmSensorClientPrivate))

/**
 * GcmSensorClientPrivate:
 *
 * Private #GcmSensorClient data
 **/
struct _GcmSensorClientPrivate
{
	gboolean			 present;
	GUdevClient			*client;
	GcmSensor			*sensor;
};

enum {
	PROP_0,
	PROP_PRESENT,
	PROP_LAST
};

enum {
	SIGNAL_CHANGED,
	SIGNAL_LAST
};

static guint signals[SIGNAL_LAST] = { 0 };
static gpointer gcm_sensor_client_object = NULL;

G_DEFINE_TYPE (GcmSensorClient, gcm_sensor_client, G_TYPE_OBJECT)

/**
 * gcm_sensor_client_get_property:
 **/
static void
gcm_sensor_client_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GcmSensorClient *sensor_client = GCM_SENSOR_CLIENT (object);
	GcmSensorClientPrivate *priv = sensor_client->priv;

	switch (prop_id) {
	case PROP_PRESENT:
		g_value_set_boolean (value, priv->present);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gcm_sensor_client_set_property:
 **/
static void
gcm_sensor_client_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gcm_sensor_client_class_init:
 **/
static void
gcm_sensor_client_class_init (GcmSensorClientClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gcm_sensor_client_finalize;
	object_class->get_property = gcm_sensor_client_get_property;
	object_class->set_property = gcm_sensor_client_set_property;

	/**
	 * GcmSensorClient:present:
	 */
	pspec = g_param_spec_boolean ("present", NULL, NULL,
				      FALSE,
				      G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_PRESENT, pspec);

	/**
	 * GcmSensorClient::changed:
	 *
	 * Since: 2.91.1
	 **/
	signals[SIGNAL_CHANGED] =
		g_signal_new ("changed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GcmSensorClientClass, changed),
			      NULL, NULL, g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	g_type_class_add_private (klass, sizeof (GcmSensorClientPrivate));
}

/**
 * gcm_sensor_client_get_sensor:
 *
 * Since: 2.91.1
 **/
GcmSensor *
gcm_sensor_client_get_sensor (GcmSensorClient *sensor_client)
{
	return sensor_client->priv->sensor;
}

/**
 * gcm_sensor_client_get_present:
 *
 * Since: 2.91.1
 **/
gboolean
gcm_sensor_client_get_present (GcmSensorClient *sensor_client)
{
	return sensor_client->priv->present;
}

/**
 * gcm_sensor_client_device_add:
 **/
static gboolean
gcm_sensor_client_device_add (GcmSensorClient *sensor_client, GUdevDevice *device)
{
	gboolean ret;
	GcmSensor *sensor = NULL;
	GcmSensorKind kind;
	const gchar *kind_str;
	GcmSensorClientPrivate *priv = sensor_client->priv;

	/* interesting device? */
	ret = g_udev_device_get_property_as_boolean (device, "GCM_COLORIMETER");
	if (!ret)
		goto out;

	/* is it a sensor we have a internal native driver for? */
	kind_str = g_udev_device_get_property (device, "GCM_KIND");
	kind = gcm_sensor_kind_from_string (kind_str);
	if (kind == GCM_SENSOR_KIND_HUEY) {
		egg_warning ("creating internal device");
		sensor = gcm_sensor_huey_new ();
	} else {
		sensor = gcm_sensor_new ();
	}

	/* get data */
	egg_debug ("adding color management device: %s", g_udev_device_get_sysfs_path (device));
	ret = gcm_sensor_set_from_device (sensor, device, NULL);
	if (!ret)
		goto out;

	/* success */
	priv->present = TRUE;
	priv->sensor = g_object_ref (sensor);

	/* signal the addition */
	egg_debug ("emit: changed");
	g_signal_emit (sensor_client, signals[SIGNAL_CHANGED], 0);
out:
	if (sensor != NULL)
		g_object_unref (sensor);
	return ret;
}

/**
 * gcm_sensor_client_device_remove:
 **/
static gboolean
gcm_sensor_client_device_remove (GcmSensorClient *sensor_client, GUdevDevice *device)
{
	gboolean ret;
	GcmSensorClientPrivate *priv = sensor_client->priv;

	/* interesting device? */
	ret = g_udev_device_get_property_as_boolean (device, "GCM_SENSOR_CLIENT");
	if (!ret)
		goto out;

	/* get data */
	egg_debug ("removing color management device: %s", g_udev_device_get_sysfs_path (device));
	priv->present = FALSE;
	if (priv->sensor != NULL)
		g_object_unref (priv->sensor);
	priv->sensor = NULL;

	/* signal the removal */
	egg_debug ("emit: changed");
	g_signal_emit (sensor_client, signals[SIGNAL_CHANGED], 0);
out:
	return ret;
}

/**
 * gcm_sensor_client_coldplug:
 **/
static gboolean
gcm_sensor_client_coldplug (GcmSensorClient *sensor_client)
{
	GList *devices;
	GList *l;
	gboolean ret = FALSE;
	GcmSensorClientPrivate *priv = sensor_client->priv;

	/* get all USB devices */
	devices = g_udev_client_query_by_subsystem (priv->client, "usb");
	for (l = devices; l != NULL; l = l->next) {
		ret = gcm_sensor_client_device_add (sensor_client, l->data);
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
 * gcm_sensor_client_uevent_cb:
 **/
static void
gcm_sensor_client_uevent_cb (GUdevClient *client, const gchar *action, GUdevDevice *device, GcmSensorClient *sensor_client)
{
	egg_debug ("uevent %s", action);
	if (g_strcmp0 (action, "add") == 0) {
		gcm_sensor_client_device_add (sensor_client, device);
	} else if (g_strcmp0 (action, "remove") == 0) {
		gcm_sensor_client_device_remove (sensor_client, device);
	}
}

/**
 * gcm_sensor_client_init:
 **/
static void
gcm_sensor_client_init (GcmSensorClient *sensor_client)
{
	const gchar *subsystems[] = {"usb", NULL};

	sensor_client->priv = GCM_SENSOR_CLIENT_GET_PRIVATE (sensor_client);

	/* use GUdev to find the calibration device */
	sensor_client->priv->client = g_udev_client_new (subsystems);
	g_signal_connect (sensor_client->priv->client, "uevent",
			  G_CALLBACK (gcm_sensor_client_uevent_cb), sensor_client);

	/* coldplug */
	gcm_sensor_client_coldplug (sensor_client);
}

/**
 * gcm_sensor_client_finalize:
 **/
static void
gcm_sensor_client_finalize (GObject *object)
{
	GcmSensorClient *sensor_client = GCM_SENSOR_CLIENT (object);
	GcmSensorClientPrivate *priv = sensor_client->priv;

	g_object_unref (priv->client);
	if (priv->sensor != NULL)
		g_object_unref (priv->sensor);

	G_OBJECT_CLASS (gcm_sensor_client_parent_class)->finalize (object);
}

/**
 * gcm_sensor_client_new:
 *
 * Return value: a new GcmSensorClient object.
 *
 * Since: 2.91.1
 **/
GcmSensorClient *
gcm_sensor_client_new (void)
{
	if (gcm_sensor_client_object != NULL) {
		g_object_ref (gcm_sensor_client_object);
	} else {
		gcm_sensor_client_object = g_object_new (GCM_TYPE_SENSOR_CLIENT, NULL);
		g_object_add_weak_pointer (gcm_sensor_client_object, &gcm_sensor_client_object);
	}
	return GCM_SENSOR_CLIENT (gcm_sensor_client_object);
}


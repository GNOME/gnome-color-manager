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
 * SECTION:gcm-colorimeter
 * @short_description: Colorimeter device abstraction
 *
 * This object allows the programmer to detect a color sensor device.
 */

#include "config.h"

#include <glib/gi18n.h>
#include <gudev/gudev.h>
#include <gtk/gtk.h>

#include "gcm-colorimeter.h"
#include "gcm-utils.h"

#include "egg-debug.h"

static void     gcm_colorimeter_finalize	(GObject     *object);

#define GCM_COLORIMETER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GCM_TYPE_COLORIMETER, GcmColorimeterPrivate))

/**
 * GcmColorimeterPrivate:
 *
 * Private #GcmColorimeter data
 **/
struct _GcmColorimeterPrivate
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
static gpointer gcm_colorimeter_object = NULL;

G_DEFINE_TYPE (GcmColorimeter, gcm_colorimeter, G_TYPE_OBJECT)

/**
 * gcm_colorimeter_get_property:
 **/
static void
gcm_colorimeter_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GcmColorimeter *colorimeter = GCM_COLORIMETER (object);
	GcmColorimeterPrivate *priv = colorimeter->priv;

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
 * gcm_colorimeter_set_property:
 **/
static void
gcm_colorimeter_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gcm_colorimeter_class_init:
 **/
static void
gcm_colorimeter_class_init (GcmColorimeterClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gcm_colorimeter_finalize;
	object_class->get_property = gcm_colorimeter_get_property;
	object_class->set_property = gcm_colorimeter_set_property;

	/**
	 * GcmColorimeter:present:
	 */
	pspec = g_param_spec_boolean ("present", NULL, NULL,
				      FALSE,
				      G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_PRESENT, pspec);

	/**
	 * GcmColorimeter::changed:
	 **/
	signals[SIGNAL_CHANGED] =
		g_signal_new ("changed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GcmColorimeterClass, changed),
			      NULL, NULL, g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	g_type_class_add_private (klass, sizeof (GcmColorimeterPrivate));
}

/**
 * gcm_colorimeter_get_sensor:
 **/
GcmSensor *
gcm_colorimeter_get_sensor (GcmColorimeter *colorimeter)
{
	return colorimeter->priv->sensor;
}

/**
 * gcm_colorimeter_get_present:
 **/
gboolean
gcm_colorimeter_get_present (GcmColorimeter *colorimeter)
{
	return colorimeter->priv->present;
}

/**
 * gcm_colorimeter_device_add:
 **/
static gboolean
gcm_colorimeter_device_add (GcmColorimeter *colorimeter, GUdevDevice *device)
{
	const gchar *vendor;
	const gchar *model;
	GcmSensorKind colorimeter_kind;
	gboolean ret;
	const gchar *kind_str;
	GcmColorimeterPrivate *priv = colorimeter->priv;

	/* interesting device? */
	ret = g_udev_device_get_property_as_boolean (device, "GCM_COLORIMETER");
	if (!ret)
		goto out;

	/* get data */
	egg_debug ("adding color management device: %s", g_udev_device_get_sysfs_path (device));
	priv->present = TRUE;
	priv->sensor = gcm_sensor_new ();

	/* vendor */
	vendor = g_udev_device_get_property (device, "ID_VENDOR_FROM_DATABASE");
	if (vendor == NULL)
		vendor = g_udev_device_get_property (device, "ID_VENDOR");
	if (vendor == NULL)
		vendor = g_udev_device_get_sysfs_attr (device, "manufacturer");

	/* model */
	model = g_udev_device_get_property (device, "ID_MODEL_FROM_DATABASE");
	if (model == NULL)
		model = g_udev_device_get_property (device, "ID_MODEL");
	if (model == NULL)
		model = g_udev_device_get_sysfs_attr (device, "product");

	/* try to get type */
	kind_str = g_udev_device_get_property (device, "GCM_KIND");
	colorimeter_kind = gcm_sensor_kind_from_string (kind_str);
	if (colorimeter_kind == GCM_SENSOR_KIND_UNKNOWN)
		egg_warning ("Failed to recognize color device: %s", model);

	g_object_set (priv->sensor,
		      "vendor", vendor,
		      "model", model,
		      "supports-display", g_udev_device_get_property_as_boolean (device, "GCM_TYPE_DISPLAY"),
		      "supports-projector",g_udev_device_get_property_as_boolean (device, "GCM_TYPE_PROJECTOR"),
		      "supports-printer", g_udev_device_get_property_as_boolean (device, "GCM_TYPE_PRINTER"),
		      "supports-spot", g_udev_device_get_property_as_boolean (device, "GCM_TYPE_SPOT"),
		      "kind", colorimeter_kind,
		      NULL);

	/* signal the addition */
	egg_debug ("emit: changed");
	g_signal_emit (colorimeter, signals[SIGNAL_CHANGED], 0);
out:
	return ret;
}

/**
 * gcm_colorimeter_device_remove:
 **/
static gboolean
gcm_colorimeter_device_remove (GcmColorimeter *colorimeter, GUdevDevice *device)
{
	gboolean ret;
	GcmColorimeterPrivate *priv = colorimeter->priv;

	/* interesting device? */
	ret = g_udev_device_get_property_as_boolean (device, "GCM_COLORIMETER");
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
	g_signal_emit (colorimeter, signals[SIGNAL_CHANGED], 0);
out:
	return ret;
}

/**
 * gcm_colorimeter_coldplug:
 **/
static gboolean
gcm_colorimeter_coldplug (GcmColorimeter *colorimeter)
{
	GList *devices;
	GList *l;
	gboolean ret = FALSE;
	GcmColorimeterPrivate *priv = colorimeter->priv;

	/* get all USB devices */
	devices = g_udev_client_query_by_subsystem (priv->client, "usb");
	for (l = devices; l != NULL; l = l->next) {
		ret = gcm_colorimeter_device_add (colorimeter, l->data);
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
 * gcm_colorimeter_uevent_cb:
 **/
static void
gcm_colorimeter_uevent_cb (GUdevClient *client, const gchar *action, GUdevDevice *device, GcmColorimeter *colorimeter)
{
	egg_debug ("uevent %s", action);
	if (g_strcmp0 (action, "add") == 0) {
		gcm_colorimeter_device_add (colorimeter, device);
	} else if (g_strcmp0 (action, "remove") == 0) {
		gcm_colorimeter_device_remove (colorimeter, device);
	}
}

/**
 * gcm_colorimeter_init:
 **/
static void
gcm_colorimeter_init (GcmColorimeter *colorimeter)
{
	const gchar *subsystems[] = {"usb", NULL};

	colorimeter->priv = GCM_COLORIMETER_GET_PRIVATE (colorimeter);

	/* use GUdev to find the calibration device */
	colorimeter->priv->client = g_udev_client_new (subsystems);
	g_signal_connect (colorimeter->priv->client, "uevent",
			  G_CALLBACK (gcm_colorimeter_uevent_cb), colorimeter);

	/* coldplug */
	gcm_colorimeter_coldplug (colorimeter);
}

/**
 * gcm_colorimeter_finalize:
 **/
static void
gcm_colorimeter_finalize (GObject *object)
{
	GcmColorimeter *colorimeter = GCM_COLORIMETER (object);
	GcmColorimeterPrivate *priv = colorimeter->priv;

	g_object_unref (priv->client);
	if (priv->sensor != NULL)
		g_object_unref (priv->sensor);

	G_OBJECT_CLASS (gcm_colorimeter_parent_class)->finalize (object);
}

/**
 * gcm_colorimeter_new:
 *
 * Return value: a new GcmColorimeter object.
 **/
GcmColorimeter *
gcm_colorimeter_new (void)
{
	if (gcm_colorimeter_object != NULL) {
		g_object_ref (gcm_colorimeter_object);
	} else {
		gcm_colorimeter_object = g_object_new (GCM_TYPE_COLORIMETER, NULL);
		g_object_add_weak_pointer (gcm_colorimeter_object, &gcm_colorimeter_object);
	}
	return GCM_COLORIMETER (gcm_colorimeter_object);
}


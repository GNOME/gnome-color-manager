/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU Lesser General Public License Version 2.1
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

/**
 * SECTION:gcm-sensor
 * @short_description: Sensor object
 *
 * This object allows abstract handling of color sensors.
 */

#include "config.h"

#include <glib-object.h>

#include "gcm-sensor.h"

static void     gcm_sensor_finalize	(GObject     *object);

#define GCM_SENSOR_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GCM_TYPE_SENSOR, GcmSensorPrivate))

/**
 * GcmSensorPrivate:
 *
 * Private #GcmSensor data
 **/
struct _GcmSensorPrivate
{
	gchar				*device;
};

enum {
	PROP_0,
	PROP_DEVICE,
	PROP_LAST
};

G_DEFINE_TYPE (GcmSensor, gcm_sensor, G_TYPE_OBJECT)

/**
 * gcm_sensor_startup:
 **/
gboolean
gcm_sensor_startup (GcmSensor *sensor, GError **error)
{
	GcmSensorClass *klass = GCM_SENSOR_GET_CLASS (sensor);
	gboolean ret = FALSE;

	/* coldplug source */
	if (klass->startup == NULL) {
		g_set_error_literal (error,
				     GCM_SENSOR_ERROR,
				     GCM_SENSOR_ERROR_INTERNAL,
				     "no klass support");
		goto out;
	}

	/* proxy */
	ret = klass->startup (sensor, error);
out:
	return ret;
}

/**
 * gcm_sensor_get_ambient:
 **/
gboolean
gcm_sensor_get_ambient (GcmSensor *sensor, gdouble *value, GError **error)
{
	GcmSensorClass *klass = GCM_SENSOR_GET_CLASS (sensor);
	gboolean ret = FALSE;

	/* coldplug source */
	if (klass->get_ambient == NULL) {
		g_set_error_literal (error,
				     GCM_SENSOR_ERROR,
				     GCM_SENSOR_ERROR_INTERNAL,
				     "no klass support");
		goto out;
	}

	/* proxy */
	ret = klass->get_ambient (sensor, value, error);
out:
	return ret;
}

/**
 * gcm_sensor_sample:
 **/
gboolean
gcm_sensor_sample (GcmSensor *sensor, GcmColorXYZ *value, GError **error)
{
	GcmSensorClass *klass = GCM_SENSOR_GET_CLASS (sensor);
	gboolean ret = FALSE;

	/* coldplug source */
	if (klass->sample == NULL) {
		g_set_error_literal (error,
				     GCM_SENSOR_ERROR,
				     GCM_SENSOR_ERROR_INTERNAL,
				     "no klass support");
		goto out;
	}

	/* proxy */
	ret = klass->sample (sensor, value, error);
out:
	return ret;
}

/**
 * gcm_sensor_set_leds:
 **/
gboolean
gcm_sensor_set_leds (GcmSensor *sensor, guint8 value, GError **error)
{
	GcmSensorClass *klass = GCM_SENSOR_GET_CLASS (sensor);
	gboolean ret = FALSE;

	/* coldplug source */
	if (klass->set_leds == NULL) {
		g_set_error_literal (error,
				     GCM_SENSOR_ERROR,
				     GCM_SENSOR_ERROR_INTERNAL,
				     "no klass support");
		goto out;
	}

	/* proxy */
	ret = klass->set_leds (sensor, value, error);
out:
	return ret;
}

/**
 * gcm_sensor_get_property:
 **/
static void
gcm_sensor_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GcmSensor *sensor = GCM_SENSOR (object);
	GcmSensorPrivate *priv = sensor->priv;

	switch (prop_id) {
	case PROP_DEVICE:
		g_value_set_string (value, priv->device);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gcm_sensor_set_property:
 **/
static void
gcm_sensor_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	GcmSensor *sensor = GCM_SENSOR (object);
	GcmSensorPrivate *priv = sensor->priv;

	switch (prop_id) {
	case PROP_DEVICE:
		g_free (priv->device);
		priv->device = g_strdup (g_value_get_string (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gcm_sensor_class_init:
 **/
static void
gcm_sensor_class_init (GcmSensorClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gcm_sensor_finalize;
	object_class->get_property = gcm_sensor_get_property;
	object_class->set_property = gcm_sensor_set_property;

	/**
	 * GcmSensor:device:
	 */
	pspec = g_param_spec_string ("device", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_DEVICE, pspec);

	g_type_class_add_private (klass, sizeof (GcmSensorPrivate));
}

/**
 * gcm_sensor_init:
 **/
static void
gcm_sensor_init (GcmSensor *sensor)
{
	sensor->priv = GCM_SENSOR_GET_PRIVATE (sensor);
	sensor->priv->device = NULL;
}

/**
 * gcm_sensor_finalize:
 **/
static void
gcm_sensor_finalize (GObject *object)
{
	GcmSensor *sensor = GCM_SENSOR (object);
	GcmSensorPrivate *priv = sensor->priv;

	g_free (priv->device);

	G_OBJECT_CLASS (gcm_sensor_parent_class)->finalize (object);
}

/**
 * gcm_sensor_new:
 *
 * Return value: a new GcmSensor object.
 **/
GcmSensor *
gcm_sensor_new (void)
{
	GcmSensor *sensor;
	sensor = g_object_new (GCM_TYPE_SENSOR, NULL);
	return GCM_SENSOR (sensor);
}


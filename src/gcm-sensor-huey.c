/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
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
 * SECTION:gcm-sensor-huey
 * @short_description: functionality to talk to the HUEY colorimeter.
 *
 * This object contains all the low level logic for the HUEY hardware.
 */

#include "config.h"

#include <glib-object.h>

#include "gcm-sensor-huey.h"

static void     gcm_sensor_huey_finalize	(GObject     *object);

#define GCM_SENSOR_HUEY_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GCM_TYPE_SENSOR_HUEY, GcmSensorHueyPrivate))

/**
 * GcmSensorHueyPrivate:
 *
 * Private #GcmSensorHuey data
 **/
struct _GcmSensorHueyPrivate
{
	guint				 dave;
};

G_DEFINE_TYPE (GcmSensorHuey, gcm_sensor_huey, GCM_TYPE_SENSOR)

/**
 * gcm_sensor_huey_get_ambient:
 **/
static gboolean
gcm_sensor_huey_get_ambient (GcmSensor *sensor_, gdouble *value, GError **error)
{
//	GcmSensorHuey *sensor = GCM_SENSOR_HUEY (sensor_);
	return TRUE;
}

/**
 * gcm_sensor_huey_set_leds:
 **/
static gboolean
gcm_sensor_huey_set_leds (GcmSensor *sensor_, guint8 value, GError **error)
{
//	GcmSensorHuey *sensor = GCM_SENSOR_HUEY (sensor_);
	return TRUE;
}

/**
 * gcm_sensor_huey_sample:
 **/
static gboolean
gcm_sensor_huey_sample (GcmSensor *sensor_, GcmColorXYZ *value, GError **error)
{
//	GcmSensorHuey *sensor = GCM_SENSOR_HUEY (sensor_);
	return TRUE;
}

/**
 * gcm_sensor_huey_startup:
 **/
static gboolean
gcm_sensor_huey_startup (GcmSensor *sensor_, GError **error)
{
//	GcmSensorHuey *sensor = GCM_SENSOR_HUEY (sensor_);
	return TRUE;
}

/**
 * gcm_sensor_huey_class_init:
 **/
static void
gcm_sensor_huey_class_init (GcmSensorHueyClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GcmSensorClass *parent_class = GCM_SENSOR_CLASS (klass);
	object_class->finalize = gcm_sensor_huey_finalize;

	/* setup klass links */
	parent_class->get_ambient = gcm_sensor_huey_get_ambient;
	parent_class->set_leds = gcm_sensor_huey_set_leds;
	parent_class->sample = gcm_sensor_huey_sample;
	parent_class->startup = gcm_sensor_huey_startup;

	g_type_class_add_private (klass, sizeof (GcmSensorHueyPrivate));
}

/**
 * gcm_sensor_huey_init:
 **/
static void
gcm_sensor_huey_init (GcmSensorHuey *sensor)
{
	GcmSensorHueyPrivate *priv;

	priv = sensor->priv = GCM_SENSOR_HUEY_GET_PRIVATE (sensor);

}

/**
 * gcm_sensor_huey_finalize:
 **/
static void
gcm_sensor_huey_finalize (GObject *object)
{
//	GcmSensorHuey *sensor = GCM_SENSOR_HUEY (object);
//	GcmSensorHueyPrivate *priv = sensor->priv;

	G_OBJECT_CLASS (gcm_sensor_huey_parent_class)->finalize (object);
}

/**
 * gcm_sensor_huey_new:
 *
 * Return value: a new GcmSensorHuey object.
 **/
GcmSensorHuey *
gcm_sensor_huey_new (void)
{
	GcmSensorHuey *sensor;
	sensor = g_object_new (GCM_TYPE_SENSOR_HUEY, NULL);
	return GCM_SENSOR_HUEY (sensor);
}


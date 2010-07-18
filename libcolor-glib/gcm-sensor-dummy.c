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
 * SECTION:gcm-sensor-dummy
 * @short_description: functionality to talk to a dummy sensor_client.
 *
 * This object contains all the low level logic for imaginary hardware.
 */

#include "config.h"

#include <glib-object.h>

#include "gcm-sensor-dummy.h"

G_DEFINE_TYPE (GcmSensorDummy, gcm_sensor_dummy, GCM_TYPE_SENSOR)

/**
 * gcm_sensor_dummy_get_ambient:
 **/
static gboolean
gcm_sensor_dummy_get_ambient (GcmSensor *sensor, gdouble *value, GError **error)
{
	*value = 17.0f;
	return TRUE;
}

/**
 * gcm_sensor_dummy_set_leds:
 **/
static gboolean
gcm_sensor_dummy_set_leds (GcmSensor *sensor, guint8 value, GError **error)
{
	return TRUE;
}

/**
 * gcm_sensor_dummy_sample:
 **/
static gboolean
gcm_sensor_dummy_sample (GcmSensor *sensor, GcmColorXYZ *value, GError **error)
{
	value->X = 17.0f;
	value->Y = 18.0f;
	value->Z = 19.0f;
	return TRUE;
}

/**
 * gcm_sensor_dummy_startup:
 **/
static gboolean
gcm_sensor_dummy_startup (GcmSensor *sensor, GError **error)
{
	return TRUE;
}

/**
 * gcm_sensor_dummy_class_init:
 **/
static void
gcm_sensor_dummy_class_init (GcmSensorDummyClass *klass)
{
	GcmSensorClass *parent_class = GCM_SENSOR_CLASS (klass);

	/* setup klass links */
	parent_class->get_ambient = gcm_sensor_dummy_get_ambient;
	parent_class->set_leds = gcm_sensor_dummy_set_leds;
	parent_class->sample = gcm_sensor_dummy_sample;
	parent_class->startup = gcm_sensor_dummy_startup;
}

/**
 * gcm_sensor_dummy_init:
 **/
static void
gcm_sensor_dummy_init (GcmSensorDummy *sensor)
{
}

/**
 * gcm_sensor_dummy_new:
 *
 * Return value: a new #GcmSensor object.
 **/
GcmSensor *
gcm_sensor_dummy_new (void)
{
	GcmSensorDummy *sensor;
	sensor = g_object_new (GCM_TYPE_SENSOR_DUMMY, NULL);
	return GCM_SENSOR (sensor);
}


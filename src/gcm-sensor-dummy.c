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
 * @short_description: Userspace driver for a dummy sensor.
 *
 * This object contains all the low level logic for imaginary hardware.
 */

#include "config.h"

#include <glib-object.h>

#include "gcm-sensor-dummy.h"

G_DEFINE_TYPE (GcmSensorDummy, gcm_sensor_dummy, GCM_TYPE_SENSOR)

/* async state for the sensor readings */
typedef struct {
	gboolean		 ret;
	gdouble			 ambient_value;
	GcmColorXYZ		*sample;
	GSimpleAsyncResult	*res;
	GcmSensor		*sensor;
} GcmSensorAsyncState;

/**
 * gcm_sensor_dummy_get_ambient_state_finish:
 **/
static void
gcm_sensor_dummy_get_ambient_state_finish (GcmSensorAsyncState *state, const GError *error)
{
	gdouble *result;

	/* set result to temp memory location */
	if (state->ret) {
		result = g_new0 (gdouble, 1);
		*result = state->ambient_value;
		g_simple_async_result_set_op_res_gpointer (state->res, result, (GDestroyNotify) g_free);
	} else {
		g_simple_async_result_set_from_error (state->res, error);
	}

	/* complete */
	g_simple_async_result_complete_in_idle (state->res);

	g_object_unref (state->res);
	g_object_unref (state->sensor);
	g_slice_free (GcmSensorAsyncState, state);
}

/**
 * gcm_sensor_dummy_get_ambient_small_wait_cb:
 **/
static gboolean
gcm_sensor_dummy_get_ambient_small_wait_cb (GcmSensorAsyncState *state)
{
	state->ret = TRUE;
	state->ambient_value = 7.7f;

	/* just return without a problem */
	gcm_sensor_dummy_get_ambient_state_finish (state, NULL);
	return FALSE;
}

/**
 * gcm_sensor_dummy_get_ambient_async:
 **/
static void
gcm_sensor_dummy_get_ambient_async (GcmSensor *sensor, GCancellable *cancellable, GAsyncResult *res)
{
	GcmSensorAsyncState *state;

	g_return_if_fail (GCM_IS_SENSOR (sensor));
	g_return_if_fail (res != NULL);

	/* save state */
	state = g_slice_new0 (GcmSensorAsyncState);
	state->res = g_object_ref (res);
	state->sensor = g_object_ref (sensor);

	/* just complete in idle */
	g_timeout_add_seconds (2, (GSourceFunc) gcm_sensor_dummy_get_ambient_small_wait_cb, state);
}

/**
 * gcm_sensor_dummy_get_ambient_finish:
 **/
static gboolean
gcm_sensor_dummy_get_ambient_finish (GcmSensor *sensor, GAsyncResult *res, gdouble *value, GError **error)
{
	GSimpleAsyncResult *simple;
	gboolean ret = TRUE;

	g_return_val_if_fail (GCM_IS_SENSOR (sensor), FALSE);
	g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (res), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* failed */
	simple = G_SIMPLE_ASYNC_RESULT (res);
	if (g_simple_async_result_propagate_error (simple, error)) {
		ret = FALSE;
		goto out;
	}

	/* grab detail */
	if (value != NULL)
		*value = *((gdouble*) g_simple_async_result_get_op_res_gpointer (simple));
out:
	return ret;
}

/**
 * gcm_sensor_dummy_sample_state_finish:
 **/
static void
gcm_sensor_dummy_sample_state_finish (GcmSensorAsyncState *state, const GError *error)
{
	GcmColorXYZ *result;

	/* set result to temp memory location */
	if (state->ret) {
		result = g_new0 (GcmColorXYZ, 1);
		gcm_color_copy_XYZ (state->sample, result);
		g_simple_async_result_set_op_res_gpointer (state->res, result, (GDestroyNotify) g_free);
	} else {
		g_simple_async_result_set_from_error (state->res, error);
	}

	/* complete */
	g_simple_async_result_complete_in_idle (state->res);

	g_object_unref (state->res);
	g_object_unref (state->sensor);
	g_free (state->sample);
	g_slice_free (GcmSensorAsyncState, state);
}

/**
 * gcm_sensor_dummy_sample_small_wait_cb:
 **/
static gboolean
gcm_sensor_dummy_sample_small_wait_cb (GcmSensorAsyncState *state)
{
	state->ret = TRUE;
	state->sample->X = 0.1;
	state->sample->Y = 0.2;
	state->sample->Z = 0.3;

	/* emulate */
	gcm_sensor_button_pressed (state->sensor);

	/* just return without a problem */
	gcm_sensor_dummy_sample_state_finish (state, NULL);
	return FALSE;
}

/**
 * gcm_sensor_dummy_sample_async:
 **/
static void
gcm_sensor_dummy_sample_async (GcmSensor *sensor, GCancellable *cancellable, GAsyncResult *res)
{
	GcmSensorAsyncState *state;

	g_return_if_fail (GCM_IS_SENSOR (sensor));
	g_return_if_fail (res != NULL);

	/* save state */
	state = g_slice_new0 (GcmSensorAsyncState);
	state->res = g_object_ref (res);
	state->sensor = g_object_ref (sensor);
	state->sample = g_new0 (GcmColorXYZ, 1);

	/* just complete in idle */
	g_timeout_add_seconds (2, (GSourceFunc) gcm_sensor_dummy_sample_small_wait_cb, state);
}

/**
 * gcm_sensor_dummy_sample_finish:
 **/
static gboolean
gcm_sensor_dummy_sample_finish (GcmSensor *sensor, GAsyncResult *res, GcmColorXYZ *value, GError **error)
{
	GSimpleAsyncResult *simple;
	gboolean ret = TRUE;

	g_return_val_if_fail (GCM_IS_SENSOR (sensor), FALSE);
	g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (res), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* failed */
	simple = G_SIMPLE_ASYNC_RESULT (res);
	if (g_simple_async_result_propagate_error (simple, error)) {
		ret = FALSE;
		goto out;
	}

	/* grab detail */
	if (value != NULL)
		gcm_color_copy_XYZ ((GcmColorXYZ*) g_simple_async_result_get_op_res_gpointer (simple), value);
out:
	return ret;
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
 * gcm_sensor_dummy_class_init:
 **/
static void
gcm_sensor_dummy_class_init (GcmSensorDummyClass *klass)
{
	GcmSensorClass *parent_class = GCM_SENSOR_CLASS (klass);

	/* setup klass links */
	parent_class->get_ambient_async = gcm_sensor_dummy_get_ambient_async;
	parent_class->get_ambient_finish = gcm_sensor_dummy_get_ambient_finish;
	parent_class->sample_async = gcm_sensor_dummy_sample_async;
	parent_class->sample_finish = gcm_sensor_dummy_sample_finish;
	parent_class->set_leds = gcm_sensor_dummy_set_leds;
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
	sensor = g_object_new (GCM_TYPE_SENSOR_DUMMY,
			       "native", TRUE,
			       NULL);
	return GCM_SENSOR (sensor);
}


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
 * SECTION:gcm-sensor-huey
 * @short_description: Userspace driver for the HUEY colorimeter.
 *
 * This object contains all the low level logic for the HUEY hardware.
 */

#include "config.h"

#include <glib-object.h>
#include <libusb-1.0/libusb.h>

#include "gcm-buffer.h"
#include "gcm-usb.h"
#include "gcm-math.h"
#include "gcm-compat.h"
#include "gcm-sensor-huey.h"
#include "gcm-sensor-huey-private.h"

static void     gcm_sensor_huey_finalize	(GObject     *object);

#define GCM_SENSOR_HUEY_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GCM_TYPE_SENSOR_HUEY, GcmSensorHueyPrivate))

/**
 * GcmSensorHueyPrivate:
 *
 * Private #GcmSensorHuey data
 **/
struct _GcmSensorHueyPrivate
{
	gboolean			 done_startup;
	GcmUsb				*usb;
	GcmMat3x3			 calibration_lcd;
	GcmMat3x3			 calibration_crt;
	gfloat				 calibration_value;
	GcmVec3				 dark_offset;
	gchar				 unlock_string[5];
};

/* async state for the sensor readings */
typedef struct {
	gboolean			 ret;
	gdouble				 ambient_value;
	gulong				 cancellable_id;
	GCancellable			*cancellable;
	GSimpleAsyncResult		*res;
	GcmSensor			*sensor;
} GcmSensorAsyncState;

static gboolean gcm_sensor_huey_startup (GcmSensor *sensor, GError **error);

G_DEFINE_TYPE (GcmSensorHuey, gcm_sensor_huey, GCM_TYPE_SENSOR)

#define HUEY_CONTROL_MESSAGE_TIMEOUT	50000 /* ms */
#define HUEY_MAX_READ_RETRIES		5

/* fudge factor to convert the value of GCM_SENSOR_HUEY_COMMAND_GET_AMBIENT to Lux */
#define HUEY_AMBIENT_UNITS_TO_LUX	125.0f

/* this is the same approx ratio as argyll uses to find the best accuracy
 * whilst maintaining a fast read. We scale each RGB value seporately. */
#define HUEY_PRECISION_TIME_VALUE		3500

/* Picked out of thin air, just to try to match reality...
 * I have no idea why we need to do this, although it probably
 * indicates we doing something wrong. */
#define HUEY_XYZ_POST_MULTIPLY_SCALE_FACTOR	3.347250f

/**
 * gcm_sensor_huey_print_data:
 **/
static void
gcm_sensor_huey_print_data (const gchar *title, const guchar *data, gsize length)
{
	guint i;

	if (g_strcmp0 (title, "request") == 0)
		g_print ("%c[%dm", 0x1B, 31);
	if (g_strcmp0 (title, "reply") == 0)
		g_print ("%c[%dm", 0x1B, 34);
	g_print ("%s\t", title);

	for (i=0; i< length; i++)
		g_print ("%02x [%c]\t", data[i], g_ascii_isprint (data[i]) ? data[i] : '?');

	g_print ("%c[%dm\n", 0x1B, 0);
}

/**
 * gcm_sensor_huey_send_data:
 **/
static gboolean
gcm_sensor_huey_send_data (GcmSensorHuey *sensor_huey,
			   const guchar *request, gsize request_len,
			   guchar *reply, gsize reply_len,
			   gsize *reply_read, GError **error)
{
	gint retval;
	gboolean ret = FALSE;
	guint i;
	gint reply_read_raw;
	libusb_device_handle *handle;

	g_return_val_if_fail (request != NULL, FALSE);
	g_return_val_if_fail (request_len != 0, FALSE);
	g_return_val_if_fail (reply != NULL, FALSE);
	g_return_val_if_fail (reply_len != 0, FALSE);
	g_return_val_if_fail (reply_read != NULL, FALSE);

	/* show what we've got */
	gcm_sensor_huey_print_data ("request", request, request_len);

	/* do sync request */
	handle = gcm_usb_get_device_handle (sensor_huey->priv->usb);
	retval = libusb_control_transfer (handle,
					  LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
					  0x09, 0x0200, 0,
					  (guchar *) request, request_len,
					  HUEY_CONTROL_MESSAGE_TIMEOUT);
	if (retval < 0) {
		g_set_error (error, GCM_SENSOR_ERROR,
			     GCM_SENSOR_ERROR_INTERNAL,
			     "failed to send request: %s", libusb_strerror (retval));
		goto out;
	}

	/* some commands need to retry the read */
	for (i=0; i<HUEY_MAX_READ_RETRIES; i++) {

		/* get sync response */
		retval = libusb_interrupt_transfer (handle, 0x81,
						    reply, (gint) reply_len, &reply_read_raw,
						    HUEY_CONTROL_MESSAGE_TIMEOUT);
		if (retval < 0) {
			g_set_error (error, GCM_SENSOR_ERROR,
				     GCM_SENSOR_ERROR_INTERNAL,
				     "failed to get reply: %s", libusb_strerror (retval));
			goto out;
		}

		/* on 64bit we can't just cast a gsize pointer to a
		 * gint pointer, we have to copy */
		*reply_read = reply_read_raw;

		/* show what we've got */
		gcm_sensor_huey_print_data ("reply", reply, *reply_read);

		/* the second byte seems to be the command again */
		if (reply[1] != request[0]) {
			g_set_error (error, GCM_SENSOR_ERROR,
				     GCM_SENSOR_ERROR_INTERNAL,
				     "wrong command reply, got 0x%02x, expected 0x%02x", reply[1], request[0]);
			goto out;
		}

		/* the first byte is status */
		if (reply[0] == GCM_SENSOR_HUEY_RETURN_SUCCESS) {
			ret = TRUE;
			break;
		}

		/* failure, the return buffer is set to "Locked" */
		if (reply[0] == GCM_SENSOR_HUEY_RETURN_LOCKED) {
			g_set_error_literal (error, GCM_SENSOR_ERROR,
					     GCM_SENSOR_ERROR_INTERNAL,
					     "the device is locked");
			goto out;
		}

		/* failure, the return buffer is set to "NoCmd" */
		if (reply[0] == GCM_SENSOR_HUEY_RETURN_ERROR) {
			g_set_error (error, GCM_SENSOR_ERROR,
				     GCM_SENSOR_ERROR_INTERNAL,
				     "failed to issue command: %s", &reply[2]);
			goto out;
		}

		/* we ignore retry */
		if (reply[0] != GCM_SENSOR_HUEY_RETURN_RETRY) {
			g_set_error (error, GCM_SENSOR_ERROR,
				     GCM_SENSOR_ERROR_INTERNAL,
				     "return value unknown: 0x%02x", reply[0]);
			goto out;
		}
	}

	/* no success */
	if (!ret) {
		g_set_error (error, GCM_SENSOR_ERROR,
			     GCM_SENSOR_ERROR_INTERNAL,
			     "gave up retrying after %i reads", HUEY_MAX_READ_RETRIES);
		goto out;
	}
out:
	return ret;
}

/**
 * gcm_sensor_huey_read_register_byte:
 **/
static gboolean
gcm_sensor_huey_read_register_byte (GcmSensorHuey *sensor_huey, guint8 addr, guint8 *value, GError **error)
{
	guchar request[] = { GCM_SENSOR_HUEY_COMMAND_REGISTER_READ, 0xff, 0x00, 0x10, 0x3c, 0x06, 0x00, 0x00 };
	guchar reply[8];
	gboolean ret;
	gsize reply_read;

	/* hit hardware */
	request[1] = addr;
	ret = gcm_sensor_huey_send_data (sensor_huey, request, 8, reply, 8, &reply_read, error);
	if (!ret)
		goto out;
	*value = reply[3];
out:
	return ret;
}

/**
 * gcm_sensor_huey_read_register_string:
 **/
static gboolean
gcm_sensor_huey_read_register_string (GcmSensorHuey *huey, guint8 addr, gchar *value, gsize len, GError **error)
{
	guint8 i;
	gboolean ret = TRUE;

	/* get each byte of the string */
	for (i=0; i<len; i++) {
		ret = gcm_sensor_huey_read_register_byte (huey, addr+i, (guint8*) &value[i], error);
		if (!ret)
			goto out;
	}
out:
	return ret;
}

/**
 * gcm_sensor_huey_read_register_word:
 **/
static gboolean
gcm_sensor_huey_read_register_word (GcmSensorHuey *huey, guint8 addr, guint32 *value, GError **error)
{
	guint8 i;
	guint8 tmp[4];
	gboolean ret = TRUE;

	/* get each byte of the 32 bit number */
	for (i=0; i<4; i++) {
		ret = gcm_sensor_huey_read_register_byte (huey, addr+i, tmp+i, error);
		if (!ret)
			goto out;
	}

	/* convert to a 32 bit integer */
	*value = gcm_buffer_read_uint32_be (tmp);
out:
	return ret;
}

/**
 * gcm_sensor_huey_read_register_float:
 **/
static gboolean
gcm_sensor_huey_read_register_float (GcmSensorHuey *huey, guint8 addr, gfloat *value, GError **error)
{
	gboolean ret;
	guint32 tmp;

	/* first read in 32 bit integer */
	ret = gcm_sensor_huey_read_register_word (huey, addr, &tmp, error);
	if (!ret)
		goto out;

	/* convert to float */
	*((guint32 *)value) = tmp;
out:
	return ret;
}

/**
 * gcm_sensor_huey_read_register_vector:
 **/
static gboolean
gcm_sensor_huey_read_register_vector (GcmSensorHuey *huey, guint8 addr, GcmVec3 *value, GError **error)
{
	gboolean ret = TRUE;
	guint i;
	gfloat tmp;
	gdouble *vector_data;

	/* get this to avoid casting */
	vector_data = gcm_vec3_get_data (value);

	/* read in vec3 */
	for (i=0; i<3; i++) {
		ret = gcm_sensor_huey_read_register_float (huey, addr + (i*4), &tmp, error);
		if (!ret)
			goto out;

		/* save in matrix */
		*(vector_data+i) = tmp;
	}
out:
	return ret;
}

/**
 * gcm_sensor_huey_read_register_matrix:
 **/
static gboolean
gcm_sensor_huey_read_register_matrix (GcmSensorHuey *huey, guint8 addr, GcmMat3x3 *value, GError **error)
{
	gboolean ret = TRUE;
	guint i;
	gfloat tmp;
	gdouble *matrix_data;

	/* get this to avoid casting */
	matrix_data = gcm_mat33_get_data (value);

	/* read in 3d matrix */
	for (i=0; i<9; i++) {
		ret = gcm_sensor_huey_read_register_float (huey, addr + (i*4), &tmp, error);
		if (!ret)
			goto out;

		/* save in matrix */
		*(matrix_data+i) = tmp;
	}
out:
	return ret;
}

#if 0
/**
 * gcm_sensor_huey_get_ambient_state_finish:
 **/
static void
gcm_sensor_huey_get_ambient_state_finish (GcmSensorAsyncState *state, const GError *error)
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

	/* deallocate */
	if (state->cancellable != NULL) {
		g_cancellable_disconnect (state->cancellable, state->cancellable_id);
		g_object_unref (state->cancellable);
	}
	g_object_unref (state->res);
	g_object_unref (state->sensor);
	g_slice_free (GcmSensorAsyncState, state);
}
#endif

/**
 * gcm_sensor_huey_cancellable_cancel_cb:
 **/
static void
gcm_sensor_huey_cancellable_cancel_cb (GCancellable *cancellable, GcmSensorAsyncState *state)
{
	g_warning ("cancelled huey");
}

static void
gcm_sensor_huey_get_ambient_thread_cb (GSimpleAsyncResult *res, GObject *object, GCancellable *cancellable)
{
	GcmSensor *sensor = GCM_SENSOR (object);
	gdouble *result;
	GError *error = NULL;
	guchar reply[8];
	gboolean ret = FALSE;
	gsize reply_read;
	GcmSensorOutputType output_type;
	guchar request[] = { GCM_SENSOR_HUEY_COMMAND_GET_AMBIENT, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
	GcmSensorHuey *sensor_huey = GCM_SENSOR_HUEY (sensor);

	/* ensure sensor is started */
	if (!sensor_huey->priv->done_startup) {
		ret = gcm_sensor_huey_startup (sensor, &error);
		if (!ret) {
			g_simple_async_result_set_from_error (res, error);
			g_error_free (error);
			goto out;
		}
	}

	/* no hardware support */
	if (gcm_sensor_get_output_type (sensor) == GCM_SENSOR_OUTPUT_TYPE_PROJECTOR) {
		g_set_error_literal (&error, GCM_SENSOR_ERROR,
				     GCM_SENSOR_ERROR_NO_SUPPORT,
				     "HUEY cannot measure ambient light in projector mode");
		g_simple_async_result_set_from_error (res, error);
		g_error_free (error);
		goto out;
	}

	/* ensure the user set this */
	output_type = gcm_sensor_get_output_type (sensor);
	if (output_type == GCM_SENSOR_OUTPUT_TYPE_UNKNOWN) {
		g_set_error_literal (&error, GCM_SENSOR_ERROR,
				     GCM_SENSOR_ERROR_INTERNAL,
				     "output sensor type was not set");
		g_simple_async_result_set_from_error (res, error);
		g_error_free (error);
		goto out;
	}

	/* set state */
	gcm_sensor_set_state (sensor, GCM_SENSOR_STATE_MEASURING);

	/* hit hardware */
	request[2] = (output_type == GCM_SENSOR_OUTPUT_TYPE_LCD) ? 0x00 : 0x02;
	ret = gcm_sensor_huey_send_data (sensor_huey, request, 8, reply, 8, &reply_read, &error);
	if (!ret) {
		g_simple_async_result_set_from_error (res, error);
		g_error_free (error);
		goto out;
	}

	/* parse the value */
	result = g_new0 (gdouble, 1);
	*result = (gdouble) gcm_buffer_read_uint16_be (reply+5) / HUEY_AMBIENT_UNITS_TO_LUX;
	g_simple_async_result_set_op_res_gpointer (res, result, (GDestroyNotify) g_free);
out:
	/* set state */
	gcm_sensor_set_state (sensor, GCM_SENSOR_STATE_IDLE);
}

/**
 * gcm_sensor_huey_get_ambient_async:
 **/
static void
gcm_sensor_huey_get_ambient_async (GcmSensor *sensor, GCancellable *cancellable, GAsyncResult *res)
{
	GcmSensorAsyncState *state;

	g_return_if_fail (GCM_IS_SENSOR (sensor));
	g_return_if_fail (res != NULL);

	/* save state */
	state = g_slice_new0 (GcmSensorAsyncState);
	state->res = g_object_ref (res);
	state->sensor = g_object_ref (sensor);
	if (cancellable != NULL) {
		state->cancellable = g_object_ref (cancellable);
		state->cancellable_id = g_cancellable_connect (cancellable, G_CALLBACK (gcm_sensor_huey_cancellable_cancel_cb), state, NULL);
	}

	/* run in a thread */
	g_simple_async_result_run_in_thread (G_SIMPLE_ASYNC_RESULT (res), gcm_sensor_huey_get_ambient_thread_cb, 0, cancellable);
}

/**
 * gcm_sensor_huey_get_ambient_finish:
 **/
static gboolean
gcm_sensor_huey_get_ambient_finish (GcmSensor *sensor, GAsyncResult *res, gdouble *value, GError **error)
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
 * gcm_sensor_huey_set_leds:
 **/
static gboolean
gcm_sensor_huey_set_leds (GcmSensor *sensor, guint8 value, GError **error)
{
	guchar reply[8];
	gsize reply_read;
	GcmSensorHuey *sensor_huey = GCM_SENSOR_HUEY (sensor);
	guchar payload[] = { GCM_SENSOR_HUEY_COMMAND_SET_LEDS, 0x00, ~value, 0x00, 0x00, 0x00, 0x00, 0x00 };
	return gcm_sensor_huey_send_data (sensor_huey, payload, 8, reply, 8, &reply_read, error);
}

typedef struct {
	guint16	R;
	guint16	G;
	guint16	B;
} GcmSensorHueyMultiplier;

/**
 * gcm_sensor_huey_sample_for_threshold:
 **/
static gboolean
gcm_sensor_huey_sample_for_threshold (GcmSensorHuey *sensor_huey, GcmSensorHueyMultiplier *threshold, GcmColorRGB *values, GError **error)
{
	guchar request[] = { GCM_SENSOR_HUEY_COMMAND_SENSOR_MEASURE_RGB, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
	guchar reply[8];
	gboolean ret;
	gsize reply_read;

	/* these are 16 bit gain values */
	gcm_buffer_write_uint16_be (request + 1, threshold->R);
	gcm_buffer_write_uint16_be (request + 3, threshold->G);
	gcm_buffer_write_uint16_be (request + 5, threshold->B);

	/* measure, and get red */
	ret = gcm_sensor_huey_send_data (sensor_huey, request, 8, reply, 8, &reply_read, error);
	if (!ret)
		goto out;

	/* get value */
	values->R = (gdouble) threshold->R / (gdouble)gcm_buffer_read_uint16_be (reply+3);

	/* get green */
	request[0] = GCM_SENSOR_HUEY_COMMAND_READ_GREEN;
	ret = gcm_sensor_huey_send_data (sensor_huey, request, 8, reply, 8, &reply_read, error);
	if (!ret)
		goto out;

	/* get value */
	values->G = (gdouble) threshold->G / (gdouble)gcm_buffer_read_uint16_be (reply+3);

	/* get blue */
	request[0] = GCM_SENSOR_HUEY_COMMAND_READ_BLUE;
	ret = gcm_sensor_huey_send_data (sensor_huey, request, 8, reply, 8, &reply_read, error);
	if (!ret)
		goto out;

	/* get value */
	values->B = (gdouble) threshold->B / (gdouble)gcm_buffer_read_uint16_be (reply+3);
out:
	return ret;
}

/**
 * gcm_sensor_huey_convert_device_RGB_to_XYZ:
 *
 * / X \   (( / R \             )   / d \    / c a l \ )
 * | Y | = (( | G | x pre-scale ) - | r |  * | m a t | ) x post_scale
 * \ Z /   (( \ B /             )   \ k /    \ l c d / )
 *
 * The device RGB values have to be scaled to something in the same
 * scale as the dark calibration. The results then have to be scaled
 * after convolving. I assume the first is a standard value, and the
 * second scale must be available in the eeprom somewhere.
 **/
static void
gcm_sensor_huey_convert_device_RGB_to_XYZ (GcmColorRGB *src, GcmColorXYZ *dest,
					   GcmMat3x3 *calibration, GcmVec3 *dark_offset,
					   gdouble pre_scale, gdouble post_scale)
{
	GcmVec3 *color_native_vec3;
	GcmVec3 *color_result_vec3;
	GcmVec3 temp;

	/* pre-multiply */
	color_native_vec3 = gcm_color_get_RGB_Vec3 (src);
	gcm_vec3_scalar_multiply (color_native_vec3, pre_scale, &temp);

	/* remove dark calibration */
	gcm_vec3_subtract (&temp, dark_offset, &temp);

	/* convolve */
	color_result_vec3 = gcm_color_get_XYZ_Vec3 (dest);
	gcm_mat33_vector_multiply (calibration, &temp, color_result_vec3);

	/* post-multiply */
	gcm_vec3_scalar_multiply (color_result_vec3, post_scale, color_result_vec3);
}

static void
gcm_sensor_huey_sample_thread_cb (GSimpleAsyncResult *res, GObject *object, GCancellable *cancellable)
{
	GcmSensor *sensor = GCM_SENSOR (object);
	GError *error = NULL;
	gboolean ret = FALSE;
	gdouble precision_value;
	GcmColorRGB color_native;
	GcmColorXYZ color_result;
	GcmColorXYZ *tmp;
	GcmSensorHueyMultiplier multiplier;
	GcmSensorHuey *sensor_huey = GCM_SENSOR_HUEY (sensor);
	GcmMat3x3 *device_calibration;
	GcmSensorOutputType output_type;

	/* ensure sensor is started */
	if (!sensor_huey->priv->done_startup) {
		ret = gcm_sensor_huey_startup (sensor, &error);
		if (!ret) {
			g_simple_async_result_set_from_error (res, error);
			g_error_free (error);
			goto out;
		}
	}

	/* no hardware support */
	output_type = gcm_sensor_get_output_type (sensor);
	if (output_type == GCM_SENSOR_OUTPUT_TYPE_PROJECTOR) {
		g_set_error_literal (&error, GCM_SENSOR_ERROR,
				     GCM_SENSOR_ERROR_NO_SUPPORT,
				     "HUEY cannot measure ambient light in projector mode");
		g_simple_async_result_set_from_error (res, error);
		g_error_free (error);
		goto out;
	}

	/* set state */
	gcm_sensor_set_state (sensor, GCM_SENSOR_STATE_MEASURING);

	/* set this to one value for a quick approximate value */
	multiplier.R = 1;
	multiplier.G = 1;
	multiplier.B = 1;
	ret = gcm_sensor_huey_sample_for_threshold (sensor_huey, &multiplier, &color_native, &error);
	if (!ret) {
		g_simple_async_result_set_from_error (res, error);
		g_error_free (error);
		goto out;
	}
	g_debug ("initial values: red=%0.6lf, green=%0.6lf, blue=%0.6lf", color_native.R, color_native.G, color_native.B);

	/* compromise between the amount of time and the precision */
	precision_value = (gdouble) HUEY_PRECISION_TIME_VALUE;
	multiplier.R = precision_value * color_native.R;
	multiplier.G = precision_value * color_native.G;
	multiplier.B = precision_value * color_native.B;

	/* don't allow a value of zero */
	if (multiplier.R == 0)
		multiplier.R = 1;
	if (multiplier.G == 0)
		multiplier.G = 1;
	if (multiplier.B == 0)
		multiplier.B = 1;
	g_debug ("using multiplier factor: red=%i, green=%i, blue=%i", multiplier.R, multiplier.G, multiplier.B);
	ret = gcm_sensor_huey_sample_for_threshold (sensor_huey, &multiplier, &color_native, &error);
	if (!ret) {
		g_simple_async_result_set_from_error (res, error);
		g_error_free (error);
		goto out;
	}

	g_debug ("scaled values: red=%0.6lf, green=%0.6lf, blue=%0.6lf", color_native.R, color_native.G, color_native.B);

	/* we use different calibration matrices for each output type */
	if (output_type == GCM_SENSOR_OUTPUT_TYPE_LCD) {
		g_debug ("using LCD calibration matrix");
		device_calibration = &sensor_huey->priv->calibration_lcd;
	} else {
		g_debug ("using CRT calibration matrix");
		device_calibration = &sensor_huey->priv->calibration_crt;
	}

	/* convert from device RGB to XYZ */
	gcm_sensor_huey_convert_device_RGB_to_XYZ (&color_native,
						   &color_result,
						   device_calibration,
						   &sensor_huey->priv->dark_offset,
						   2000.0f,
						   HUEY_XYZ_POST_MULTIPLY_SCALE_FACTOR);

	g_debug ("finished values: red=%0.6lf, green=%0.6lf, blue=%0.6lf", color_result.X, color_result.Y, color_result.Z);

	/* save result */
	tmp = g_new0 (GcmColorXYZ, 1);
	gcm_color_copy_XYZ (&color_result, tmp);
	g_simple_async_result_set_op_res_gpointer (res, tmp, (GDestroyNotify) g_free);
out:
	/* set state */
	gcm_sensor_set_state (sensor, GCM_SENSOR_STATE_IDLE);
}

/**
 * gcm_sensor_huey_sample_async:
 **/
static void
gcm_sensor_huey_sample_async (GcmSensor *sensor, GCancellable *cancellable, GAsyncResult *res)
{
	GcmSensorAsyncState *state;

	g_return_if_fail (GCM_IS_SENSOR (sensor));
	g_return_if_fail (res != NULL);

	/* save state */
	state = g_slice_new0 (GcmSensorAsyncState);
	state->res = g_object_ref (res);
	state->sensor = g_object_ref (sensor);
	if (cancellable != NULL) {
		state->cancellable = g_object_ref (cancellable);
		state->cancellable_id = g_cancellable_connect (cancellable, G_CALLBACK (gcm_sensor_huey_cancellable_cancel_cb), state, NULL);
	}

	/* run in a thread */
	g_simple_async_result_run_in_thread (G_SIMPLE_ASYNC_RESULT (res), gcm_sensor_huey_sample_thread_cb, 0, cancellable);
}

/**
 * gcm_sensor_huey_sample_finish:
 **/
static gboolean
gcm_sensor_huey_sample_finish (GcmSensor *sensor, GAsyncResult *res, GcmColorXYZ *value, GError **error)
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
 * gcm_sensor_huey_send_unlock:
 **/
static gboolean
gcm_sensor_huey_send_unlock (GcmSensorHuey *sensor_huey, GError **error)
{
	guchar request[8];
	guchar reply[8];
	gboolean ret;
	gsize reply_read;

	request[0] = GCM_SENSOR_HUEY_COMMAND_UNLOCK;
	request[1] = 'G';
	request[2] = 'r';
	request[3] = 'M';
	request[4] = 'b';
	request[5] = 'k'; /* <- perhaps junk, need to test next time locked */
	request[6] = 'e'; /* <-         "" */
	request[7] = 'd'; /* <-         "" */

	/* no idea why the hardware gets 'locked' */
	ret = gcm_sensor_huey_send_data (sensor_huey, request, 8, reply, 8, &reply_read, error);
	if (!ret)
		goto out;
out:
	return ret;
}

/**
 * gcm_sensor_huey_startup:
 **/
static gboolean
gcm_sensor_huey_startup (GcmSensor *sensor, GError **error)
{
	gboolean ret = FALSE;
	guint i;
	guint32 serial_number;
	gchar *serial_number_tmp = NULL;
	GcmSensorHuey *sensor_huey = GCM_SENSOR_HUEY (sensor);
	GcmSensorHueyPrivate *priv = sensor_huey->priv;
	const guint8 spin_leds[] = { 0x0, 0x1, 0x2, 0x4, 0x8, 0x4, 0x2, 0x1, 0x0, 0xff };

	/* connect */
	ret = gcm_usb_connect (priv->usb,
			       GCM_SENSOR_HUEY_VENDOR_ID, GCM_SENSOR_HUEY_PRODUCT_ID,
			       0x01, 0x00, error);
	if (!ret)
		goto out;

	/* set state */
	gcm_sensor_set_state (sensor, GCM_SENSOR_STATE_STARTING);

	/* unlock */
	ret = gcm_sensor_huey_send_unlock (sensor_huey, error);
	if (!ret)
		goto out;

	/* get serial number */
	ret = gcm_sensor_huey_read_register_word (sensor_huey,
						  GCM_SENSOR_HUEY_EEPROM_ADDR_SERIAL,
						  &serial_number, error);
	if (!ret)
		goto out;
	serial_number_tmp = g_strdup_printf ("%i", serial_number);
	gcm_sensor_set_serial_number (sensor, serial_number_tmp);
	g_debug ("Serial number: %s", serial_number_tmp);

	/* get unlock string */
	ret = gcm_sensor_huey_read_register_string (sensor_huey,
						    GCM_SENSOR_HUEY_EEPROM_ADDR_UNLOCK,
						    priv->unlock_string, 5, error);
	if (!ret)
		goto out;
	g_debug ("Unlock string: %s", priv->unlock_string);

	/* get matrix */
	gcm_mat33_clear (&priv->calibration_lcd);
	ret = gcm_sensor_huey_read_register_matrix (sensor_huey,
						    GCM_SENSOR_HUEY_EEPROM_ADDR_CALIBRATION_DATA_LCD,
						    &priv->calibration_lcd, error);
	if (!ret)
		goto out;
	g_debug ("device matrix1: %s", gcm_mat33_to_string (&priv->calibration_lcd));

	/* get another matrix, although this one is different... */
	gcm_mat33_clear (&priv->calibration_crt);
	ret = gcm_sensor_huey_read_register_matrix (sensor_huey,
						    GCM_SENSOR_HUEY_EEPROM_ADDR_CALIBRATION_DATA_CRT,
						    &priv->calibration_crt, error);
	if (!ret)
		goto out;
	g_debug ("device matrix2: %s", gcm_mat33_to_string (&priv->calibration_crt));

	/* this number is different on all three hueys */
	ret = gcm_sensor_huey_read_register_float (sensor_huey,
						   GCM_SENSOR_HUEY_EEPROM_ADDR_AMBIENT_CALIB_VALUE,
						   &priv->calibration_value,
						   error);
	if (!ret)
		goto out;

	/* this vector changes between sensor 1 and 3 */
	ret = gcm_sensor_huey_read_register_vector (sensor_huey,
						    GCM_SENSOR_HUEY_EEPROM_ADDR_DARK_OFFSET,
						    &priv->dark_offset, error);
	if (!ret)
		goto out;

	/* spin the LEDs */
	for (i=0; spin_leds[i] != 0xff; i++) {
		ret = gcm_sensor_huey_set_leds (sensor, spin_leds[i], error);
		if (!ret)
			goto out;
		g_usleep (50000);
	}

	/* success */
	sensor_huey->priv->done_startup = TRUE;
out:
	/* set state */
	gcm_sensor_set_state (sensor, GCM_SENSOR_STATE_IDLE);
	g_free (serial_number_tmp);
	return ret;
}

/**
 * gcm_sensor_huey_dump:
 **/
static gboolean
gcm_sensor_huey_dump (GcmSensor *sensor, GString *data, GError **error)
{
	GcmSensorHuey *sensor_huey = GCM_SENSOR_HUEY (sensor);
	GcmSensorHueyPrivate *priv = sensor_huey->priv;
	gboolean ret;
	guint i;
	guint8 value;

	/* ensure sensor is started */
	if (!priv->done_startup) {
		ret = gcm_sensor_huey_startup (sensor, error);
		if (!ret)
			goto out;
	}

	/* dump the unlock string */
	g_string_append_printf (data, "huey-dump-version:%i\n", 2);
	g_string_append_printf (data, "unlock-string:%s\n", priv->unlock_string);
	g_string_append_printf (data, "calibration-value:%f\n", priv->calibration_value);
	g_string_append_printf (data, "dark-offset:%f,%f,%f\n",
				priv->dark_offset.v0,
				priv->dark_offset.v1,
				priv->dark_offset.v2);

	/* read all the register space */
	for (i=0; i<0xff; i++) {
		ret = gcm_sensor_huey_read_register_byte (sensor_huey, i, &value, error);
		if (!ret)
			goto out;
		/* write details */
		g_string_append_printf (data, "register[0x%02x]:0x%02x\n", i, value);
	}
out:
	return ret;
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
	parent_class->get_ambient_async = gcm_sensor_huey_get_ambient_async;
	parent_class->get_ambient_finish = gcm_sensor_huey_get_ambient_finish;
	parent_class->set_leds = gcm_sensor_huey_set_leds;
	parent_class->sample_async = gcm_sensor_huey_sample_async;
	parent_class->sample_finish = gcm_sensor_huey_sample_finish;
	parent_class->dump = gcm_sensor_huey_dump;

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
	priv->usb = gcm_usb_new ();
	gcm_mat33_clear (&priv->calibration_lcd);
	gcm_mat33_clear (&priv->calibration_crt);
	priv->unlock_string[0] = '\0';
}

/**
 * gcm_sensor_huey_finalize:
 **/
static void
gcm_sensor_huey_finalize (GObject *object)
{
	GcmSensorHuey *sensor = GCM_SENSOR_HUEY (object);
	GcmSensorHueyPrivate *priv = sensor->priv;

	g_object_unref (priv->usb);

	G_OBJECT_CLASS (gcm_sensor_huey_parent_class)->finalize (object);
}

/**
 * gcm_sensor_huey_new:
 *
 * Return value: a new #GcmSensor object.
 *
 * Since: 2.91.1
 **/
GcmSensor *
gcm_sensor_huey_new (void)
{
	GcmSensorHuey *sensor;
	sensor = g_object_new (GCM_TYPE_SENSOR_HUEY,
			       "native", TRUE,
			       "kind", GCM_SENSOR_KIND_HUEY,
			       "image-display", "huey-attach.svg",
			       NULL);
	return GCM_SENSOR (sensor);
}


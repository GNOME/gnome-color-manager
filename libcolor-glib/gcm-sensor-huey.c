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

#include "egg-debug.h"

#include "gcm-buffer.h"
#include "gcm-usb.h"
#include "gcm-common.h"
#include "gcm-compat.h"
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
	GcmUsb				*usb;
	GcmMat3x3			 calibration_matrix1;
	GcmMat3x3			 calibration_matrix2;
	gchar				 unlock_string[5];
};

G_DEFINE_TYPE (GcmSensorHuey, gcm_sensor_huey, GCM_TYPE_SENSOR)

#define HUEY_VENDOR_ID			0x0971
#define HUEY_PRODUCT_ID			0x2005
#define HUEY_CONTROL_MESSAGE_TIMEOUT	50000 /* ms */
#define HUEY_MAX_READ_RETRIES		5

#define HUEY_RETVAL_SUCCESS		0x00
#define HUEY_RETVAL_LOCKED		0xc0
#define HUEY_RETVAL_UNKNOWN_5A		0x5a /* seen in profiling */
#define HUEY_RETVAL_ERROR		0x80
#define HUEY_RETVAL_UNKNOWN_81		0x81 /* seen once in init */
#define HUEY_RETVAL_RETRY		0x90

/*
 * Get the currect status of the device
 *
 *  input:   00 00 00 00 3f 00 00 00
 * returns: 00 00 43 69 72 30 30 31  (or)
 *     "Cir001" --^^^^^^^^^^^^^^^^^ -- Circuit1?...
 *          c0 00 4c 6f 63 6b 65 64
 *     "locked" --^^^^^^^^^^^^^^^^^
 */
#define HUEY_COMMAND_GET_STATUS		0x00

/*
 * Read the green sample data
 *
 * input:   02 xx xx xx xx xx xx xx
 * returns: 00 02 00 00 0a 00 00 00 (or)
 *          00 02 00 0e c6 80 00 00
 *            data --^^^^^ ^-- only ever 00 or 80
 *                    |
 *                    \-- for RGB(00,00,00) is 09 f2
 *                            RGB(ff,ff,ff) is 00 00
 *                            RGB(ff,00,00) is 02 a5
 *                            RGB(00,ff,00) is 00 f1
 *                            RGB(00,00,ff) is 08 56
 *
 * This doesn't do a sensor read, it seems to be a simple accessor.
 * HUEY_COMMAND_SENSOR_MEASURE_RGB has to be used before this one.
 */
#define HUEY_COMMAND_READ_GREEN		0x02

/*
 * Read the blue sample data
 *
 * input:   03 xx xx xx xx xx xx xx
 * returns: 00 03 00 0f 18 00 00 00
 *            data --^^^^^ ^-- only ever 00 or 80
 *                    |
 *                    \-- for RGB(00,00,00) is 09 64
 *                            RGB(ff,ff,ff) is 08 80
 *                            RGB(ff,00,00) is 03 22
 *                            RGB(00,ff,00) is 00 58
 *                            RGB(00,00,ff) is 00 59
 *
 * This doesn't do a sensor read, it seems to be a simple accessor.
 * HUEY_COMMAND_SENSOR_MEASURE_RGB has to be used before this one.
 */
#define HUEY_COMMAND_READ_BLUE	0x03

/*
 * Set value of some 32 bit register.
 *
 * input:   05 ?? 11 12 13 14 xx xx
 * returns: 00 05 00 00 00 00 00 00
 *              ^--- always the same no matter the input
 *
 * This is never used in profiling
 */
#define HUEY_COMMAND_SET_VALUE		0x05

/*
 * Get the value of some 32 bit register.
 *
 * input:   06 xx xx xx xx xx xx xx
 * returns: 00 06 11 12 13 14 00 00
 *    4 bytes ----^^^^^^^^^^^ (from HUEY_COMMAND_SET_VALUE)
 *
 * This is some sort of 32bit register on the device.
 * The default value at plug-in is 00 0f 42 40, although during
 * profiling it is set to 00 00 6f 00 and then 00 00 61 00.
 */
#define HUEY_COMMAND_GET_VALUE		0x06

/* NEVER USED */
#define HUEY_COMMAND_UNKNOWN_07		0x07

/*
 * Reads a register value.
 *
 * (sent at startup  after the unlock)
 * input:   08 0b xx xx xx xx xx xx
 *             ^^-- register address
 * returns: 00 08 0b b8 00 00 00 00
 *      address --^^ ^^-- value
 *
 * It appears you can only ask for one byte at a time, well, if you
 * can ask for more the Windows driver seems to do this one byte at
 * at time...
 */
#define HUEY_COMMAND_REGISTER_READ	0x08

/*
 * Unlock a locked sensor.
 *
 * input:   0e 47 72 4d 62 6b 65 64
 *  "GrMbked"--^^^^^^^^^^^^^^^^^^^^
 * returns: 00 0e 00 00 00 00 00 00
 *
 * It might be only GrMbk that is needed to unlock.
 * We still don't know how to 'lock' a device, it just kinda happens.
 */
#define HUEY_COMMAND_UNLOCK		0x0e

/*
 * Unknown command
 *
 * returns: all NULL all of the time */
#define HUEY_COMMAND_UNKNOWN_0F		0x0f

/*
 * Unknown command
 *
 * Something to do with sampling */
#define HUEY_COMMAND_UNKNOWN_10		0x10

/*
 * Unknown command
 *
 * Something to do with sampling (that needs a retry with code 5a)
 */
#define HUEY_COMMAND_UNKNOWN_11		0x11

/*
 * Unknown command
 *
 * something to do with sampling
 */
#define HUEY_COMMAND_UNKNOWN_12		0x12

/*
 * Unknown command
 *
 * returns: all NULL all of the time
 */
#define HUEY_COMMAND_UNKNOWN_13		0x13

/*
 * Unknown command
 *
 * returns: seems to be sent, but not requested
 */
#define HUEY_COMMAND_UNKNOWN_15		0x15

/*
 * Sample a color and return the red component
 *
 * input:   16 00 01 00 01 00 01 00
 * returns: 00 16 00 00 00 00 00 00
 *
 * or:
 *             ,,-,,-,,-,,-,,-,,-- 'gain control'
 *             || || || || || ||
 * input:   16 00 35 00 48 00 1d 03
 * returns: 00 16 00 0b d0 00 00 00
 *            data --^^^^^ ^^-- only ever 00 or 80
 *
 * This is used when profiling, and all commands are followed by
 * HUEY_COMMAND_READ_GREEN and HUEY_COMMAND_READ_BLUE.
 * 
 * The returned values are some kind of 16 bit register count that
 * indicate how much light fell on a sensor. If the sensors are
 * converting light to pulses, then the 'gain' control tells the sensor
 * how long to read. It's therefore quicker to read white than black.
 *
 * Given there exists only GREEN and BLUE accessors, and that RED comes
 * first in a RGB sequence, I think it's safe to assume that this command
 * does the measurement, and the others just return cached data.
 *
 * argyll does (for #ff0000)
 *
 * -> 16 00 01 00 01 00 01 00
 * <-       00 00 0b 00 00 00
 * -> 02 xx xx xx xx xx xx xx
 * <-       00 00 12 00 00 00
 * -> 03 xx xx xx xx xx xx xx
 * <-       00 03 41 00 00 00
 *
 * then does:
 *
 * -> 16 01 63 00 d9 00 04 00
 * <-       00 0f ce 80 00 00
 * -> 02 xx xx xx xx xx xx xx
 * <-       00 0e d0 80 00 00
 * -> 03 xx xx xx xx xx xx xx
 * <-       00 0d 3c 00 00 00
 *
 * then returns XYZ=87.239169 45.548708 1.952249
 */
#define HUEY_COMMAND_SENSOR_MEASURE_RGB		0x16

/*
 * Unknown command (some sort of poll?)
 *
 * input:   21 09 00 02 00 00 08 00 (or)
 * returns: [never seems to return a value]
 *
 * Only when profiling, and over and over.
 */
#define HUEY_COMMAND_UNKNOWN_21		0x21

/*
 * Get the level of ambient light from the sensor
 *
 *                 ,,--- The output-type, where 00 is LCD and 02 is CRT
 *  input:   17 03 00 xx xx xx xx xx
 * returns: 90 17 03 00 00 00 00 00  then on second read:
 * 	    00 17 03 00 00 62 57 00 in light (or)
 * 	    00 17 03 00 00 00 08 00 in dark
 * 	no idea	--^^       ^---^ = 16bits data?
 */
#define HUEY_COMMAND_GET_AMBIENT	0x17

/*
 * Set the LEDs on the sensor
 *
 * input:   18 00 f0 xx xx xx xx xx
 * returns: 00 18 f0 00 00 00 00 00
 *   led mask ----^^
 */
#define HUEY_COMMAND_SET_LEDS		0x18

/*
 * Unknown command
 *
 * returns: all NULL for NULL input: times out for f1 f2 f3 f4 f5 f6 f7 f8 */
#define HUEY_COMMAND_UNKNOWN_19		0x19

/* fudge factor to convert the value of HUEY_COMMAND_GET_AMBIENT to Lux */
#define HUEY_AMBIENT_UNITS_TO_LUX	125.0f

/* this is the same approx ratio as argyll uses to find the best accuracy
 * whilst maintaining a fast read. We scale each RGB value seporately. */
#define HUEY_PRECISION_TIME_VALUE		3500

/* Picked out of thin air, just to try to match reality...
 * I have no idea why we need to do this, although it probably
 * indicates we doing something wrong. */
#define HUEY_XYZ_POST_MULTIPLY_SCALE_FACTOR	6880.0f

/**
 * gcm_sensor_huey_print_data:
 **/
static void
gcm_sensor_huey_print_data (const gchar *title, const guchar *data, gsize length)
{
	guint i;

	if (!egg_debug_is_verbose ())
		return;

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
						    reply, (gint) reply_len, (gint*)reply_read,
						    HUEY_CONTROL_MESSAGE_TIMEOUT);
		if (retval < 0) {
			g_set_error (error, GCM_SENSOR_ERROR,
				     GCM_SENSOR_ERROR_INTERNAL,
				     "failed to get reply: %s", libusb_strerror (retval));
			goto out;
		}

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
		if (reply[0] == HUEY_RETVAL_SUCCESS) {
			ret = TRUE;
			break;
		}

		/* failure, the return buffer is set to "Locked" */
		if (reply[0] == HUEY_RETVAL_LOCKED) {
			g_set_error_literal (error, GCM_SENSOR_ERROR,
					     GCM_SENSOR_ERROR_INTERNAL,
					     "the device is locked");
			goto out;
		}

		/* failure, the return buffer is set to "NoCmd" */
		if (reply[0] == HUEY_RETVAL_ERROR) {
			g_set_error (error, GCM_SENSOR_ERROR,
				     GCM_SENSOR_ERROR_INTERNAL,
				     "failed to issue command: %s", &reply[2]);
			goto out;
		}

		/* we ignore retry */
		if (reply[0] != HUEY_RETVAL_RETRY) {
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
	guchar request[] = { HUEY_COMMAND_REGISTER_READ, 0xff, 0x00, 0x10, 0x3c, 0x06, 0x00, 0x00 };
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
	*value = (tmp[0] << 24) + (tmp[1] << 16) + (tmp[2] << 8) + (tmp[3] << 0);
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

/**
 * gcm_sensor_huey_get_ambient:
 **/
static gboolean
gcm_sensor_huey_get_ambient (GcmSensor *sensor, gdouble *value, GError **error)
{
	guchar reply[8];
	gboolean ret = FALSE;
	gsize reply_read;
	GcmSensorOutputType output_type;
	guchar request[] = { HUEY_COMMAND_GET_AMBIENT, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
	GcmSensorHuey *sensor_huey = GCM_SENSOR_HUEY (sensor);

	/* no hardware support */
	if (gcm_sensor_get_output_type (sensor) == GCM_SENSOR_OUTPUT_TYPE_PROJECTOR) {
		g_set_error_literal (error, GCM_SENSOR_ERROR,
				     GCM_SENSOR_ERROR_NO_SUPPORT,
				     "HUEY cannot measure ambient light in projector mode");
		goto out;
	}

	/* ensure the user set this */
	output_type = gcm_sensor_get_output_type (sensor);
	if (output_type == GCM_SENSOR_OUTPUT_TYPE_UNKNOWN) {
		g_set_error_literal (error, GCM_SENSOR_ERROR,
				     GCM_SENSOR_ERROR_INTERNAL,
				     "output sensor type was not set");
		goto out;
	}

	/* set state */
	gcm_sensor_set_state (sensor, GCM_SENSOR_STATE_MEASURING);

	/* hit hardware */
	request[2] = (output_type == GCM_SENSOR_OUTPUT_TYPE_LCD) ? 0x00 : 0x02;
	ret = gcm_sensor_huey_send_data (sensor_huey, request, 8, reply, 8, &reply_read, error);
	if (!ret)
		goto out;

	/* parse the value */
	*value = (gdouble) (reply[5] * 0xff + reply[6]) / HUEY_AMBIENT_UNITS_TO_LUX;
out:
	/* set state */
	gcm_sensor_set_state (sensor, GCM_SENSOR_STATE_IDLE);
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
	guchar payload[] = { HUEY_COMMAND_SET_LEDS, 0x00, ~value, 0x00, 0x00, 0x00, 0x00, 0x00 };
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
	guchar request[] = { HUEY_COMMAND_SENSOR_MEASURE_RGB, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
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
	values->R = 1.0f / ((reply[3] * 0xff) + reply[4]);

	/* get green */
	request[0] = HUEY_COMMAND_READ_GREEN;
	ret = gcm_sensor_huey_send_data (sensor_huey, request, 8, reply, 8, &reply_read, error);
	if (!ret)
		goto out;

	/* get value */
	values->G = 1.0f / ((reply[3] * 0xff) + reply[4]);

	/* get blue */
	request[0] = HUEY_COMMAND_READ_BLUE;
	ret = gcm_sensor_huey_send_data (sensor_huey, request, 8, reply, 8, &reply_read, error);
	if (!ret)
		goto out;

	/* get value */
	values->B = 1.0f / ((reply[3] * 0xff) + reply[4]);
out:
	return ret;
}

/* in a dark box, the sensors still report a reading */
#define HUEY_ABSOLUTE_OFFSET_RED	0.000119
#define HUEY_ABSOLUTE_OFFSET_GREEN	0.000119
#define HUEY_ABSOLUTE_OFFSET_BLUE	0.000018

/**
 * gcm_sensor_huey_sample:
 *
 * Sample the data in two passes.
 **/
static gboolean
gcm_sensor_huey_sample (GcmSensor *sensor, GcmColorXYZ *value, GError **error)
{
	gboolean ret = FALSE;
	gdouble precision_value;
	GcmColorRGB native;
	GcmSensorHueyMultiplier multiplier;
	GcmVec3 *input = (GcmVec3 *) &native;
	GcmVec3 *output = (GcmVec3 *) value;
	GcmSensorHuey *sensor_huey = GCM_SENSOR_HUEY (sensor);

	/* no hardware support */
	if (gcm_sensor_get_output_type (sensor) == GCM_SENSOR_OUTPUT_TYPE_PROJECTOR) {
		g_set_error_literal (error, GCM_SENSOR_ERROR,
				     GCM_SENSOR_ERROR_NO_SUPPORT,
				     "HUEY cannot measure ambient light in projector mode");
		goto out;
	}

	/* set state */
	gcm_sensor_set_state (sensor, GCM_SENSOR_STATE_MEASURING);

	/* set this to one value for a quick approximate value */
	multiplier.R = 1;
	multiplier.G = 1;
	multiplier.B = 1;
	ret = gcm_sensor_huey_sample_for_threshold (sensor_huey, &multiplier, &native, error);
	if (!ret)
		goto out;
	egg_debug ("initial values: red=%0.6lf, green=%0.6lf, blue=%0.6lf", native.R, native.G, native.B);

	/* compromise between the amount of time and the precision */
	precision_value = (gdouble) HUEY_PRECISION_TIME_VALUE;
	multiplier.R = precision_value * native.R;
	multiplier.G = precision_value * native.G;
	multiplier.B = precision_value * native.B;

	/* don't allow a value of zero */
	if (multiplier.R == 0)
		multiplier.R = 1;
	if (multiplier.G == 0)
		multiplier.G = 1;
	if (multiplier.B == 0)
		multiplier.B = 1;
	egg_debug ("using multiplier factor: red=%i, green=%i, blue=%i", multiplier.R, multiplier.G, multiplier.B);
	ret = gcm_sensor_huey_sample_for_threshold (sensor_huey, &multiplier, &native, error);
	if (!ret)
		goto out;
	egg_debug ("prescaled values: red=%0.4lf, green=%0.4lf, blue=%0.4lf", native.R, native.G, native.B);
	native.R = native.R * (gdouble)multiplier.R;
	native.G = native.G * (gdouble)multiplier.G;
	native.B = native.B * (gdouble)multiplier.B;
	egg_debug ("scaled values: red=%0.6lf, green=%0.6lf, blue=%0.6lf", native.R, native.G, native.B);
	egg_debug ("PRE MULTIPLY: %s\n", gcm_vec3_to_string (input));

	/* it would be rediculous for the device to emit RGB, it would be completely arbitrary --
	 * we assume the matrix of data is designed to convert to LAB or XYZ */
	gcm_mat33_vector_multiply (&sensor_huey->priv->calibration_matrix1, input, output);

	/* scale correct */
	gcm_vec3_scalar_multiply (output, HUEY_XYZ_POST_MULTIPLY_SCALE_FACTOR, output);

	egg_debug ("POST MULTIPLY: %s\n", gcm_vec3_to_string (output));
out:
	/* set state */
	gcm_sensor_set_state (sensor, GCM_SENSOR_STATE_IDLE);
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

	request[0] = HUEY_COMMAND_UNLOCK;
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
			       HUEY_VENDOR_ID, HUEY_PRODUCT_ID,
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
	ret = gcm_sensor_huey_read_register_word (sensor_huey, 0x00, &serial_number, error);
	if (!ret)
		goto out;
	serial_number_tmp = g_strdup_printf ("%i", serial_number);
	gcm_sensor_set_serial_number (sensor, serial_number_tmp);
	egg_debug ("Serial number: %s", serial_number_tmp);

	/* get unlock string */
	ret = gcm_sensor_huey_read_register_string (sensor_huey, 0x7a, priv->unlock_string, 5, error);
	if (!ret)
		goto out;
	egg_debug ("Unlock string: %s", priv->unlock_string);

	/* get matrix */
	gcm_mat33_clear (&priv->calibration_matrix1);
	ret = gcm_sensor_huey_read_register_matrix (sensor_huey, 0x04, &priv->calibration_matrix1, error);
	if (!ret)
		goto out;
	egg_debug ("device matrix1: %s", gcm_mat33_to_string (&priv->calibration_matrix1));

	/* get another matrix, although this one is different... */
	gcm_mat33_clear (&priv->calibration_matrix2);
	ret = gcm_sensor_huey_read_register_matrix (sensor_huey, 0x36, &priv->calibration_matrix2, error);
	if (!ret)
		goto out;
	egg_debug ("device matrix2: %s", gcm_mat33_to_string (&priv->calibration_matrix2));

	/* spin the LEDs */
	for (i=0; spin_leds[i] != 0xff; i++) {
		ret = gcm_sensor_huey_set_leds (sensor, spin_leds[i], error);
		if (!ret)
			goto out;
		g_usleep (50000);
	}
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

	/* dump the unlock string */
	g_string_append_printf (data, "huey-dump-version:%i\n", 1);
	g_string_append_printf (data, "unlock-string:%s\n", priv->unlock_string);

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
	parent_class->get_ambient = gcm_sensor_huey_get_ambient;
	parent_class->set_leds = gcm_sensor_huey_set_leds;
	parent_class->sample = gcm_sensor_huey_sample;
	parent_class->startup = gcm_sensor_huey_startup;
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
	gcm_mat33_clear (&priv->calibration_matrix1);
	gcm_mat33_clear (&priv->calibration_matrix2);
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
 **/
GcmSensor *
gcm_sensor_huey_new (void)
{
	GcmSensorHuey *sensor;
	sensor = g_object_new (GCM_TYPE_SENSOR_HUEY,
			       "native", TRUE,
			       "kind", GCM_SENSOR_KIND_HUEY,
			       NULL);
	return GCM_SENSOR (sensor);
}


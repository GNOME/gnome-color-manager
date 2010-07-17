/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009 Richard Hughes <richard@hughsie.com>
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

/*
 * I've reversed engineered this myself using USB Snoop on Windows XP,
 * in a KVM virtual machine. Please see ../docs/huey for data.
 */

#include <stdio.h>
#include <sys/types.h>

#include <libusb-1.0/libusb.h>
#include <glib.h>
#include <libcolor-glib.h>

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

/* input:   00 00 00 00 3f 00 00 00
 * returns: 00 00 43 69 72 30 30 31  (or)
 *     "Cir001" --^^^^^^^^^^^^^^^^^ -- Cirrus Logic? Circuit1?...
 *          c0 00 4c 6f 63 6b 65 64
 *     "locked" --^^^^^^^^^^^^^^^^^
 *
 * Seems to get the currect status of the device.
 */
#define HUEY_COMMAND_GET_STATUS		0x00

/* input:   02 xx xx xx xx xx xx xx
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
 * only when profiling
 * has to be preceeded by HUEY_COMMAND_SENSOR_MEASURE_RGB (00 1e 00 27 00 15 03)
 */
#define HUEY_COMMAND_READ_GREEN		0x02

/* input:   03 xx xx xx xx xx xx xx
 * returns: 00 03 00 0f 18 00 00 00
 *            data --^^^^^ ^-- only ever 00 or 80
 *                    |
 *                    \-- for RGB(00,00,00) is 09 64
 *                            RGB(ff,ff,ff) is 08 80
 *                            RGB(ff,00,00) is 03 22
 *                            RGB(00,ff,00) is 00 58
 *                            RGB(00,00,ff) is 00 59
 *
 * Only used when doing profiling
 * has to be preceeded by HUEY_COMMAND_SENSOR_MEASURE_RGB (00 01 00 01 00 01 7f)
 */
#define HUEY_COMMAND_READ_BLUE	0x03

/* input:   05 ?? 11 12 13 14 xx xx
 * returns: 00 05 00 00 00 00 00 00
 *              ^--- always the same no matter the input
 *
 * never used in profiling */
#define HUEY_COMMAND_SET_VALUE		0x05

/* input:   06 xx xx xx xx xx xx xx
 * returns: 00 06 11 12 13 14 00 00
 *    4 bytes ----^^^^^^^^^^^ (from HUEY_COMMAND_SET_VALUE)
 *
 * This is some sort of 32bit register on the device -- the
 * default value at plug-in is 00 0f 42 40, although during profiling it is set to
 * 00 00 6f 00 and then 00 00 61 00.
 */
#define HUEY_COMMAND_GET_VALUE		0x06

/* NEVER USED */
#define HUEY_COMMAND_UNKNOWN_07		0x07

/* (sent at startup  after the unlock)
 * input:   08 0b xx xx xx xx xx xx
 *             ^^-- register address
 * returns: 00 08 0b b8 00 00 00 00
 *      address --^^ ^^-- value
 */
#define HUEY_COMMAND_REGISTER_READ	0x08

/* input:   0e 47 72 4d 62 6b 65 64
 *  "GrMbked"--^^^^^^^^^^^^^^^^^^^^
 * returns: 00 0e 00 00 00 00 00 00
 */
#define HUEY_COMMAND_UNLOCK		0x0e

/* returns: all NULL all of the time */
#define HUEY_COMMAND_UNKNOWN_0F		0x0f

/* something to do with sampling */
#define HUEY_COMMAND_UNKNOWN_10		0x10

/* something to do with sampling (that needs a retry with code 5a) */
#define HUEY_COMMAND_UNKNOWN_11		0x11

/* something to do with sampling */
#define HUEY_COMMAND_UNKNOWN_12		0x12

/* returns: all NULL all of the time */
#define HUEY_COMMAND_UNKNOWN_13		0x13

/* returns: seems to be sent, but not requested */
#define HUEY_COMMAND_UNKNOWN_15		0x15

/* input:   16 00 01 00 01 00 01 00
 * returns: 00 16 00 00 00 00 00 00
 *
 * or:
 *                ||----||----||-- numbers steadily increase -- some kind of gain control?
 *    0 or 1 ---.-----.-----.    ,,-- only 00 7f or 03 in the profile-complete
 * input:   16 00 35 00 48 00 1d 03
 * returns: 00 16 00 0b d0 00 00 00
 *            data --^^^^^ ^^-- only ever 00 or 80
 *                    \-- for RGB(00,00,00) is odd	(00 16 02 20 f4 ee 07 00)
 *                            RGB(ff,ff,ff) is odd	(00 16 00 03 ac 80 00 00)
 *                            RGB(ff,00,00) is 06 ea	(00 16 00 06 ed 00 00 00)
 *                            RGB(00,ff,00) is 08 9b	(00 16 00 08 a0 80 00 00)
 *                            RGB(00,00,ff) is 55 5e	(00 16 00 55 73 80 00 00)
 *
 * only when profiling, and used with blue and green
 * THIS COMMAND TAKES A LONG TIME TO EXECUTE
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
 *
 * IT'S QUICKER TO READ WHITE THAN BLACK!! -- maybe amount of time to count a number of photons?
 */
#define HUEY_COMMAND_SENSOR_MEASURE_RGB		0x16

/* input:   21 09 00 02 00 00 08 00 (or)
 * returns: [never seems to return a value]
 *
 * only when profiling, and over and over -- some sort of poll? */
#define HUEY_COMMAND_UNKNOWN_21		0x21

/* input:   17 03 00 xx xx xx xx xx
 * returns: 90 17 03 00 00 00 00 00  then on second read:
 * 	    00 17 03 00 00 62 57 00 in light (or)
 * 	    00 17 03 00 00 00 08 00 in dark
 * 	no idea	--^^  |    ^---^ = 16bits data?
 *                    \-- only ever 0 or 2 (only ever saw 2 once...)
 */
#define HUEY_COMMAND_AMBIENT		0x17

/* input:   18 00 f0 xx xx xx xx xx
 * returns: 00 18 f0 00 00 00 00 00
 *   led mask ----^^
 */
#define HUEY_COMMAND_SET_LEDS		0x18

/* returns: all NULL for NULL input: times out for f1 f2 f3 f4 f5 f6 f7 f8 */
#define HUEY_COMMAND_UNKNOWN_19		0x19

#define HUEY_AMBIENT_UNITS_TO_LUX	125.0f /* fudge factor */

typedef struct {
	gboolean		 connected;
	libusb_device		*device;
	libusb_device_handle	*handle;
	GcmMat3x3		 device_matrix;
} GcmSensorHuey;

/**
 * gcm_sensor_huey_find_device:
 **/
static gboolean
gcm_sensor_huey_find_device (GcmSensorHuey *huey, GError **error)
{
	struct libusb_device_descriptor desc;
	libusb_device **devs = NULL;
	libusb_device *dev;
	gint retval;
	gsize cnt;
	guint i = 0;
	gboolean ret = FALSE;

	/* get device */
	cnt = libusb_get_device_list (NULL, &devs);
	if (cnt < 0) {
		g_set_error (error, 1, 0, "failed to get device list: %s", libusb_strerror (cnt));
		goto out;
	}

	/* find device */
	for (i=0; i<cnt; i++) {
		dev = devs[i];

		/* get descriptor */
		retval = libusb_get_device_descriptor (dev, &desc);
		if (retval < 0) {
			g_warning ("failed to get device descriptor for %02x:%02x, possibly faulty hardware",
				   libusb_get_bus_number (dev), libusb_get_device_address (dev));
			continue;
		}

		/* does match HUEY? */
		if (desc.idVendor == HUEY_VENDOR_ID &&
		    desc.idProduct == HUEY_PRODUCT_ID) {
			g_debug ("got huey!");
			ret = TRUE;
			huey->device = libusb_ref_device (dev);
			break;
		}
	}

	/* not found */
	if (!ret) {
		g_set_error_literal (error, 1, 0, "no compatible hardware attached");
		goto out;
	}

	/* open device */
	retval = libusb_open (huey->device, &huey->handle);
	if (retval < 0) {
		ret = FALSE;
		g_set_error (error, 1, 0, "failed to open device: %s", libusb_strerror (retval));
		goto out;
	}

	/* set configuration and interface, the only values we've got in lsusb-vvv */
	retval = libusb_set_configuration (huey->handle, 1);
	if (retval < 0) {
		ret = FALSE;
		g_set_error (error, 1, 0, "failed to set configuration: %s", libusb_strerror (retval));
		goto out;
	}
	retval = libusb_claim_interface (huey->handle, 0);
	if (retval < 0) {
		ret = FALSE;
		g_set_error (error, 1, 0, "failed to claim interface: %s", libusb_strerror (retval));
		goto out;
	}
out:
	libusb_free_device_list (devs, 1);
	return ret;
}

#define CONSOLE_RESET		0
#define CONSOLE_BLACK 		30
#define CONSOLE_RED		31
#define CONSOLE_GREEN		32
#define CONSOLE_YELLOW		33
#define CONSOLE_BLUE		34
#define CONSOLE_MAGENTA		35
#define CONSOLE_CYAN		36
#define CONSOLE_WHITE		37

/**
 * gcm_sensor_huey_print_data:
 **/
static void
gcm_sensor_huey_print_data (const gchar *title, const guchar *data, gsize length)
{
	guint i;

	if (g_strcmp0 (title, "request") == 0)
		g_print ("%c[%dm", 0x1B, CONSOLE_RED);
	if (g_strcmp0 (title, "reply") == 0)
		g_print ("%c[%dm", 0x1B, CONSOLE_BLUE);
	g_print ("%s\t", title);

	for (i=0; i< length; i++)
		g_print ("%02x [%c]\t", data[i], g_ascii_isprint (data[i]) ? data[i] : '?');
		//g_print ("%02x,", data[i]);

	g_print ("%c[%dm", 0x1B, CONSOLE_RESET);

	g_print ("\n");
}

/**
 * gcm_sensor_huey_send_data:
 **/
static gboolean
gcm_sensor_huey_send_data (GcmSensorHuey *huey,
			   const guchar *request, gsize request_len,
			   guchar *reply, gsize reply_len,
			   gsize *reply_read, GError **error)
{
	gint retval;
	gboolean ret = FALSE;
	guint i;

	g_return_val_if_fail (request != NULL, FALSE);
	g_return_val_if_fail (request_len != 0, FALSE);
	g_return_val_if_fail (reply != NULL, FALSE);
	g_return_val_if_fail (reply_len != 0, FALSE);
	g_return_val_if_fail (reply_read != NULL, FALSE);

	/* show what we've got */
	gcm_sensor_huey_print_data ("request", request, request_len);

	/* do sync request */
	retval = libusb_control_transfer (huey->handle,
					  LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
					  0x09, 0x0200, 0,
					  (guchar *) request, request_len,
					  HUEY_CONTROL_MESSAGE_TIMEOUT);
	if (retval < 0) {
		g_set_error (error, 1, 0, "failed to send request: %s", libusb_strerror (retval));
		goto out;
	}

	/* some commands need to retry the read, unknown reason */
	for (i=0; i<HUEY_MAX_READ_RETRIES; i++) {

		/* get sync response, from bEndpointAddress */
		retval = libusb_interrupt_transfer (huey->handle, 0x81,
						    reply, (gint) reply_len, (gint*)reply_read,
						    HUEY_CONTROL_MESSAGE_TIMEOUT);
		if (retval < 0) {
			g_set_error (error, 1, 0, "failed to get reply: %s", libusb_strerror (retval));
			goto out;
		}

		/* show what we've got */
		gcm_sensor_huey_print_data ("reply", reply, *reply_read);

		/* the second byte seems to be the command again */
		if (reply[1] != request[0]) {
			g_set_error (error, 1, 0, "wrong command reply, got 0x%02x, expected 0x%02x", reply[1], request[0]);
			goto out;
		}

		/* the first byte is status */
		if (reply[0] == HUEY_RETVAL_SUCCESS) {
			ret = TRUE;
			break;
		}

		/* failure, the return buffer is set to "Locked" */
		if (reply[0] == HUEY_RETVAL_LOCKED) {
			g_set_error_literal (error, 1, 0, "the device is locked");
			goto out;
		}

		/* failure, the return buffer is set to "NoCmd" */
		if (reply[0] == HUEY_RETVAL_ERROR) {
			g_set_error (error, 1, 0, "failed to issue command: %s", &reply[2]);
			goto out;
		}

		/* we ignore retry */
		if (reply[0] != HUEY_RETVAL_RETRY) {
			g_set_error (error, 1, 0, "return value unknown: 0x%02x", reply[0]);
			goto out;
		}
	}

	/* no success */
	if (!ret) {
		g_set_error (error, 1, 0, "gave up retrying after %i reads", HUEY_MAX_READ_RETRIES);
		goto out;
	}
out:
	return ret;
}

/**
 * gcm_sensor_huey_send_command:
 **/
static gboolean
gcm_sensor_huey_send_command (GcmSensorHuey *huey, guchar command, const guchar *payload, GError **error)
{
	guchar request[8];
	guchar reply[8];
	gboolean ret;
	gsize reply_read;
	guint i;

	/* first byte seems to be a command */
	request[0] = command;
	for (i=1; i<8; i++)
		request[i] = payload[i-1];

	/* show what we've got */
	g_print ("cmd 0x%02x\n", command);

	ret = gcm_sensor_huey_send_data (huey, request, 8, reply, 8, &reply_read, error);
	if (!ret)
		goto out;

out:
	return ret;
}

/**
 * gcm_sensor_huey_send_unlock:
 **/
static gboolean
gcm_sensor_huey_send_unlock (GcmSensorHuey *huey, GError **error)
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
	request[5] = 'k'; // <- perhaps junk, need to test next time locked */
	request[6] = 'e'; // <-         "" */
	request[7] = 'd'; // <-         "" */

	/* GrMbked == I have no idea, neither does google */
	ret = gcm_sensor_huey_send_data (huey, request, 8, reply, 8, &reply_read, error);
	if (!ret)
		goto out;
out:
	return ret;
}

/**
 * gcm_sensor_huey_send_leds:
 **/
static gboolean
gcm_sensor_huey_send_leds (GcmSensorHuey *huey, guchar mask, GError **error)
{
	guchar payload[] = { 0x00, ~mask, 0x00, 0x00, 0x00, 0x00, 0x00 };
	return gcm_sensor_huey_send_command (huey, HUEY_COMMAND_SET_LEDS, payload, error);
}

/**
 * gcm_sensor_huey_get_ambient:
 **/
static gboolean
gcm_sensor_huey_get_ambient (GcmSensorHuey *huey, gdouble *value, GError **error)
{
	const guchar request[] = { HUEY_COMMAND_AMBIENT, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
	guchar reply[8];
	gboolean ret;
	gsize reply_read;

	/* hit hardware */
	ret = gcm_sensor_huey_send_data (huey, request, 8, reply, 8, &reply_read, error);
	if (!ret)
		goto out;

	/* parse the value */
	g_debug ("%i, %i", reply[5], reply[5]);
	*value = (gdouble) (reply[5] * 0xff + reply[6]) / HUEY_AMBIENT_UNITS_TO_LUX;
out:
	return ret;
}

/**
 * gcm_sensor_huey_data_to_float:
 **/
static gfloat
gcm_sensor_huey_data_to_float (guint8 *value)
{
	guint32 big;
	gfloat retval;

	/* first, convert the guchar data into one long int */
	big = (value[0] << 24) + (value[1] << 16) + (value[2] << 8) + (value[3] << 0);

	/* then convert it to a float */
	*((guint32 *)(&retval)) = big;
	return retval;
}

/**
 * gcm_sensor_huey_read_register_byte:
 **/
static gboolean
gcm_sensor_huey_read_register_byte (GcmSensorHuey *huey, guint8 addr, guint8 *value, GError **error)
{
	guchar request[] = { HUEY_COMMAND_REGISTER_READ, 0xff, 0x00, 0x10, 0x3c, 0x06, 0x00, 0x00 };
	guchar reply[8];
	gboolean ret;
	gsize reply_read;

	/* hit hardware */
	request[1] = addr;
	ret = gcm_sensor_huey_send_data (huey, request, 8, reply, 8, &reply_read, error);
	if (!ret)
		goto out;

	/* this seems like the only byte of data that's useful -- it would be
	 * good to be able to get more than one byte of data at a time... */
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
 * gcm_sensor_huey_read_registers:
 **/
static gboolean
gcm_sensor_huey_read_registers (GcmSensorHuey *huey, GError **error)
{
	gboolean ret;
	guint8 i, j;
	guint len = 0xff;
	guint8 data[len];
	gchar unlock[5];

	/* get unlock string */
	ret = gcm_sensor_huey_read_register_string (huey, 0x7a, unlock, 5, error);
	if (!ret)
		goto out;
	g_print ("Unlock string: %s\n", unlock);

	/* get matrix */
	gcm_mat33_clear (&huey->device_matrix);
	ret = gcm_sensor_huey_read_register_matrix (huey, 0x04, &huey->device_matrix, error);
	if (!ret)
		goto out;
	g_print ("device matrix: %s\n", gcm_mat33_to_string (&huey->device_matrix));

goto out;

	/* We read from 0x04 to 0x72 at startup */
	for (i=0x00; i<=len; i++) {
		ret = gcm_sensor_huey_read_register_byte (huey, i, &data[i], error);
		if (!ret)
			goto out;
	}

	/* try to find patterns */
	for (i=0; i<len; i+=4) {
		g_print ("0x%02x\t", i);
		for (j=0; j<4; j++)
			g_print ("%c ", g_ascii_isprint (data[i+j]) ? data[i+j] : '?');
		g_print ("\n");
	}
	g_print ("\n");

	for (i=0; i<len; i+=4) {
		g_print ("0x%02x\t", i);
		for (j=0; j<4; j++)
			g_print ("%02i ", data[i+j]);
		g_print ("\n");
	}
	g_print ("\n");

	for (i=0; i<len; i+=4) {
		g_print ("0x%02x\t", i);
		g_print ("%.4f ", gcm_sensor_huey_data_to_float (&data[i]));
		g_print ("\n");
	}
	g_print ("\n");



out:
	return ret;
}

/**
 * gcm_sensor_huey_get_color_for_threshold:
 **/
static gboolean
gcm_sensor_huey_get_color_for_threshold (GcmSensorHuey *huey, GcmColorRgbInt *threshold, GcmColorRgb *values, GError **error)
{
	guchar request[] = { HUEY_COMMAND_SENSOR_MEASURE_RGB, 0x00, threshold->red, 0x00, threshold->green, 0x00, threshold->blue, 0x00 };
	guchar reply[8];
	gboolean ret;
	gsize reply_read;

	/* measure, and get red */
	ret = gcm_sensor_huey_send_data (huey, request, 8, reply, 8, &reply_read, error);
	if (!ret)
		goto out;

	/* get value */
	values->red = 1.0f / ((reply[3] * 0xff) + reply[4]);

	/* get green */
	request[0] = HUEY_COMMAND_READ_GREEN;
	ret = gcm_sensor_huey_send_data (huey, request, 8, reply, 8, &reply_read, error);
	if (!ret)
		goto out;

	/* get value */
	values->green = 1.0f / ((reply[3] * 0xff) + reply[4]);

	/* get blue */
	request[0] = HUEY_COMMAND_READ_BLUE;
	ret = gcm_sensor_huey_send_data (huey, request, 8, reply, 8, &reply_read, error);
	if (!ret)
		goto out;

	/* get value */
	values->blue = 1.0f / ((reply[3] * 0xff) + reply[4]);
out:
	return ret;
}

/* this is a random number chosen to find the best accuracy whilst
 * maintaining a fast read. We scale each RGB value seporately. */
#define HUEY_PRECISION_TIME_VALUE	0.25f

/* the sensor returns an arbitary value which we scale to 1.0 for 100%
 * (this is based on my monitor, not any agreed standard...) */
#define HUEY_SCALE_VALUE_RED		1.0f
#define HUEY_SCALE_VALUE_GREEN		1.0f
#define HUEY_SCALE_VALUE_BLUE		1.0f

/**
 * gcm_sensor_huey_get_color:
 **/
static gboolean
gcm_sensor_huey_get_color (GcmSensorHuey *huey, GcmColorRgb *values, GError **error)
{
	gboolean ret;
	gdouble value;
	GcmColorRgbInt multiplier;

	/* set this to one value for a quick approximate value */
	multiplier.red = 1;
	multiplier.green = 1;
	multiplier.blue = 1;
	ret = gcm_sensor_huey_get_color_for_threshold (huey, &multiplier, values, error);
	if (!ret)
		goto out;
	g_debug ("initial values: red=%0.4lf, green=%0.4lf, blue=%0.4lf", values->red, values->green, values->blue);

	/* compromise between the amount of time and the precision */
	value = HUEY_PRECISION_TIME_VALUE;
	if (values->red < value)
		multiplier.red = (gfloat)value / values->red;
	if (values->green < value)
		multiplier.green = (gfloat)value / values->green;
	if (values->blue < value)
		multiplier.blue = (gfloat)value / values->blue;
	g_debug ("using multiplier factor: red=%i, green=%i, blue=%i", multiplier.red, multiplier.green, multiplier.blue);
	ret = gcm_sensor_huey_get_color_for_threshold (huey, &multiplier, values, error);
	if (!ret)
		goto out;
	g_debug ("prescaled values: red=%0.4lf, green=%0.4lf, blue=%0.4lf", values->red, values->green, values->blue);
	values->red = HUEY_SCALE_VALUE_RED * values->red / multiplier.red;
	values->green = HUEY_SCALE_VALUE_GREEN * values->green / multiplier.green;
	values->blue = HUEY_SCALE_VALUE_BLUE * values->blue / multiplier.blue;
	g_debug ("scaled values: red=%0.4lf, green=%0.4lf, blue=%0.4lf", values->red, values->green, values->blue);

{
	GcmVec3 *vec = (GcmVec3 *) values;
	GcmVec3 test;

	g_print ("PRE: %s\n", gcm_vec3_to_string (vec));

	/* it would be redicuous for the device to emit RGB, it would be completely arbitrary --
	 * see if the matrix of data is designed to convert to LAB or XYZ */
	gcm_mat33_vector_multiply (&huey->device_matrix, vec, &test);

test.v0 *= 1000;
test.v1 *= 1000;
test.v2 *= 1000;

// argyll gets XYZ: 95.773273 50.278955 2.210076 -- we get 3.636, 0.435, -0.017

	g_print ("POST: %s\n", gcm_vec3_to_string (&test));
}

	g_assert (values->red < 1.0f);
	g_assert (values->green < 1.0f);
	g_assert (values->blue < 1.0f);

out:
	return ret;
}

/**
 * main:
 **/
int
main (void)
{
	gint retval;
	gdouble value;
	gboolean ret;
	GcmSensorHuey *huey;
	GError *error = NULL;

	huey = g_new0 (GcmSensorHuey, 1);

	/* connect */
	retval = libusb_init (NULL);
	if (retval < 0) {
		g_warning ("failed to init libusb: %s", libusb_strerror (retval));
		goto out;
	}
	huey->connected = TRUE;

	/* find device */
	ret = gcm_sensor_huey_find_device (huey, &error);
	if (!ret) {
		g_warning ("failed to find sensor: %s", error->message);
		g_error_free (error);
		goto out;
	}

if(0){
	/* unlock */
	ret = gcm_sensor_huey_send_unlock (huey, &error);
	if (!ret) {
		g_warning ("failed to unlock: %s", error->message);
		g_error_free (error);
		goto out;
	}
}

if (1) {
	/* this is done by the windows driver */
	ret = gcm_sensor_huey_read_registers (huey, &error);
	if (!ret) {
		g_warning ("failed to do read register: %s", error->message);
		g_error_free (error);
		goto out;
	}
}

	/* set LEDs */
	ret = gcm_sensor_huey_send_leds (huey, 0x0f, &error);
	if (!ret) {
		g_warning ("failed to send leds: %s", error->message);
		g_error_free (error);
		goto out;
	}

if (1) {
	/* get ambient */
	ret = gcm_sensor_huey_get_ambient (huey, &value, &error);
	if (!ret) {
		g_warning ("failed to get ambient: %s", error->message);
		g_error_free (error);
		goto out;
	}
	g_debug ("ambient = %.1lf Lux", value);
}

if (0) {
	guchar payload[] = { 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17 };
	g_warning ("moo");

	/* get default value when device is plugged */
	gcm_sensor_huey_send_command (huey, HUEY_COMMAND_GET_VALUE, payload, &error);

	gcm_sensor_huey_send_command (huey, HUEY_COMMAND_SET_VALUE, payload, &error);

	payload[0] = 0x01;
	payload[1] = 0x02;
	payload[2] = 0x03;
	payload[3] = 0x04;
	payload[4] = 0x05;
	payload[5] = 0x06;
	payload[6] = 0x07;

	gcm_sensor_huey_send_command (huey, HUEY_COMMAND_GET_VALUE, payload, &error);
}

/* try to get color value */
if (1) {

	GcmColorRgb values;
	ret = gcm_sensor_huey_get_color (huey, &values, &error);
	if (!ret) {
		g_warning ("failed to measure: %s", error->message);
		g_error_free (error);
		goto out;
	}
	g_debug ("red=%0.4lf, green=%0.4lf, blue=%0.4lf", values.red, values.green, values.blue);
}
	/* set LEDs */
	ret = gcm_sensor_huey_send_leds (huey, 0x00, &error);
	if (!ret) {
		g_warning ("failed to send leds: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* close device */
	libusb_close (huey->handle);
out:
	if (huey->device != NULL)
		libusb_unref_device (huey->device);
	if (huey->connected)
		libusb_exit (NULL);
	g_free (huey);
	return 0;
}


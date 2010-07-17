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

#define HUEY_VENDOR_ID			0x0971
#define HUEY_PRODUCT_ID			0x2005
#define HUEY_CONTROL_MESSAGE_TIMEOUT	5000 /* ms */

#define HUEY_RETVAL_SUCCESS		0x00
#define HUEY_RETVAL_LOCKED		0xc0
#define HUEY_RETVAL_ERROR		0x80

#define HUEY_COMMAND_UNKNOWN_00		0x00 /* returns: "Cir001" -- Cirrus Logic? It's a Cyprus IC... */
#define HUEY_COMMAND_UNKNOWN_02		0x02 /* returns: all NULL for NULL input, 00,02,02,cc,53,6c,00,00 for 0xf1f2f3f4f5f6f7f8 */
#define HUEY_COMMAND_UNKNOWN_03		0x03 /* returns: all NULL for NULL input, 00,03,62,18,88,85,00,00 for 0xf1f2f3f4f5f6f7f8 */
#define HUEY_COMMAND_UNKNOWN_05		0x05 /* returns: all NULL for NULL input, 00,05,00,00,00,00,00,00 for 0xf1f2f3f4f5f6f7f8 */
#define HUEY_COMMAND_UNKNOWN_06		0x06 /* returns: all NULL for NULL input, 00,06,f1,f2,f3,f4,00,00 for 0xf1f2f3f4f5f6f7f8 */
#define HUEY_COMMAND_UNKNOWN_07		0x07 /* returns: all NULL all of the time */
#define HUEY_COMMAND_UNKNOWN_08		0x08 /* returns: all NULL for NULL input, 00,08,f1,f2,00,00,00,00 for 0xf1f2f3f4f5f6f7f8 */
#define HUEY_COMMAND_MAYBE_UNLOCK	0x0e /* returns: all NULL all of the time */
#define HUEY_COMMAND_UNKNOWN_0F		0x0f /* returns: all NULL all of the time */
#define HUEY_COMMAND_UNKNOWN_13		0x13 /* returns: all NULL all of the time */
#define HUEY_COMMAND_UNKNOWN_16		0x16 /* returns: all NULL for NULL input, times out for 0xf1f2f3f4f5f6f7f8 */
#define HUEY_COMMAND_SET_LEDS		0x18 /* returns: all NULL for NULL input, times out for 0xf1f2f3f4f5f6f7f8 */
#define HUEY_COMMAND_UNKNOWN_19		0x19 /* returns: all NULL for NULL input, times out for 0xf1f2f3f4f5f6f7f8 */

typedef struct {
	gboolean		 connected;
	libusb_device		*device;
	libusb_device_handle	*handle;
} GcmPriv;

static gboolean
gcm_find_device (GcmPriv *priv, GError **error)
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
			priv->device = libusb_ref_device (dev);
			break;
		}
	}

	/* not found */
	if (!ret) {
		g_set_error_literal (error, 1, 0, "no compatible hardware attached");
		goto out;
	}

	/* open device */
	retval = libusb_open (priv->device, &priv->handle);
	if (retval < 0) {
		ret = FALSE;
		g_set_error (error, 1, 0, "failed to open device: %s", libusb_strerror (retval));
		goto out;
	}

	/* set configuration and interface, the only values we've got in lsusb-vvv */
	retval = libusb_set_configuration (priv->handle, 1);
	if (retval < 0) {
		ret = FALSE;
		g_set_error (error, 1, 0, "failed to set configuration: %s", libusb_strerror (retval));
		goto out;
	}
	retval = libusb_claim_interface (priv->handle, 0);
	if (retval < 0) {
		ret = FALSE;
		g_set_error (error, 1, 0, "failed to claim interface: %s", libusb_strerror (retval));
		goto out;
	}
out:
	libusb_free_device_list (devs, 1);
	return ret;
}

static void
print_data (const gchar *title, const guchar *data, gsize length)
{
	guint i;
	g_print ("%s\t", title);
	for (i=0; i< length; i++)
		g_print ("%02x [%c]\t", data[i], g_ascii_isprint (data[i]) ? data[i] : '?');
		//g_print ("%02x,", data[i]);
	g_print ("\n");
}


static gboolean
send_data (GcmPriv *priv,
	   const guchar *request, gsize request_len,
	   guchar *reply, gsize reply_len,
	   gsize *reply_read, GError **error)
{
	gint retval;
	gboolean ret = FALSE;

	g_return_val_if_fail (request != NULL, FALSE);
	g_return_val_if_fail (request_len != 0, FALSE);
	g_return_val_if_fail (reply != NULL, FALSE);
	g_return_val_if_fail (reply_len != 0, FALSE);
	g_return_val_if_fail (reply_read != NULL, FALSE);

	/* show what we've got */
	print_data ("request", request, request_len);

	g_usleep (10000);

	/* do sync request */
	retval = libusb_control_transfer (priv->handle,
					  LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
					  0x09, 0x0200, 0,
					  (guchar *) request, request_len,
					  HUEY_CONTROL_MESSAGE_TIMEOUT);
	if (retval < 0) {
		g_set_error (error, 1, 0, "failed to send request: %s", libusb_strerror (retval));
		goto out;
	}

	/* get sync response, from bEndpointAddress */
	retval = libusb_interrupt_transfer (priv->handle, 0x81,
					    reply, (gint) reply_len, (gint*)reply_read,
					    HUEY_CONTROL_MESSAGE_TIMEOUT);
	if (retval < 0) {
		g_set_error (error, 1, 0, "failed to get reply: %s", libusb_strerror (retval));
		goto out;
	}

	/* show what we've got */
	print_data ("reply", reply, *reply_read);

	/* the first byte is success */
	switch (reply[0]) {
	case HUEY_RETVAL_SUCCESS:
		/* assume success */
		break;
	case HUEY_RETVAL_LOCKED:
		/* failure, the return buffer is set to "Locked" */
		g_set_error_literal (error, 1, 0, "the device is locked");
		goto out;
	case HUEY_RETVAL_ERROR:
		/* failure, the return buffer is set to "NoCmd" */
		g_set_error (error, 1, 0, "failed to issue command: %s", &reply[2]);
		goto out;
	default:
		g_warning ("return value unknown: 0x%02x, continuing", reply[0]);
		break;
	}

	/* the second byte seems to be the command again */
	if (reply[1] != request[0]) {
		g_set_error (error, 1, 0, "wrong command reply, got 0x%02x, expected 0x%02x", reply[1], request[0]);
		goto out;
	}

	/* success */
	ret = TRUE;
out:
	return ret;
}

static gboolean
send_command (GcmPriv *priv, guchar command, const guchar *payload, GError **error)
{
	/* according to wMaxPacketSize, all the messages have just 8 bytes */
	guchar request[8];
	guchar reply[8];
	gboolean ret;
	gsize reply_read;
	guint i;

	/* first byte seems to be a command, i've no idea what the others do */
	request[0] = command;
	for (i=1; i<8; i++)
		request[i] = payload[i-1];

	/* show what we've got */
	g_print ("cmd 0x%02x\n", command);

	ret = send_data (priv, request, 8, reply, 8, &reply_read, error);
	if (!ret)
		goto out;

out:
	return ret;
}

static gboolean
send_unlock (GcmPriv *priv, GError **error)
{
	/* according to wMaxPacketSize, all the messages have just 8 bytes */
	guchar request[8];
	guchar reply[8];
	gboolean ret;
	gsize reply_read;

	request[0] = HUEY_COMMAND_MAYBE_UNLOCK;
	request[1] = 'G';
	request[2] = 'r';
	request[3] = 'M';
	request[4] = 'b';
	request[5] = 'k';
	request[6] = 'e';
	request[7] = 'd';

	/* GrMbked == I have no idea, neither does google */
	ret = send_data (priv, request, 8, reply, 8, &reply_read, error);
	if (!ret)
		goto out;
out:
	return ret;
}

static gboolean
send_leds (GcmPriv *priv, guchar mask, GError **error)
{
	guchar payload[] = { 0x00, ~mask, 0x00, 0x00, 0x00, 0x00, 0x00 };

	/* send all commands that are implemented */
	return send_command (priv, HUEY_COMMAND_SET_LEDS, payload, error);
}

int
main (void)
{
	gint retval;
	guint i;
	gboolean ret;
	GcmPriv *priv;
	GError *error = NULL;

	priv = g_new0 (GcmPriv, 1);

	/* connect */
	retval = libusb_init (NULL);
	if (retval < 0) {
		g_warning ("failed to init libusb: %s", libusb_strerror (retval));
		goto out;
	}
	priv->connected = TRUE;

	/* find device */
	ret = gcm_find_device (priv, &error);
	if (!ret) {
		g_warning ("failed to find sensor: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* unlock */
	ret = send_unlock (priv, &error);
	if (!ret) {
		g_warning ("failed to unlock: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* set LEDs */
	ret = send_leds (priv, 0x0f, &error);
	if (!ret) {
		g_warning ("failed to send leds: %s", error->message);
		g_error_free (error);
		goto out;
	}

if (0) {
	guchar payload[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
	guchar available[] = { 0x00, 0x02, 0x03, 0x05, 0x06, 0x07, 0x08, 0x0e, 0x0f, 0x13, 0x16, 0x18, 0x19, 0xff };

	/* send all commands that are implemented */
	for (i=0; available[i] != 0xff; i++) {
		ret = send_command (priv, available[i], payload, &error);
		if (!ret) {
			g_warning ("failed to write command 0x%02i: %s\n", available[i], error->message);
			g_clear_error (&error);
		}
	}
}

	/* close device */
	libusb_close (priv->handle);
out:
	if (priv->device != NULL)
		libusb_unref_device (priv->device);
	if (priv->connected)
		libusb_exit (NULL);
	g_free (priv);
	return 0;
}


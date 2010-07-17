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

static gboolean
send_command (GcmPriv *priv, guchar command, GError **error)
{
	gint retval;
	gboolean ret = FALSE;
	guchar *data;
	gint bytes_read = 0;
	guint i;

	/* according to wMaxPacketSize, all the messages have just 8 bytes */
	data = g_new0 (guchar, 8);
	data[1] = 0xf1;
	data[2] = 0xf2;
	data[3] = 0xf3;
	data[4] = 0xf4;
	data[5] = 0xf5;
	data[6] = 0xf6;
	data[7] = 0xf7;

	/* first byte seems to be a command, i've no idea what the others do */
	data[0] = command;

	/* do sync request */
	retval = libusb_control_transfer (priv->handle,
					  LIBUSB_ENDPOINT_OUT | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
					  0x09, 0x0200, 0,
					  data, 8,
					  HUEY_CONTROL_MESSAGE_TIMEOUT);
	if (retval < 0) {
		g_set_error (error, 1, 0, "failed to send message interface: %s", libusb_strerror (retval));
		goto out;
	}

	/* get sync response, from bEndpointAddress */
	retval = libusb_interrupt_transfer (priv->handle, 0x81, data, 8, &bytes_read, HUEY_CONTROL_MESSAGE_TIMEOUT);
	if (retval < 0) {
		g_set_error (error, 1, 0, "failed to recieve message interface: %s", libusb_strerror (retval));
		goto out;
	}

if(1){
	/* show what we've got */
	g_print ("cmd 0x%02x\t", command);
	for (i=2; i< (guint)bytes_read; i++)
		g_print ("%02x [%c]\t", data[i], g_ascii_isprint (data[i]) ? data[i] : '?');
	g_print ("\n");
}

	/* the first byte is success */
	switch (data[0]) {
	case 0x00:
		/* assume success */
		break;
	case 0x80:
		/* failure, the return buffer is set to NoCmd */
		g_set_error (error, 1, 0, "failed to issue command: %s", &data[2]);
		goto out;
	default:
		g_warning ("return value unknown: 0x%02x, continuing", data[0]);
		break;
	}

	/* the second byte seems to be the command again */
	if (data[1] != command) {
		g_set_error (error, 1, 0, "wrong command reply, got 0x%02x, expected 0x%02x", data[1], command);
		goto out;
	}

	/* success */
	ret = TRUE;
out:
	g_free (data);
	return ret;
}

#define HUEY_COMMAND_UNKNOWN_00		0x00 /* returns: "Cir001" -- Cirrus Logic? It's a Cyprus IC... */
#define HUEY_COMMAND_UNKNOWN_02		0x02 /* returns: all NULL */
#define HUEY_COMMAND_UNKNOWN_03		0x03 /* returns: all NULL */
#define HUEY_COMMAND_UNKNOWN_05		0x05 /* returns: all NULL */
#define HUEY_COMMAND_UNKNOWN_06		0x06 /* returns: all NULL */
#define HUEY_COMMAND_UNKNOWN_07		0x07 /* returns: all NULL */
#define HUEY_COMMAND_UNKNOWN_08		0x08 /* returns: all NULL */
#define HUEY_COMMAND_UNKNOWN_0E		0x0e /* returns: all NULL */
#define HUEY_COMMAND_UNKNOWN_0F		0x0f /* returns: all NULL */
#define HUEY_COMMAND_UNKNOWN_13		0x13 /* returns: all NULL */
#define HUEY_COMMAND_UNKNOWN_16		0x16 /* returns: all NULL */
#define HUEY_COMMAND_UNKNOWN_18		0x18 /* returns: all NULL */
#define HUEY_COMMAND_UNKNOWN_19		0x19 /* returns: all NULL */

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

{
	guchar available[] = { 0x00, 0x02, 0x03, 0x05, 0x06, 0x07, 0x08, 0x0e, 0x0f, 0x13, 0x16, 0x18, 0x19, 0xff };

	/* send all commands that are implemented */
	for (i=0; available[i] != 0xff; i++) {
		ret = send_command (priv, available[i], &error);
		if (!ret) {
			g_warning ("failed to write command 0x%02i: %s\n", available[i], error->message);
			g_clear_error (&error);
//		} else {
//			g_print ("0x%02x, ", i);
		}
	}
}

	/* do something */
	g_warning ("fixme");

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


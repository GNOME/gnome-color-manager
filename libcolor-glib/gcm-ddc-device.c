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
 * SECTION:gcm-ddc-device
 * @short_description: For managing different i2c devices
 *
 * A GObject to use for accessing devices.
 */

#include "config.h"

#include <glib-object.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <glib/gstdio.h>
#include <string.h>

#include <gcm-ddc-device.h>
#include <gcm-ddc-control.h>

static void     gcm_ddc_device_finalize	(GObject     *object);

#define GCM_DDC_DEVICE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GCM_TYPE_DDC_DEVICE, GcmDdcDevicePrivate))

/* ddc/ci iface tunables */
#define GCM_MAX_MESSAGE_BYTES		127
#define GCM_READ_DELAY_SECS   		0.04f
#define GCM_WRITE_DELAY_SECS   		0.05f

#define GCM_SAVE_CURRENT_SETTINGS	0x0c
#define GCM_SAVE_DELAY_USECS   		200000

/* magic numbers */
#define GCM_MAGIC_BYTE1			0x51	/* host address */
#define GCM_MAGIC_BYTE2			0x80	/* ored with length */
#define GCM_MAGIC_XOR 			0x50	/* initial xor for received frame */

/**
 * GcmDdcDevicePrivate:
 *
 * Private #GcmDdcDevice data
 **/
struct _GcmDdcDevicePrivate
{
	gint			 fd;
	guint			 addr;
	GcmDdcDeviceKind	 kind;
	gchar			*model;
	gchar			*pnpid;
	guint8			*edid_data;
	gsize			 edid_length;
	gchar			*edid_md5;
	GPtrArray		*controls;
	gboolean		 has_controls;
	gboolean		 has_edid;
	gdouble			 required_wait;
	GTimer			*timer;
	GcmVerbose		 verbose;
};

enum {
	PROP_0,
	PROP_HAS_EDID,
	PROP_LAST
};

G_DEFINE_TYPE (GcmDdcDevice, gcm_ddc_device, G_TYPE_OBJECT)

/**
 * gcm_ddc_device_print_hex_data:
 **/
static void
gcm_ddc_device_print_hex_data (const gchar *text, const guchar *data, gsize length)
{
	guint i;

	g_print ("%s: ", text);
	for (i=0; i<length; i++) {
		if (!g_ascii_isprint (data[i]))
			g_print ("%02x [?]  ", data[i]);
		else
			g_print ("%02x [%c]  ", data[i], data[i]);
		if (i % 8 == 7)
			g_print ("\n      ");
	}
	g_print ("\n");
}

/**
 * gcm_ddc_device_set_required_wait:
 **/
static void
gcm_ddc_device_set_required_wait (GcmDdcDevice *device, gdouble delay)
{
	device->priv->required_wait = delay;
}

/**
 * gcm_ddc_device_i2c_write:
 **/
static gboolean
gcm_ddc_device_i2c_write (GcmDdcDevice *device, guint addr, const guchar *data, gsize length, GError **error)
{
	gint i;
	struct i2c_rdwr_ioctl_data msg_rdwr;
	struct i2c_msg i2cmsg;

	g_return_val_if_fail (GCM_IS_DDC_DEVICE(device), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* done, prepare message */
	msg_rdwr.msgs = &i2cmsg;
	msg_rdwr.nmsgs = 1;

	i2cmsg.addr  = addr;
	i2cmsg.flags = 0;
	i2cmsg.len   = length;
	i2cmsg.buf   = (unsigned char *) data;

	/* hit hardware */
	i = ioctl (device->priv->fd, I2C_RDWR, &msg_rdwr);
	if (i < 0 ) {
		g_set_error (error, GCM_DDC_DEVICE_ERROR, GCM_DDC_DEVICE_ERROR_FAILED,
			     "ioctl returned %d", i);
		return FALSE;
	}

	if (device->priv->verbose == GCM_VERBOSE_PROTOCOL)
		gcm_ddc_device_print_hex_data ("Send", data, length);
	return TRUE;
}

/**
 * gcm_ddc_device_i2c_read:
 **/
static gboolean
gcm_ddc_device_i2c_read (GcmDdcDevice *device, guint addr, guchar *data, gsize data_length, gsize *recieved_length, GError **error)
{
	struct i2c_rdwr_ioctl_data msg_rdwr;
	struct i2c_msg i2cmsg;
	gint i;

	g_return_val_if_fail (GCM_IS_DDC_DEVICE(device), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	msg_rdwr.msgs = &i2cmsg;
	msg_rdwr.nmsgs = 1;

	i2cmsg.addr  = addr;
	i2cmsg.flags = I2C_M_RD;
	i2cmsg.len   = data_length;
	i2cmsg.buf   = data;

	/* hit hardware */
	i = ioctl (device->priv->fd, I2C_RDWR, &msg_rdwr);
	if (i < 0) {
		g_set_error (error, GCM_DDC_DEVICE_ERROR, GCM_DDC_DEVICE_ERROR_FAILED,
			     "ioctl returned %d", i);
		return FALSE;
	}

	if (recieved_length != NULL)
		*recieved_length = i2cmsg.len;

	if (device->priv->verbose == GCM_VERBOSE_PROTOCOL)
		gcm_ddc_device_print_hex_data ("Recv", data, i2cmsg.len);
	return TRUE;
}

/**
 * gcm_ddc_device_edid_valid:
 **/
static gboolean
gcm_ddc_device_edid_valid (const guint8 *data, gsize length)
{
	if (length < 0x0f)
		return FALSE;
	if (data[0] != 0 || data[1] != 0xff || data[2] != 0xff || data[3] != 0xff ||
	    data[4] != 0xff || data[5] != 0xff || data[6] != 0xff || data[7] != 0) {
		return FALSE;
	}
	return TRUE;
}

/**
 * gcm_ddc_device_ensure_edid:
 **/
static gboolean
gcm_ddc_device_ensure_edid (GcmDdcDevice *device, GError **error)
{
	gboolean ret = FALSE;
	GError *error_local = NULL;
	gint addr = GCM_DEFAULT_EDID_ADDR;
	guchar buf[128];

	g_return_val_if_fail (GCM_IS_DDC_DEVICE(device), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* already done */
	if (device->priv->has_edid)
		return TRUE;

	/* send edid with offset zero */
	buf[0] = 0;
	if (!gcm_ddc_device_i2c_write (device, addr, buf, 1, &error_local)) {
		g_set_error (error, GCM_DDC_DEVICE_ERROR, GCM_DDC_DEVICE_ERROR_FAILED,
			     "failed to request EDID: %s", error_local->message);
		g_error_free (error_local);
		goto out;
	}

	/* read out data */
	device->priv->edid_data = g_new0 (guint8, 128);
	if (!gcm_ddc_device_i2c_read (device, addr, device->priv->edid_data, 128, &device->priv->edid_length, &error_local)) {
		g_set_error (error, GCM_DDC_DEVICE_ERROR, GCM_DDC_DEVICE_ERROR_FAILED,
			     "failed to recieve EDID: %s", error_local->message);
		g_error_free (error_local);
		goto out;
	}

	/* check valid */
	ret = gcm_ddc_device_edid_valid (device->priv->edid_data, device->priv->edid_length);
	if (!ret) {
		g_set_error (error, GCM_DDC_DEVICE_ERROR, GCM_DDC_DEVICE_ERROR_FAILED,
			     "corrupted EDID at 0x%02x", addr);
		goto out;
	}

	/* get md5 hash */
	device->priv->edid_md5 = g_compute_checksum_for_data (G_CHECKSUM_MD5,
							device->priv->edid_data,
							device->priv->edid_length);

	/* print */
	device->priv->pnpid = g_strdup_printf ("%c%c%c%02X%02X",
		 ((device->priv->edid_data[8] >> 2) & 31) + 'A' - 1,
		 ((device->priv->edid_data[8] & 3) << 3) + (device->priv->edid_data[9] >> 5) + 'A' - 1,
		 (device->priv->edid_data[9] & 31) + 'A' - 1, device->priv->edid_data[11], device->priv->edid_data[10]);
	device->priv->has_edid = TRUE;
out:
	return ret;
}

/**
 * gcm_ddc_device_wait_for_hardware:
 *
 * Stalls execution, allowing write transaction to complete
 **/
static void
gcm_ddc_device_wait_for_hardware (GcmDdcDevice *device)
{
	gdouble elapsed;
	GcmDdcDevicePrivate *priv = device->priv;

	/* only wait if enough time hasn't yet passed */
	elapsed = g_timer_elapsed (priv->timer, NULL);
	if (elapsed < priv->required_wait)
		g_usleep ((priv->required_wait - elapsed) * G_USEC_PER_SEC);
	g_timer_reset (priv->timer);
}

/**
 * gcm_ddc_device_write:
 *
 * write data to ddc/ci at address addr
 **/
gboolean
gcm_ddc_device_write (GcmDdcDevice *device, guchar *data, gsize length, GError **error)
{
	gint i = 0;
	guchar buf[GCM_MAX_MESSAGE_BYTES + 3];
	unsigned xor;
	gboolean ret;

	g_return_val_if_fail (GCM_IS_DDC_DEVICE(device), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* initial xor value */
	xor = ((guchar)device->priv->addr << 1);

	/* put first magic */
	xor ^= (buf[i++] = GCM_MAGIC_BYTE1);

	/* second magic includes message size */
	xor ^= (buf[i++] = GCM_MAGIC_BYTE2 | length);

	/* bytes to send */
	while (length--)
		xor ^= (buf[i++] = *data++);

	/* finally put checksum */
	buf[i++] = xor;

	/* wait for previous write to complete */
	gcm_ddc_device_wait_for_hardware (device);

	/* write to device */
	ret = gcm_ddc_device_i2c_write (device, device->priv->addr, buf, i, error);
	if (!ret)
		goto out;

	/* we have to wait at least this much time before submitting another command */
	gcm_ddc_device_set_required_wait (device, GCM_WRITE_DELAY_SECS);
out:
	return ret;
}

/**
 * gcm_ddc_device_read:
 *
 * Read ddc/ci formatted frame from ddc/ci
 **/
gboolean
gcm_ddc_device_read (GcmDdcDevice *device, guchar *data, gsize data_length, gsize *recieved_length, GError **error)
{
	guchar buf[GCM_MAX_MESSAGE_BYTES];
	guchar xor = GCM_MAGIC_XOR;
	guint i;
	gsize len;
	gboolean ret;

	g_return_val_if_fail (GCM_IS_DDC_DEVICE(device), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* wait for previous write to complete */
	gcm_ddc_device_wait_for_hardware (device);

	/* get data */
	ret = gcm_ddc_device_i2c_read (device, device->priv->addr, buf, data_length + 3, recieved_length, error);
	if (!ret)
		goto out;

	/* validate answer */
	if (buf[0] != device->priv->addr * 2) { /* busy ??? */
		g_set_error (error, GCM_DDC_DEVICE_ERROR, GCM_DDC_DEVICE_ERROR_FAILED,
			     "Invalid response, first byte is 0x%02x, should be 0x%02x",
			     buf[0], device->priv->addr * 2);
		if (device->priv->verbose == GCM_VERBOSE_PROTOCOL)
			gcm_ddc_device_print_hex_data ("Bugz", buf, data_length + 3);
		ret = FALSE;
		goto out;
	}

	if ((buf[1] & GCM_MAGIC_BYTE2) == 0) {
		/* Fujitsu Siemens P19-2 and NEC LCD 1970NX send wrong magic when reading caps. */
		g_debug ( "Invalid response, magic is 0x%02x, correcting", buf[1]);
	}

	len = buf[1] & ~GCM_MAGIC_BYTE2;
	if (len > data_length || len > sizeof(buf)) {
		g_set_error (error, GCM_DDC_DEVICE_ERROR, GCM_DDC_DEVICE_ERROR_FAILED,
			     "Invalid response, length is %d, should be %d at most",
			     len, data_length);
		ret = FALSE;
		goto out;
	}

	/* get the xor value */
	for (i = 0; i < len + 3; i++)
		xor ^= buf[i];
	if (xor != 0) {
		g_set_error (error, GCM_DDC_DEVICE_ERROR, GCM_DDC_DEVICE_ERROR_FAILED,
			     "Invalid response, corrupted data - xor is 0x%02x, length 0x%02x", xor, len);
		if (device->priv->verbose == GCM_VERBOSE_PROTOCOL)
			gcm_ddc_device_print_hex_data ("Bugz", buf, data_length + 3);
		ret = FALSE;
		goto out;
	}

	/* copy payload data */
	memcpy (data, buf + 2, len);
	if (recieved_length != NULL)
		*recieved_length = len;

	/* we have to wait at least this much time before reading the results */
	gcm_ddc_device_set_required_wait (device, GCM_READ_DELAY_SECS);
out:
	return ret;
}

/**
 * gcm_ddc_device_capabilities_request:
 *
 * read capabilities raw data of ddc/ci
 **/
static gint
gcm_ddc_device_capabilities_request (GcmDdcDevice *device, guint offset, guchar *data, gsize data_length, gsize *recieved_length, GError **error)
{
	guchar buf[3];

	g_return_val_if_fail (GCM_IS_DDC_DEVICE(device), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	buf[0] = GCM_CAPABILITIES_REQUEST;
	buf[1] = offset >> 8;
	buf[2] = offset & 255;

	if (!gcm_ddc_device_write (device, buf, sizeof(buf), error))
		return FALSE;

	return gcm_ddc_device_read (device, data, data_length, recieved_length, error);
}

/**
 * gcm_ddc_device_add_control:
 **/
static gboolean
gcm_ddc_device_add_control (GcmDdcDevice *device, const gchar *index_str, const gchar *controls_str)
{
	GcmDdcControl *control;
	control = gcm_ddc_control_new ();
	gcm_ddc_control_set_verbose (control, device->priv->verbose);
	gcm_ddc_control_set_device (control, device);
	gcm_ddc_control_parse (control, strtoul (index_str, NULL, 16), controls_str);
	g_ptr_array_add (device->priv->controls, control);
	return TRUE;
}

/**
 * gcm_ddc_device_set_device_property:
 **/
static gboolean
gcm_ddc_device_set_device_property (GcmDdcDevice *device, const gchar *key, const gchar *value)
{
	if (device->priv->verbose == GCM_VERBOSE_OVERVIEW)
		g_debug ("key=%s, value=%s", key, value);
	if (g_strcmp0 (key, "type") == 0) {
		if (g_strcmp0 (value, "lcd") == 0)
			device->priv->kind = GCM_DDC_DEVICE_KIND_LCD;
		else if (g_strcmp0 (value, "crt") == 0)
			device->priv->kind = GCM_DDC_DEVICE_KIND_CRT;
	} else if (g_strcmp0 (key, "model") == 0)
		device->priv->model = g_strdup (value);
	else if (g_strcmp0 (key, "vcp") == 0) {
		guint i;
		gchar *tmp;
		gchar *index_str;
		gchar *caps_str = NULL;
		guint refcount = 0;

		tmp = g_strdup (value);
		index_str = tmp;
		for (i=0; tmp[i] != '\0'; i++) {
			if (tmp[i] == '(') {
				tmp[i] = '\0';
				refcount++;
				caps_str = &tmp[i] + 1;
			} else if (tmp[i] == ')') {
				tmp[i] = '\0';
				refcount--;
			} else if (tmp[i] == ' ' && refcount == 0) {
				tmp[i] = '\0';
				gcm_ddc_device_add_control (device, index_str, caps_str);
				index_str = &tmp[i] + 1;
				caps_str = NULL;
			}
		}
		gcm_ddc_device_add_control (device, index_str, caps_str);
	}
	return TRUE;
}

/**
 * gcm_ddc_device_parse_caps:
 **/
static gboolean
gcm_ddc_device_parse_caps (GcmDdcDevice *device, const gchar *caps)
{
	guint i;
	gchar *tmp = g_strdup (caps);
	gchar *key = tmp+1;
	gchar *value = NULL;
	guint refcount = 0;

	/* decode string */
	for (i=1; tmp[i] != '\0'; i++) {
		if (tmp[i] == '(') {
			if (refcount++ == 0) {
				tmp[i] = '\0';
				value = &tmp[i+1];
			}
		} else if (tmp[i] == ')') {
			if (--refcount == 0) {
				tmp[i] = '\0';
				gcm_ddc_device_set_device_property (device, key, value);
				key = &tmp[i]+1;
			}
		}
	}

	g_free (tmp);
	return TRUE;
}

/**
 * gcm_ddc_device_ensure_controls:
 **/
static gboolean
gcm_ddc_device_ensure_controls (GcmDdcDevice *device, GError **error)
{
	guchar buf[64];
	gint offset = 0;
	gsize len;
	gint retries = 5;
	GString *string;
	gboolean ret = FALSE;

	g_return_val_if_fail (GCM_IS_DDC_DEVICE(device), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* already done */
	if (device->priv->has_controls)
		return TRUE;

	/* allocate space for the controls */
	string = g_string_new ("");
	do {
		/* we're shit out of luck, Brian */
		if (retries == 0)
			goto out;

		/* clear previous error */
		g_clear_error (error);

		/* try to read */
		ret = gcm_ddc_device_capabilities_request (device, offset, buf, sizeof(buf), &len, error);
		if (!ret) {
			if (device->priv->verbose == GCM_VERBOSE_PROTOCOL)
				g_warning ("Failed to read capabilities offset 0x%02x.", offset);
			retries--;
			continue;
		}

		/* not enough data */
		if (len < 3) {
			if (device->priv->verbose == GCM_VERBOSE_PROTOCOL)
				g_warning ("Not enough capabilities data at offset 0x%02x.", offset);
			retries--;
			continue;
		}

		/* check response */
		if (buf[0] != GCM_CAPABILITIES_REPLY) {
			if (device->priv->verbose == GCM_VERBOSE_PROTOCOL)
				g_warning ("Not correct capabilities reply at offset 0x%02x.", offset);
			retries--;
			continue;
		}

		/* check offset */
		if ((buf[1] * 256 + buf[2]) != offset) {
			if (device->priv->verbose == GCM_VERBOSE_PROTOCOL)
				g_warning ("Not correct capabilities offset at offset 0x%02x.", offset);
			retries--;
			continue;
		}

		/* add to results */
		g_string_append_len (string, (const gchar *) buf + 3, len - 3);
		offset += len - 3;
		retries = 3;
	} while (len != 3);

	if (device->priv->verbose == GCM_VERBOSE_OVERVIEW)
		g_debug ("raw caps: %s", string->str);

	/* parse */
	ret = gcm_ddc_device_parse_caps (device, string->str);
	if (!ret) {
		g_set_error (error, GCM_DDC_DEVICE_ERROR, GCM_DDC_DEVICE_ERROR_FAILED,
			     "failed to parse caps");
		goto out;
	}

	/* success */
	device->priv->has_controls = TRUE;
out:
	g_string_free (string, TRUE);
	return ret;
}

/**
 * gcm_ddc_device_save:
 **/
gboolean
gcm_ddc_device_save (GcmDdcDevice *device, GError **error)
{
	gboolean ret = FALSE;
	GcmDdcControl *control;

	g_return_val_if_fail (GCM_IS_DDC_DEVICE(device), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* get control */
	control = gcm_ddc_device_get_control_by_id (device, GCM_SAVE_CURRENT_SETTINGS, error);
	if (control == NULL)
		goto out;

	/* run it */
	ret = gcm_ddc_control_run (control, error);
	if (!ret)
		goto out;

	/* super long delay to allow for saving to eeprom */
	g_usleep (GCM_SAVE_DELAY_USECS);
out:
	return ret;
}

/**
 * gcm_ddc_device_startup:
 **/
static gboolean
gcm_ddc_device_startup (GcmDdcDevice *device, GError **error)
{
	GcmDdcControl *control;
	gboolean ret = FALSE;
	if (device->priv->pnpid != NULL && g_str_has_prefix (device->priv->pnpid, "SAM")) {
		control = gcm_ddc_device_get_control_by_id (device, GCM_ENABLE_APPLICATION_REPORT, error);
		if (control == NULL)
			goto out;
		ret = gcm_ddc_control_set (control, GCM_CTRL_ENABLE, error);
	} else {
		/* this is not fatal if it's not found */
		control = gcm_ddc_device_get_control_by_id (device, GCM_COMMAND_PRESENCE, NULL);
		if (control == NULL) {
			ret = TRUE;
			goto out;
		}
		ret = gcm_ddc_control_run (control, error);
	}
out:
	return ret;
}

/**
 * gcm_ddc_device_close:
 **/
gboolean
gcm_ddc_device_close (GcmDdcDevice *device, GError **error)
{
	GcmDdcControl *control;
	gboolean ret = FALSE;

	g_return_val_if_fail (GCM_IS_DDC_DEVICE(device), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (device->priv->pnpid != NULL && g_str_has_prefix (device->priv->pnpid, "SAM")) {
		control = gcm_ddc_device_get_control_by_id (device, GCM_ENABLE_APPLICATION_REPORT, error);
		if (control == NULL)
			goto out;
		ret = gcm_ddc_control_set (control, GCM_CTRL_DISABLE, error);
	} else {
		ret = TRUE;
	}
out:
	return ret;
}

/**
 * gcm_ddc_device_open:
 **/
gboolean
gcm_ddc_device_open (GcmDdcDevice *device, const gchar *filename, GError **error)
{
	gboolean ret;

	g_return_val_if_fail (GCM_IS_DDC_DEVICE(device), FALSE);
	g_return_val_if_fail (filename != NULL, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* ensure we have the module loaded */
	ret = g_file_test ("/sys/module/i2c_dev/srcversion", G_FILE_TEST_EXISTS);
	if (!ret) {
		g_set_error_literal (error, GCM_DDC_DEVICE_ERROR, GCM_DDC_DEVICE_ERROR_FAILED,
			     "unable to use I2C, you need to 'modprobe i2c-dev'");
		goto out;
	}

	/* open file */
	device->priv->fd = open (filename, O_RDWR);
	if (device->priv->fd < 0) {
		ret = FALSE;
		g_set_error (error, GCM_DDC_DEVICE_ERROR, GCM_DDC_DEVICE_ERROR_FAILED,
			     "failed to open: %i", device->priv->fd);
		goto out;
	}

	/* enable interface (need edid for pnpid) */
	ret = gcm_ddc_device_ensure_edid (device, error);
	if (!ret)
		goto out;

	/* startup for samsung mode */
	ret = gcm_ddc_device_startup (device, error);
	if (!ret)
		goto out;
out:
	return ret;
}

/**
 * gcm_ddc_device_get_controls:
 **/
GPtrArray *
gcm_ddc_device_get_controls (GcmDdcDevice *device, GError **error)
{
	gboolean ret;
	GPtrArray *controls = NULL;

	g_return_val_if_fail (GCM_IS_DDC_DEVICE(device), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	/* get capabilities */
	ret = gcm_ddc_device_ensure_controls (device, error);
	if (!ret)
		goto out;

	/* success */
	controls = g_ptr_array_ref (device->priv->controls);
out:
	return controls;
}

/**
 * gcm_ddc_device_get_control_by_id:
 **/
GcmDdcControl *
gcm_ddc_device_get_control_by_id (GcmDdcDevice *device, guchar id, GError **error)
{
	guint i;
	gboolean ret;
	GcmDdcControl *control = NULL;
	GcmDdcControl *control_tmp = NULL;

	g_return_val_if_fail (GCM_IS_DDC_DEVICE(device), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	/* get capabilities */
	ret = gcm_ddc_device_ensure_controls (device, error);
	if (!ret)
		goto out;

	/* search each control */
	for (i=0; i<device->priv->controls->len; i++) {
		control_tmp = g_ptr_array_index (device->priv->controls, i);
		if (gcm_ddc_control_get_id (control_tmp) == id) {
			control = g_object_ref (control_tmp);
			break;
		}
	}

	/* failure */
	if (control == NULL) {
		g_set_error (error, GCM_DDC_DEVICE_ERROR, GCM_DDC_DEVICE_ERROR_FAILED,
			     "could not find a control id 0x%02x", (guint) id);
	}
out:
	return control;
}

/**
 * gcm_ddc_device_get_edid:
 **/
const guint8 *
gcm_ddc_device_get_edid	(GcmDdcDevice *device, gsize *length, GError **error)
{
	gboolean ret;
	const guint8 *data = NULL;

	g_return_val_if_fail (GCM_IS_DDC_DEVICE(device), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	/* get capabilities */
	ret = gcm_ddc_device_ensure_edid (device, error);
	if (!ret)
		goto out;

	/* success */
	data = device->priv->edid_data;
	if (length != NULL)
		*length = device->priv->edid_length;
out:
	return data;
}

/**
 * gcm_ddc_device_get_edid_md5:
 **/
const gchar *
gcm_ddc_device_get_edid_md5 (GcmDdcDevice *device, GError **error)
{
	gboolean ret;
	const gchar *data = NULL;

	g_return_val_if_fail (GCM_IS_DDC_DEVICE(device), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	/* get capabilities */
	ret = gcm_ddc_device_ensure_edid (device, error);
	if (!ret)
		goto out;

	/* success */
	data = device->priv->edid_md5;
out:
	return data;
}

/**
 * gcm_ddc_device_get_pnpid:
 **/
const gchar *
gcm_ddc_device_get_pnpid (GcmDdcDevice	*device, GError **error)
{
	gboolean ret;
	const gchar *pnpid = NULL;

	g_return_val_if_fail (GCM_IS_DDC_DEVICE(device), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	/* get capabilities */
	ret = gcm_ddc_device_ensure_edid (device, error);
	if (!ret)
		goto out;

	/* success */
	pnpid = device->priv->pnpid;
out:
	return pnpid;
}

/**
 * gcm_ddc_device_get_model:
 **/
const gchar *
gcm_ddc_device_get_model (GcmDdcDevice	*device, GError **error)
{
	gboolean ret;
	const gchar *model = NULL;

	g_return_val_if_fail (GCM_IS_DDC_DEVICE(device), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	/* get capabilities */
	ret = gcm_ddc_device_ensure_controls (device, error);
	if (!ret)
		goto out;

	/* success */
	model = device->priv->model;
out:
	return model;
}

/**
 * gcm_ddc_device_get_kind:
 **/
GcmDdcDeviceKind
gcm_ddc_device_get_kind (GcmDdcDevice *device, GError **error)
{
	gboolean ret;
	GcmDdcDeviceKind kind = GCM_DDC_DEVICE_KIND_UNKNOWN;

	g_return_val_if_fail (GCM_IS_DDC_DEVICE(device), kind);
	g_return_val_if_fail (error == NULL || *error == NULL, kind);

	/* get capabilities */
	ret = gcm_ddc_device_ensure_controls (device, error);
	if (!ret)
		goto out;

	/* success */
	kind = device->priv->kind;
out:
	return kind;
}

/**
 * gcm_ddc_device_set_verbose:
 **/
void
gcm_ddc_device_set_verbose (GcmDdcDevice *device, GcmVerbose verbose)
{
	g_return_if_fail (GCM_IS_DDC_DEVICE(device));
	device->priv->verbose = verbose;
}

/**
 * gcm_ddc_device_error_quark:
 *
 * Return value: Our personal error quark.
 *
 * Since: 0.0.1
 **/
GQuark
gcm_ddc_device_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("gcm_ddc_device_error");
	return quark;
}

/**
 * gcm_ddc_device_get_property:
 **/
static void
gcm_ddc_device_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GcmDdcDevice *device = GCM_DDC_DEVICE (object);
	GcmDdcDevicePrivate *priv = device->priv;

	switch (prop_id) {
	case PROP_HAS_EDID:
		g_value_set_boolean (value, priv->has_edid);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gcm_ddc_device_set_property:
 **/
static void
gcm_ddc_device_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gcm_ddc_device_class_init:
 **/
static void
gcm_ddc_device_class_init (GcmDdcDeviceClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gcm_ddc_device_finalize;
	object_class->get_property = gcm_ddc_device_get_property;
	object_class->set_property = gcm_ddc_device_set_property;

	/**
	 * GcmDdcDevice:has-coldplug:
	 *
	 * Since: 0.0.1
	 */
	pspec = g_param_spec_boolean ("has-coldplug", NULL, "if there are no transactions in progress on this device",
				      TRUE,
				      G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_HAS_EDID, pspec);

	g_type_class_add_private (klass, sizeof (GcmDdcDevicePrivate));
}

/**
 * gcm_ddc_device_init:
 **/
static void
gcm_ddc_device_init (GcmDdcDevice *device)
{
	device->priv = GCM_DDC_DEVICE_GET_PRIVATE (device);
	device->priv->kind = GCM_DDC_DEVICE_KIND_UNKNOWN;
	device->priv->addr = GCM_DEFAULT_DDCCI_ADDR;
	device->priv->controls = g_ptr_array_new ();
	device->priv->fd = -1;
	/* assume the hardware is busy */
	device->priv->required_wait = GCM_WRITE_DELAY_SECS;
	device->priv->timer = g_timer_new ();
}

/**
 * gcm_ddc_device_finalize:
 **/
static void
gcm_ddc_device_finalize (GObject *object)
{
	GcmDdcDevice *device = GCM_DDC_DEVICE (object);
	GcmDdcDevicePrivate *priv = device->priv;

	g_return_if_fail (GCM_IS_DDC_DEVICE(device));
	if (priv->fd > 0)
		close (priv->fd);
	g_free (priv->model);
	g_free (priv->pnpid);
	g_free (priv->edid_data);
	g_free (priv->edid_md5);
	g_timer_destroy (priv->timer);
	g_ptr_array_free (priv->controls, TRUE);

	G_OBJECT_CLASS (gcm_ddc_device_parent_class)->finalize (object);
}

/**
 * gcm_ddc_device_new:
 *
 * GcmDdcDevice is a nice GObject wrapper for gcm.
 *
 * Return value: A new %GcmDdcDevice instance
 *
 * Since: 0.0.1
 **/
GcmDdcDevice *
gcm_ddc_device_new (void)
{
	GcmDdcDevice *device;
	device = g_object_new (GCM_TYPE_DDC_DEVICE, NULL);
	return GCM_DDC_DEVICE (device);
}

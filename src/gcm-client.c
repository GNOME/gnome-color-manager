/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009 Richard Hughes <richard@hughsie.com>
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
 * SECTION:gcm-client
 * @short_description: Client object to hold an array of devices.
 *
 * This object holds an array of %GcmDevices, and watches both udev and xorg
 * for changes. If a device is added or removed then a signal is fired.
 */

#include "config.h"

#include <glib/gi18n.h>
#include <glib-object.h>
#include <gudev/gudev.h>
#include <libgnomeui/gnome-rr.h>
#include <cups/cups.h>

#include "gcm-client.h"
#include "gcm-device-xrandr.h"
#include "gcm-device-udev.h"
#include "gcm-device-cups.h"
#include "gcm-screen.h"
#include "gcm-utils.h"

#include "egg-debug.h"

static void     gcm_client_finalize	(GObject     *object);

#define GCM_CLIENT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GCM_TYPE_CLIENT, GcmClientPrivate))

static void gcm_client_xrandr_add (GcmClient *client, GnomeRROutput *output);

/**
 * GcmClientPrivate:
 *
 * Private #GcmClient data
 **/
struct _GcmClientPrivate
{
	gchar				*display_name;
	GPtrArray			*array;
	GUdevClient			*gudev_client;
	GcmScreen			*screen;
	http_t				*http;
};

enum {
	PROP_0,
	PROP_DISPLAY_NAME,
	PROP_LAST
};

enum {
	SIGNAL_ADDED,
	SIGNAL_REMOVED,
	SIGNAL_LAST
};

static guint signals[SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE (GcmClient, gcm_client, G_TYPE_OBJECT)

/**
 * gcm_client_get_devices:
 *
 * @client: a valid %GcmClient instance
 *
 * Gets the device list.
 *
 * Return value: an array, free with g_ptr_array_unref()
 **/
GPtrArray *
gcm_client_get_devices (GcmClient *client)
{
	g_return_val_if_fail (GCM_IS_CLIENT (client), NULL);
	return g_ptr_array_ref (client->priv->array);
}

/**
 * gcm_client_get_device_by_id:
 *
 * @client: a valid %GcmClient instance
 * @id: the device identifer
 *
 * Gets a device.
 *
 * Return value: a valid %GcmDevice or %NULL. Free with g_object_unref()
 **/
GcmDevice *
gcm_client_get_device_by_id (GcmClient *client, const gchar *id)
{
	guint i;
	GcmDevice *device = NULL;
	GcmDevice *device_tmp;
	const gchar *id_tmp;
	GcmClientPrivate *priv = client->priv;

	g_return_val_if_fail (GCM_IS_CLIENT (client), NULL);
	g_return_val_if_fail (id != NULL, NULL);

	/* find device */
	for (i=0; i<priv->array->len; i++) {
		device_tmp = g_ptr_array_index (priv->array, i);
		id_tmp = gcm_device_get_id (device_tmp);
		if (g_strcmp0 (id, id_tmp) == 0) {
			device = g_object_ref (device_tmp);
			break;
		}
	}

	return device;
}

/**
 * gcm_client_get_id_for_udev_device:
 **/
static gchar *
gcm_client_get_id_for_udev_device (GUdevDevice *udev_device)
{
	gchar *id;

	/* get id */
	id = g_strdup_printf ("sysfs_%s_%s",
			      g_udev_device_get_property (udev_device, "ID_VENDOR"),
			      g_udev_device_get_property (udev_device, "ID_MODEL"));

	/* replace unsafe chars */
	gcm_utils_alphanum_lcase (id);
	return id;
}

/**
 * gcm_client_gudev_remove:
 **/
static void
gcm_client_gudev_remove (GcmClient *client, GUdevDevice *udev_device)
{
	GcmDevice *device;
	gchar *id;
	gboolean ret;
	GcmClientPrivate *priv = client->priv;

	/* generate id */
	id = gcm_client_get_id_for_udev_device (udev_device);

	/* do we have a device that matches */
	device = gcm_client_get_device_by_id (client, id);
	if (device == NULL)
		goto out;

	/* remove device first as we hold a reference to it */
	ret = g_ptr_array_remove (priv->array, device);
	if (!ret) {
		egg_warning ("failed to remove %s", id);
		goto out;
	}

	/* emit a signal */
	egg_debug ("emit removed: %s", id);
	g_signal_emit (client, signals[SIGNAL_REMOVED], 0, device);
out:
	if (device != NULL)
		g_object_unref (device);
	g_free (id);
}

/**
 * gcm_client_gudev_add:
 **/
static void
gcm_client_gudev_add (GcmClient *client, GUdevDevice *udev_device)
{
	GcmDevice *device = NULL;
	GcmDevice *device_tmp = NULL;
	gboolean ret;
	const gchar *value;
	GError *error = NULL;
	GcmClientPrivate *priv = client->priv;

	/* matches our udev rules, which we can change without recompiling */
	value = g_udev_device_get_property (udev_device, "GCM_DEVICE");
	if (value == NULL)
		goto out;

	/* create new device */
	device = gcm_device_udev_new ();
	ret = gcm_device_udev_set_from_device (device, udev_device, &error);
	if (!ret) {
		egg_debug ("failed to set for device: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* we might have a previous saved device with this ID, in which case nuke it */
	device_tmp = gcm_client_get_device_by_id (client, gcm_device_get_id (device));
	if (device_tmp != NULL) {

		/* remove from the array */
		g_ptr_array_remove (client->priv->array, device_tmp);

		/* emit a signal */
		egg_debug ("emit removed: %s", gcm_device_get_id (device));
		g_signal_emit (client, signals[SIGNAL_REMOVED], 0, device_tmp);
	}

	/* load the device */
	ret = gcm_device_load (device, &error);
	if (!ret) {
		egg_warning ("failed to load: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* add to list */
	g_ptr_array_add (priv->array, g_object_ref (device));

	/* signal the addition */
	egg_debug ("emit: added %s to device list", gcm_device_get_id (device));
	g_signal_emit (client, signals[SIGNAL_ADDED], 0, device);
out:
	if (device != NULL)
		g_object_unref (device);
	if (device_tmp != NULL)
		g_object_unref (device_tmp);
}

/**
 * gcm_client_uevent_cb:
 **/
static void
gcm_client_uevent_cb (GUdevClient *gudev_client, const gchar *action, GUdevDevice *udev_device, GcmClient *client)
{
	if (g_strcmp0 (action, "remove") == 0)
		gcm_client_gudev_remove (client, udev_device);
	else if (g_strcmp0 (action, "add") == 0)
		gcm_client_gudev_add (client, udev_device);
}

/**
 * gcm_client_add_connected_devices_udev:
 **/
static gboolean
gcm_client_add_connected_devices_udev (GcmClient *client, GError **error)
{
	GList *devices;
	GList *l;
	GUdevDevice *udev_device;
	GcmClientPrivate *priv = client->priv;

	/* get all USB devices */
	devices = g_udev_client_query_by_subsystem (priv->gudev_client, "usb");
	for (l = devices; l != NULL; l = l->next) {
		udev_device = l->data;
		gcm_client_gudev_add (client, udev_device);
	}

	/* get all video4linux devices */
	devices = g_udev_client_query_by_subsystem (priv->gudev_client, "video4linux");
	for (l = devices; l != NULL; l = l->next) {
		udev_device = l->data;
		gcm_client_gudev_add (client, udev_device);
	}

	g_list_foreach (devices, (GFunc) g_object_unref, NULL);
	g_list_free (devices);
	return TRUE;
}

/**
 * gcm_client_get_device_by_window_covered:
 **/
static gfloat
gcm_client_get_device_by_window_covered (gint x, gint y, gint width, gint height,
				         gint window_x, gint window_y, gint window_width, gint window_height)
{
	gfloat covered = 0.0f;
	gint overlap_x;
	gint overlap_y;

	/* to the right of the window */
	if (window_x > x + width)
		goto out;
	if (window_y > y + height)
		goto out;

	/* to the left of the window */
	if (window_x + window_width < x)
		goto out;
	if (window_y + window_height < y)
		goto out;

	/* get the overlaps */
	overlap_x = MIN((window_x + window_width - x), width) - MAX(window_x - x, 0);
	overlap_y = MIN((window_y + window_height - y), height) - MAX(window_y - y, 0);

	/* not in this window */
	if (overlap_x <= 0)
		goto out;
	if (overlap_y <= 0)
		goto out;

	/* get the coverage */
	covered = (gfloat) (overlap_x * overlap_y) / (gfloat) (window_width * window_height);
	egg_debug ("overlap_x=%i,overlap_y=%i,covered=%f", overlap_x, overlap_y, covered);
out:
	return covered;
}

/**
 * gcm_client_get_device_by_window:
 **/
GcmDevice *
gcm_client_get_device_by_window (GcmClient *client, GdkWindow *window)
{
	gfloat covered;
	gfloat covered_max = 0.0f;
	gint window_width, window_height;
	gint window_x, window_y;
	gint x, y;
	guint i;
	guint len = 0;
	guint width, height;
	GnomeRRMode *mode;
	GnomeRROutput *output_best = NULL;
	GnomeRROutput **outputs;
	GcmDevice *device = NULL;
	gchar *id = NULL;

	/* get the window parameters, in root co-ordinates */
	gdk_window_get_origin (window, &window_x, &window_y);
	gdk_drawable_get_size (GDK_DRAWABLE(window), &window_width, &window_height);

	/* get list of updates */
	outputs = gcm_screen_get_outputs (client->priv->screen, NULL);
	if (outputs == NULL)
		goto out;

	/* find length */
	for (i=0; outputs[i] != NULL; i++)
		len++;

	/* go through each option */
	for (i=0; i<len; i++) {

		/* not interesting */
		if (!gnome_rr_output_is_connected (outputs[i]))
			continue;

		/* get details about the output */
		gnome_rr_output_get_position (outputs[i], &x, &y);
		mode = gnome_rr_output_get_current_mode (outputs[i]);
		width = gnome_rr_mode_get_width (mode);
		height = gnome_rr_mode_get_height (mode);
		egg_debug ("%s: %ix%i -> %ix%i (%ix%i -> %ix%i)", gnome_rr_output_get_name (outputs[i]),
			   x, y, x+width, y+height,
			   window_x, window_y, window_x+window_width, window_y+window_height);

		/* get the fraction of how much the window is covered */
		covered = gcm_client_get_device_by_window_covered (x, y, width, height,
								   window_x, window_y, window_width, window_height);

		/* keep a running total of which one is best */
		if (covered > 0.01f && covered > covered_max) {
			output_best = outputs[i];

			/* optimize */
			if (covered > 0.99) {
				egg_debug ("all in one window, no need to search other windows");
				goto out;
			}

			/* keep looking */
			covered_max = covered;
			egg_debug ("personal best of %f for %s", covered, gnome_rr_output_get_name (output_best));
		}
	}
out:
	/* if we found an output, get the device */
	if (output_best != NULL) {
		GcmDevice *device_tmp;
		device_tmp = gcm_device_xrandr_new ();
		gcm_device_xrandr_set_from_output (device_tmp, output_best, NULL);
		device = gcm_client_get_device_by_id (client, gcm_device_get_id (device_tmp));
		g_free (device_tmp);
	}
	g_free (id);
	return device;
}

/**
 * gcm_client_xrandr_add:
 **/
static void
gcm_client_xrandr_add (GcmClient *client, GnomeRROutput *output)
{
	gboolean ret;
	gboolean connected;
	GError *error = NULL;
	GcmDevice *device = NULL;
	GcmDevice *device_tmp = NULL;
	GcmClientPrivate *priv = client->priv;

	/* if nothing connected then ignore */
	ret = gnome_rr_output_is_connected (output);
	if (!ret) {
		egg_debug ("%s is not connected", gnome_rr_output_get_name (output));
		goto out;
	}

	/* create new device */
	device = gcm_device_xrandr_new ();
	ret = gcm_device_xrandr_set_from_output (device, output, &error);
	if (!ret) {
		egg_debug ("failed to set for output: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* we might have a previous saved device with this ID, in which case nuke it */
	device_tmp = gcm_client_get_device_by_id (client, gcm_device_get_id (device));
	if (device_tmp != NULL) {

		/* old device not connected */
		g_object_get (device_tmp,
			      "connected", &connected,
			      NULL);

		/* this is just a dupe */
		if (connected) {
			egg_debug ("ignoring dupe");
			goto out;
		}

		/* remove from the array */
		g_ptr_array_remove (client->priv->array, device_tmp);

		/* emit a signal */
		egg_debug ("emit removed: %s (unconnected device that has just been connected)", gcm_device_get_id (device_tmp));
		g_signal_emit (client, signals[SIGNAL_REMOVED], 0, device_tmp);
	}

	/* load the device */
	ret = gcm_device_load (device, &error);
	if (!ret) {
		egg_warning ("failed to load: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* add to the array */
	g_ptr_array_add (priv->array, g_object_ref (device));

	/* signal the addition */
	egg_debug ("emit: added %s to device list", gcm_device_get_id (device));
	g_signal_emit (client, signals[SIGNAL_ADDED], 0, device);
out:
	if (device != NULL)
		g_object_unref (device);
	if (device_tmp != NULL)
		g_object_unref (device_tmp);
}

/**
 * gcm_client_add_connected_devices_xrandr:
 **/
static gboolean
gcm_client_add_connected_devices_xrandr (GcmClient *client, GError **error)
{
	GnomeRROutput **outputs;
	guint i;
	GcmClientPrivate *priv = client->priv;

	outputs = gcm_screen_get_outputs (priv->screen, error);
	if (outputs == NULL)
		return FALSE;
	for (i=0; outputs[i] != NULL; i++)
		gcm_client_xrandr_add (client, outputs[i]);
	return TRUE;
}

/**
 * gcm_client_cups_add:
 **/
static void
gcm_client_cups_add (GcmClient *client, cups_dest_t dest)
{
	gboolean ret;
	GError *error = NULL;
	GcmDevice *device = NULL;
	GcmClientPrivate *priv = client->priv;

	/* create new device */
	device = gcm_device_xrandr_new ();
	ret = gcm_device_cups_set_from_dest (device, priv->http, dest, &error);
	if (!ret) {
		egg_debug ("failed to set for output: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* load the device */
	ret = gcm_device_load (device, &error);
	if (!ret) {
		egg_warning ("failed to load: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* add to the array */
	g_ptr_array_add (priv->array, g_object_ref (device));

	/* signal the addition */
	egg_debug ("emit: added %s to device list", gcm_device_get_id (device));
	g_signal_emit (client, signals[SIGNAL_ADDED], 0, device);
out:
	if (device != NULL)
		g_object_unref (device);
}

/**
 * gcm_client_add_connected_devices_cups:
 **/
static gboolean
gcm_client_add_connected_devices_cups (GcmClient *client, GError **error)
{
	gint num_dests;
	cups_dest_t *dests;
	gint i;
	GcmClientPrivate *priv = client->priv;

	num_dests = cupsGetDests2 (priv->http, &dests);
	egg_debug ("got %i printers", num_dests);

	/* get printers on the local server */
	for (i = 0; i < num_dests; i++)
		gcm_client_cups_add (client, dests[i]);
	cupsFreeDests (num_dests, dests);
	return TRUE;
}

/**
 * gcm_client_add_unconnected_device:
 **/
static void
gcm_client_add_unconnected_device (GcmClient *client, GKeyFile *keyfile, const gchar *id)
{
	gchar *title;
	gchar *type_text = NULL;
	GcmDeviceTypeEnum type;
	GcmDevice *device = NULL;
	gboolean ret;
	GError *error = NULL;
	GcmClientPrivate *priv = client->priv;

	/* add new device */
	title = g_key_file_get_string (keyfile, id, "title", NULL);
	if (title == NULL)
		goto out;
	type_text = g_key_file_get_string (keyfile, id, "type", NULL);
	type = gcm_device_type_enum_from_string (type_text);
	if (type == GCM_DEVICE_TYPE_ENUM_UNKNOWN)
		goto out;

	/* create device or specified type */
	if (type == GCM_DEVICE_TYPE_ENUM_DISPLAY) {
		device = gcm_device_xrandr_new ();
	} else if (type == GCM_DEVICE_TYPE_ENUM_PRINTER) {
		device = gcm_device_cups_new ();
	} else if (type == GCM_DEVICE_TYPE_ENUM_CAMERA) {
		/* FIXME: use GPhoto? */
		device = gcm_device_udev_new ();
	} else if (type == GCM_DEVICE_TYPE_ENUM_SCANNER) {
		/* FIXME: use SANE? */
		device = gcm_device_udev_new ();
	} else {
		egg_warning ("device type internal error");
		goto out;
	}

	/* create device */
	g_object_set (device,
		      "type", type,
		      "id", id,
		      "connected", FALSE,
		      "title", title,
		      "saved", TRUE,
		      "colorspace", GCM_COLORSPACE_ENUM_RGB, //FIXME: should get from config file
		      NULL);

	/* load the device */
	ret = gcm_device_load (device, &error);
	if (!ret) {
		egg_warning ("failed to load: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* add to list */
	g_ptr_array_add (priv->array, g_object_ref (device));

	/* signal the addition */
	egg_debug ("emit: added %s to device list", id);
	g_signal_emit (client, signals[SIGNAL_ADDED], 0, device);
out:
	if (device != NULL)
		g_object_unref (device);
	g_free (type_text);
	g_free (title);
}

/**
 * gcm_client_add_saved:
 **/
gboolean
gcm_client_add_saved (GcmClient *client, GError **error)
{
	gboolean ret;
	gchar *filename;
	GKeyFile *keyfile;
	gchar **groups = NULL;
	guint i;
	GcmDevice *device;

	/* get the config file */
	filename = gcm_utils_get_default_config_location ();
	egg_debug ("using %s", filename);

	/* load the config file */
	keyfile = g_key_file_new ();
	ret = g_key_file_load_from_file (keyfile, filename, G_KEY_FILE_NONE, error);
	if (!ret)
		goto out;

	/* get groups */
	groups = g_key_file_get_groups (keyfile, NULL);
	if (groups == NULL) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0, "failed to get groups");
		goto out;
	}

	/* add each device if it's not already connected */
	for (i=0; groups[i] != NULL; i++) {
		device = gcm_client_get_device_by_id (client, groups[i]);
		if (device == NULL) {
			egg_debug ("not found %s", groups[i]);
			gcm_client_add_unconnected_device (client, keyfile, groups[i]);
		} else {
			egg_debug ("found already added %s", groups[i]);
			g_object_set (device,
				      "saved", TRUE,
				      NULL);
		}
	}
out:
	g_strfreev (groups);
	g_free (filename);
	g_key_file_free (keyfile);
	return ret;
}

/**
 * gcm_client_add_connected:
 **/
gboolean
gcm_client_add_connected (GcmClient *client, GError **error)
{
	gboolean ret;

	g_return_val_if_fail (GCM_IS_CLIENT (client), FALSE);

	/* udev */
	ret = gcm_client_add_connected_devices_udev (client, error);
	if (!ret)
		goto out;

	/* xrandr */
	ret = gcm_client_add_connected_devices_xrandr (client, error);
	if (!ret)
		goto out;

	/* cups */
	ret = gcm_client_add_connected_devices_cups (client, error);
	if (!ret)
		goto out;
out:
	return ret;
}

/**
 * gcm_client_delete_device:
 **/
gboolean
gcm_client_delete_device (GcmClient *client, GcmDevice *device, GError **error)
{
	gboolean ret = FALSE;
	gchar *id = NULL;
	gchar *data = NULL;
	gchar *filename = NULL;
	GKeyFile *keyfile = NULL;
	gboolean connected;

	g_return_val_if_fail (GCM_IS_CLIENT (client), FALSE);
	g_return_val_if_fail (GCM_IS_DEVICE (device), FALSE);

	/* check removable */
	g_object_get (device,
		      "connected", &connected,
		      "id", &id,
		      NULL);
	if (connected) {
		g_set_error_literal (error, 1, 0, "device is still connected");
		goto out;
	}

	/* try to remove from array */
	ret = g_ptr_array_remove (client->priv->array, device);
	if (!ret) {
		g_set_error_literal (error, 1, 0, "not found in device array");
		goto out;
	}

	/* get the config file */
	filename = gcm_utils_get_default_config_location ();
	egg_debug ("removing %s from %s", id, filename);

	/* load the config file */
	keyfile = g_key_file_new ();
	ret = g_key_file_load_from_file (keyfile, filename, G_KEY_FILE_NONE, error);
	if (!ret)
		goto out;

	/* remove from the config file */
	g_key_file_remove_group (keyfile, id, error);
	if (!ret)
		goto out;

	/* convert to string */
	data = g_key_file_to_data (keyfile, NULL, error);
	if (data == NULL) {
		ret = FALSE;
		goto out;
	}

	/* save contents */
	ret = g_file_set_contents (filename, data, -1, error);
	if (!ret)
		goto out;

	/* update status */
	g_object_set (device,
		      "saved", FALSE,
		      NULL);

	/* emit a signal */
	egg_debug ("emit removed: %s", id);
	g_signal_emit (client, signals[SIGNAL_REMOVED], 0, device);
out:
	g_free (id);
	g_free (data);
	g_free (filename);
	if (keyfile != NULL)
		g_key_file_free (keyfile);
	return ret;
}

/**
 * gcm_client_get_property:
 **/
static void
gcm_client_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GcmClient *client = GCM_CLIENT (object);
	GcmClientPrivate *priv = client->priv;

	switch (prop_id) {
	case PROP_DISPLAY_NAME:
		g_value_set_string (value, priv->display_name);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gcm_client_set_property:
 **/
static void
gcm_client_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	GcmClient *client = GCM_CLIENT (object);
	GcmClientPrivate *priv = client->priv;

	switch (prop_id) {
	case PROP_DISPLAY_NAME:
		g_free (priv->display_name);
		priv->display_name = g_strdup (g_value_get_string (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gcm_client_randr_event_cb:
 **/
static void
gcm_client_randr_event_cb (GcmScreen *screen, GcmClient *client)
{
	GnomeRROutput **outputs;
	guint i;

	egg_debug ("screens may have changed");

	/* replug devices */
	outputs = gcm_screen_get_outputs (screen, NULL);
	for (i=0; outputs[i] != NULL; i++)
		gcm_client_xrandr_add (client, outputs[i]);
}

/**
 * gcm_client_class_init:
 **/
static void
gcm_client_class_init (GcmClientClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gcm_client_finalize;
	object_class->get_property = gcm_client_get_property;
	object_class->set_property = gcm_client_set_property;

	/**
	 * GcmClient:display-name:
	 */
	pspec = g_param_spec_string ("display-name", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_DISPLAY_NAME, pspec);

	/**
	 * GcmClient::added
	 **/
	signals[SIGNAL_ADDED] =
		g_signal_new ("added",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GcmClientClass, added),
			      NULL, NULL, g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, G_TYPE_OBJECT);

	/**
	 * GcmClient::removed
	 **/
	signals[SIGNAL_REMOVED] =
		g_signal_new ("removed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GcmClientClass, removed),
			      NULL, NULL, g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, G_TYPE_OBJECT);

	g_type_class_add_private (klass, sizeof (GcmClientPrivate));
}

/**
 * gcm_client_init:
 **/
static void
gcm_client_init (GcmClient *client)
{
	const gchar *subsystems[] = {"usb", "video4linux", NULL};

	client->priv = GCM_CLIENT_GET_PRIVATE (client);
	client->priv->display_name = NULL;
	client->priv->array = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	client->priv->screen = gcm_screen_new ();
	g_signal_connect (client->priv->screen, "outputs-changed",
			  G_CALLBACK (gcm_client_randr_event_cb), client);

	/* use GUdev to find devices */
	client->priv->gudev_client = g_udev_client_new (subsystems);
	g_signal_connect (client->priv->gudev_client, "uevent",
			  G_CALLBACK (gcm_client_uevent_cb), client);

	/* for CUPS */
	httpInitialize();

	/* should be okay for localhost */
	client->priv->http = httpConnectEncrypt (cupsServer (), ippPort (), cupsEncryption ());
}

/**
 * gcm_client_finalize:
 **/
static void
gcm_client_finalize (GObject *object)
{
	GcmClient *client = GCM_CLIENT (object);
	GcmClientPrivate *priv = client->priv;

	g_free (priv->display_name);
	g_ptr_array_unref (priv->array);
	g_object_unref (priv->gudev_client);
	g_object_unref (priv->screen);
	httpClose (priv->http);

	G_OBJECT_CLASS (gcm_client_parent_class)->finalize (object);
}

/**
 * gcm_client_new:
 *
 * Return value: a new GcmClient object.
 **/
GcmClient *
gcm_client_new (void)
{
	GcmClient *client;
	client = g_object_new (GCM_TYPE_CLIENT, NULL);
	return GCM_CLIENT (client);
}


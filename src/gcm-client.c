/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009-2010 Richard Hughes <richard@hughsie.com>
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
 * for changes. If a device is added, removed or changed then a signal is fired.
 */

#include "config.h"

#include <glib/gi18n.h>
#include <glib-object.h>
#include <gudev/gudev.h>
#include <libgnomeui/gnome-rr.h>
#include <cups/cups.h>

#ifdef GCM_USE_SANE
 #include <sane/sane.h>
#endif

#include "gcm-client.h"
#include "gcm-device-xrandr.h"
#include "gcm-device-udev.h"
#include "gcm-device-cups.h"
#ifdef GCM_USE_SANE
 #include "gcm-device-sane.h"
#endif
#include "gcm-device-virtual.h"
#include "gcm-screen.h"
#include "gcm-utils.h"

#include "egg-debug.h"

static void     gcm_client_finalize	(GObject     *object);

#define GCM_CLIENT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GCM_TYPE_CLIENT, GcmClientPrivate))

static void gcm_client_xrandr_add (GcmClient *client, GnomeRROutput *output);

#ifdef GCM_USE_SANE
static gboolean gcm_client_add_connected_devices_sane (GcmClient *client, GError **error);
static gpointer gcm_client_add_connected_devices_sane_thrd (GcmClient *client);
#endif

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
	GSettings			*settings;
	GcmScreen			*screen;
	http_t				*http;
	gboolean			 loading;
	guint				 loading_refcount;
	gboolean			 use_threads;
	gboolean			 init_cups;
	gboolean			 init_sane;
};

enum {
	PROP_0,
	PROP_DISPLAY_NAME,
	PROP_LOADING,
	PROP_USE_THREADS,
	PROP_LAST
};

enum {
	SIGNAL_ADDED,
	SIGNAL_REMOVED,
	SIGNAL_CHANGED,
	SIGNAL_LAST
};

static guint signals[SIGNAL_LAST] = { 0 };
static gpointer gcm_client_object = NULL;

G_DEFINE_TYPE (GcmClient, gcm_client, G_TYPE_OBJECT)

#define	GCM_CLIENT_SANE_REMOVED_TIMEOUT		200	/* ms */

/**
 * gcm_client_set_use_threads:
 **/
void
gcm_client_set_use_threads (GcmClient *client, gboolean use_threads)
{
	client->priv->use_threads = use_threads;
	g_object_notify (G_OBJECT (client), "use-threads");
}

/**
 * gcm_client_set_loading:
 **/
static void
gcm_client_set_loading (GcmClient *client, gboolean ret)
{
	client->priv->loading = ret;
	g_object_notify (G_OBJECT (client), "loading");
	egg_debug ("loading: %i", ret);
}

/**
 * gcm_client_done_loading:
 **/
static void
gcm_client_done_loading (GcmClient *client)
{
	static GStaticMutex mutex = G_STATIC_MUTEX_INIT;

	/* decrement refcount, with a lock */
	g_static_mutex_lock (&mutex);
	client->priv->loading_refcount--;
	if (client->priv->loading_refcount == 0)
		gcm_client_set_loading (client, FALSE);
	g_static_mutex_unlock (&mutex);
}

/**
 * gcm_client_add_loading:
 **/
static void
gcm_client_add_loading (GcmClient *client)
{
	static GStaticMutex mutex = G_STATIC_MUTEX_INIT;

	/* decrement refcount, with a lock */
	g_static_mutex_lock (&mutex);
	client->priv->loading_refcount++;
	if (client->priv->loading_refcount > 0)
		gcm_client_set_loading (client, TRUE);
	g_static_mutex_unlock (&mutex);
}

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
 * gcm_client_get_loading:
 *
 * @client: a valid %GcmClient instance
 *
 * Gets the loading status.
 *
 * Return value: %TRUE if the object is still loading devices
 **/
gboolean
gcm_client_get_loading (GcmClient *client)
{
	g_return_val_if_fail (GCM_IS_CLIENT (client), FALSE);
	return client->priv->loading;
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
 * gcm_client_device_changed_cb:
 **/
static void
gcm_client_device_changed_cb (GcmDevice *device, GcmClient *client)
{
	/* emit a signal */
	egg_debug ("emit changed: %s", gcm_device_get_id (device));
	g_signal_emit (client, signals[SIGNAL_CHANGED], 0, device);
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
static gboolean
gcm_client_gudev_remove (GcmClient *client, GUdevDevice *udev_device)
{
	GcmDevice *device;
	gchar *id;
	gboolean ret = FALSE;

	/* generate id */
	id = gcm_client_get_id_for_udev_device (udev_device);

	/* do we have a device that matches */
	device = gcm_client_get_device_by_id (client, id);
	if (device == NULL) {
		egg_debug ("no match for %s", id);
		goto out;
	}

	/* set disconnected */
	gcm_device_set_connected (device, FALSE);

	/* success */
	ret = TRUE;
out:
	if (device != NULL)
		g_object_unref (device);
	g_free (id);
	return ret;
}

/**
 * gcm_client_gudev_add:
 **/
static gboolean
gcm_client_gudev_add (GcmClient *client, GUdevDevice *udev_device)
{
	GcmDevice *device = NULL;
	GcmDevice *device_tmp = NULL;
	gboolean ret = FALSE;
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
		/* already disconnected device now connected */
		g_object_unref (device);
		device = g_object_ref (device_tmp);
		gcm_device_set_connected (device, TRUE);
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

	/* connect to the changed signal */
	g_signal_connect (device, "changed", G_CALLBACK (gcm_client_device_changed_cb), client);
out:
	if (device != NULL)
		g_object_unref (device);
	if (device_tmp != NULL)
		g_object_unref (device_tmp);
	return ret;
}

#ifdef GCM_USE_SANE
/**
 * gcm_client_sane_refresh_cb:
 **/
static gboolean
gcm_client_sane_refresh_cb (GcmClient *client)
{
	gboolean ret;
	GError *error = NULL;
	GThread *thread;

	/* inform UI if we are loading devices still */
	client->priv->loading_refcount = 1;
	gcm_client_set_loading (client, TRUE);

	/* rescan */
	egg_debug ("rescanning sane");
	if (client->priv->use_threads) {
		thread = g_thread_create ((GThreadFunc) gcm_client_add_connected_devices_sane_thrd, client, FALSE, &error);
		if (thread == NULL) {
			egg_debug ("failed to rescan sane devices: %s", error->message);
			g_error_free (error);
			goto out;
		}
	} else {
		ret = gcm_client_add_connected_devices_sane (client, &error);
		if (!ret) {
			egg_debug ("failed to rescan sane devices: %s", error->message);
			g_error_free (error);
			goto out;
		}
	}
out:
	return FALSE;
}
#endif

/**
 * gcm_client_uevent_cb:
 **/
static void
gcm_client_uevent_cb (GUdevClient *gudev_client, const gchar *action, GUdevDevice *udev_device, GcmClient *client)
{
	gboolean ret;
#ifdef GCM_USE_SANE
	const gchar *value;
	GcmDevice *device_tmp;
	guint i;
	GcmClientPrivate *priv = client->priv;
#endif

	if (g_strcmp0 (action, "remove") == 0) {
		ret = gcm_client_gudev_remove (client, udev_device);
		if (ret)
			egg_debug ("removed %s", g_udev_device_get_sysfs_path (udev_device));

#ifdef GCM_USE_SANE
		/* we need to remove scanner devices */
		value = g_udev_device_get_property (udev_device, "GCM_RESCAN");
		if (g_strcmp0 (value, "scanner") == 0) {

			/* set all scanners as disconnected */
			for (i=0; i<priv->array->len; i++) {
				device_tmp = g_ptr_array_index (priv->array, i);
				if (gcm_device_get_kind (device_tmp) == GCM_DEVICE_KIND_SCANNER)
					gcm_device_set_connected (device_tmp, FALSE);
			}

			/* find any others that might still be connected */
			g_timeout_add (GCM_CLIENT_SANE_REMOVED_TIMEOUT, (GSourceFunc) gcm_client_sane_refresh_cb, client);
		}
#endif

	} else if (g_strcmp0 (action, "add") == 0) {
		ret = gcm_client_gudev_add (client, udev_device);
		if (ret)
			egg_debug ("added %s", g_udev_device_get_sysfs_path (udev_device));

#ifdef GCM_USE_SANE
		/* we need to rescan scanner devices */
		value = g_udev_device_get_property (udev_device, "GCM_RESCAN");
		if (g_strcmp0 (value, "scanner") == 0)
			g_timeout_add (GCM_CLIENT_SANE_REMOVED_TIMEOUT, (GSourceFunc) gcm_client_sane_refresh_cb, client);
#endif
	}
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

	/* inform the UI */
	gcm_client_done_loading (client);

	return TRUE;
}

/**
 * gcm_client_add_connected_devices_udev_thrd:
 **/
static gpointer
gcm_client_add_connected_devices_udev_thrd (GcmClient *client)
{
	gcm_client_add_connected_devices_udev (client, NULL);
	return NULL;
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
		g_object_unref (device_tmp);
	}
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

	/* we might have a previous saved device with this ID, in which use that instead */
	device_tmp = gcm_client_get_device_by_id (client, gcm_device_get_id (device));
	if (device_tmp != NULL) {

		/* this is just a dupe */
		connected = gcm_device_get_connected (device_tmp);
		if (connected) {
			egg_debug ("ignoring dupe");
			goto out;
		}

		/* use original device */
		g_object_unref (device);
		device = g_object_ref (device_tmp);
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

	/* connect to the changed signal */
	g_signal_connect (device, "changed", G_CALLBACK (gcm_client_device_changed_cb), client);
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

	/* inform the UI */
	gcm_client_done_loading (client);

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
	device = gcm_device_cups_new ();
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

	/* connect to the changed signal */
	g_signal_connect (device, "changed", G_CALLBACK (gcm_client_device_changed_cb), client);
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

	/* initialize */
	if (!client->priv->init_cups) {
		httpInitialize();
		/* should be okay for localhost */
		client->priv->http = httpConnectEncrypt (cupsServer (), ippPort (), cupsEncryption ());
		client->priv->init_cups = TRUE;
	}

	num_dests = cupsGetDests2 (priv->http, &dests);
	egg_debug ("got %i printers", num_dests);

	/* get printers on the local server */
	for (i = 0; i < num_dests; i++)
		gcm_client_cups_add (client, dests[i]);
	cupsFreeDests (num_dests, dests);

	/* inform the UI */
	gcm_client_done_loading (client);
	return TRUE;
}

/**
 * gcm_client_add_connected_devices_cups_thrd:
 **/
static gpointer
gcm_client_add_connected_devices_cups_thrd (GcmClient *client)
{
	gcm_client_add_connected_devices_cups (client, NULL);
	return NULL;
}

#ifdef GCM_USE_SANE
/**
 * gcm_client_sane_add:
 **/
static void
gcm_client_sane_add (GcmClient *client, const SANE_Device *sane_device)
{
	gboolean ret;
	GError *error = NULL;
	GcmDevice *device = NULL;
	GcmDevice *device_tmp = NULL;
	GcmClientPrivate *priv = client->priv;

	/* create new device */
	device = gcm_device_sane_new ();
	ret = gcm_device_sane_set_from_device (device, sane_device, &error);
	if (!ret) {
		egg_debug ("failed to set for output: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* we might have a previous saved device with this ID, in which use that instead */
	device_tmp = gcm_client_get_device_by_id (client, gcm_device_get_id (device));
	if (device_tmp != NULL) {
		gcm_device_set_connected (device_tmp, TRUE);
		g_object_unref (device);
		device = g_object_ref (device_tmp);
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

	/* connect to the changed signal */
	g_signal_connect (device, "changed", G_CALLBACK (gcm_client_device_changed_cb), client);
out:
	if (device != NULL)
		g_object_unref (device);
	if (device_tmp != NULL)
		g_object_unref (device_tmp);
}

/**
 * gcm_client_add_connected_devices_sane:
 **/
static gboolean
gcm_client_add_connected_devices_sane (GcmClient *client, GError **error)
{
	gint i;
	gboolean ret = TRUE;
	SANE_Status status;
	const SANE_Device **device_list;

	/* force sane to drop it's cache of devices -- yes, it is that crap */
	if (client->priv->init_sane) {
		sane_exit ();
		client->priv->init_sane = FALSE;
	}
	status = sane_init (NULL, NULL);
	if (status != SANE_STATUS_GOOD) {
		ret = FALSE;
		g_set_error (error, 1, 0, "failed to init SANE: %s", sane_strstatus (status));
		goto out;
	}
	client->priv->init_sane = TRUE;

	/* get scanners on the local server */
	status = sane_get_devices (&device_list, FALSE);
	if (status != SANE_STATUS_GOOD) {
		ret = FALSE;
		g_set_error (error, 1, 0, "failed to get devices from SANE: %s", sane_strstatus (status));
		goto out;
	}

	/* nothing */
	if (device_list == NULL || device_list[0] == NULL) {
		egg_debug ("no devices to add");
		goto out;
	}

	/* add them */
	for (i=0; device_list[i] != NULL; i++)
		gcm_client_sane_add (client, device_list[i]);
out:
	/* inform the UI */
	gcm_client_done_loading (client);
	return ret;
}

/**
 * gcm_client_add_connected_devices_sane_thrd:
 **/
static gpointer
gcm_client_add_connected_devices_sane_thrd (GcmClient *client)
{
	gcm_client_add_connected_devices_sane (client, NULL);
	return NULL;
}
#endif

/**
 * gcm_client_add_unconnected_device:
 **/
static void
gcm_client_add_unconnected_device (GcmClient *client, GKeyFile *keyfile, const gchar *id)
{
	gchar *title;
	gchar *kind_text = NULL;
	gchar *colorspace_text = NULL;
	GcmColorspace colorspace;
	GcmDeviceKind kind;
	GcmDevice *device = NULL;
	gboolean ret;
	gboolean virtual;
	GError *error = NULL;
	GcmClientPrivate *priv = client->priv;

	/* add new device */
	title = g_key_file_get_string (keyfile, id, "title", NULL);
	if (title == NULL)
		goto out;
	virtual = g_key_file_get_boolean (keyfile, id, "virtual", NULL);
	kind_text = g_key_file_get_string (keyfile, id, "type", NULL);
	kind = gcm_device_kind_from_string (kind_text);
	if (kind == GCM_DEVICE_KIND_UNKNOWN)
		goto out;

	/* get colorspace */
	colorspace_text = g_key_file_get_string (keyfile, id, "colorspace", NULL);
	if (colorspace_text == NULL) {
		egg_warning ("legacy device %s, falling back to RGB", id);
		colorspace = GCM_COLORSPACE_RGB;
	} else {
		colorspace = gcm_colorspace_from_string (colorspace_text);
	}

	/* create device of specified type */
	if (virtual) {
		device = gcm_device_virtual_new ();
	} else if (kind == GCM_DEVICE_KIND_DISPLAY) {
		device = gcm_device_xrandr_new ();
	} else if (kind == GCM_DEVICE_KIND_PRINTER) {
		device = gcm_device_cups_new ();
	} else if (kind == GCM_DEVICE_KIND_CAMERA) {
		/* FIXME: use GPhoto? */
		device = gcm_device_udev_new ();
#ifdef GCM_USE_SANE
	} else if (kind == GCM_DEVICE_KIND_SCANNER) {
		device = gcm_device_sane_new ();
#endif
	} else {
		egg_warning ("device kind internal error");
		goto out;
	}

	/* create device */
	g_object_set (device,
		      "kind", kind,
		      "id", id,
		      "connected", FALSE,
		      "title", title,
		      "saved", TRUE,
		      "virtual", virtual,
		      "colorspace", colorspace,
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

	/* connect to the changed signal */
	g_signal_connect (device, "changed", G_CALLBACK (gcm_client_device_changed_cb), client);
out:
	if (device != NULL)
		g_object_unref (device);
	g_free (kind_text);
	g_free (colorspace_text);
	g_free (title);
}

/**
 * gcm_client_possibly_migrate_config_file:
 *
 * Copy the configuration file from ~/.config/gnome-color-manager to ~/.config/color
 **/
static gboolean
gcm_client_possibly_migrate_config_file (GcmClient *client)
{
	gchar *dest = NULL;
	gchar *source = NULL;
	GFile *gdest = NULL;
	GFile *gsource = NULL;
	gboolean ret = FALSE;
	gboolean done_migration;
	GError *error = NULL;

	/* have we already attempted this (check first to avoid stating a file */
	done_migration = g_settings_get_boolean (client->priv->settings, GCM_SETTINGS_DONE_MIGRATION);
	if (done_migration)
		goto out;

	/* create default path */
	source = g_build_filename (g_get_user_config_dir (), "gnome-color-manager", "device-profiles.conf", NULL);
	gsource = g_file_new_for_path (source);

	/* no old profile */
	ret = g_file_query_exists (gsource, NULL);
	if (!ret) {
		g_settings_set_boolean (client->priv->settings, GCM_SETTINGS_DONE_MIGRATION, TRUE);
		goto out;
	}

	/* ensure new root exists */
	dest = gcm_utils_get_default_config_location ();
	ret = gcm_utils_mkdir_for_filename (dest, &error);
	if (!ret) {
		egg_warning ("failed to create new tree: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* copy to new root */
	gdest = g_file_new_for_path (dest);
	ret = g_file_copy (gsource, gdest, G_FILE_COPY_NONE, NULL, NULL, NULL, &error);
	if (!ret) {
		egg_warning ("failed to copy: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* do not attempt to migrate this again */
	g_settings_set_boolean (client->priv->settings, GCM_SETTINGS_DONE_MIGRATION, TRUE);
out:
	g_free (source);
	g_free (dest);
	if (gsource != NULL)
		g_object_unref (gsource);
	if (gdest != NULL)
		g_object_unref (gdest);
	return ret;
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

	/* copy from old location */
	gcm_client_possibly_migrate_config_file (client);

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
			gcm_device_set_saved (device, TRUE);
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
gcm_client_add_connected (GcmClient *client, GcmClientColdplug coldplug, GError **error)
{
	gboolean ret;
	GThread *thread;

	g_return_val_if_fail (GCM_IS_CLIENT (client), FALSE);

	/* copy from old location */
	gcm_client_possibly_migrate_config_file (client);

	/* reset */
	client->priv->loading_refcount = 0;

	/* XRandR */
	if (!coldplug || coldplug & GCM_CLIENT_COLDPLUG_XRANDR) {
		gcm_client_add_loading (client);
		egg_debug ("adding devices of type XRandR");
		ret = gcm_client_add_connected_devices_xrandr (client, error);
		if (!ret)
			goto out;
	}

	/* UDEV */
	if (!coldplug || coldplug & GCM_CLIENT_COLDPLUG_UDEV) {
		gcm_client_add_loading (client);
		egg_debug ("adding devices of type UDEV");
		if (client->priv->use_threads) {
			thread = g_thread_create ((GThreadFunc) gcm_client_add_connected_devices_udev_thrd, client, FALSE, error);
			if (thread == NULL)
				goto out;
		} else {
			ret = gcm_client_add_connected_devices_udev (client, error);
			if (!ret)
				goto out;
		}
	}

	/* CUPS */
	if (!coldplug || coldplug & GCM_CLIENT_COLDPLUG_CUPS) {
		gcm_client_add_loading (client);
		egg_debug ("adding devices of type CUPS");
		if (client->priv->use_threads) {
			thread = g_thread_create ((GThreadFunc) gcm_client_add_connected_devices_cups_thrd, client, FALSE, error);
			if (thread == NULL)
				goto out;
		} else {
			ret = gcm_client_add_connected_devices_cups (client, error);
			if (!ret)
				goto out;
		}
	}

#ifdef GCM_USE_SANE
	/* SANE */
	if (!coldplug || coldplug & GCM_CLIENT_COLDPLUG_SANE) {
		gcm_client_add_loading (client);
		egg_debug ("adding devices of type SANE");
		if (client->priv->use_threads) {
			thread = g_thread_create ((GThreadFunc) gcm_client_add_connected_devices_sane_thrd, client, FALSE, error);
			if (thread == NULL)
				goto out;
		} else {
			ret = gcm_client_add_connected_devices_sane (client, error);
			if (!ret)
				goto out;
		}
	}
#endif
out:
	return ret;
}

/**
 * gcm_client_add_virtual_device:
 **/
gboolean
gcm_client_add_virtual_device (GcmClient *client, GcmDevice *device, GError **error)
{
	gboolean ret = FALSE;
	const gchar *id;
	gboolean virtual;
	GcmDevice *device_tmp = NULL;

	g_return_val_if_fail (GCM_IS_CLIENT (client), FALSE);
	g_return_val_if_fail (GCM_IS_DEVICE (device), FALSE);

	/* check removable */
	virtual = gcm_device_get_virtual (device);
	if (!virtual) {
		g_set_error_literal (error, 1, 0, "cannot add non-virtual devices");
		goto out;
	}

	/* look to see if device already exists */
	id = gcm_device_get_id (device);
	device_tmp = gcm_client_get_device_by_id (client, id);
	if (device_tmp != NULL) {
		/* TRANSLATORS: error message */
		g_set_error_literal (error, 1, 0, _("This device already exists"));
		goto out;
	}

	/* add to the array */
	g_ptr_array_add (client->priv->array, g_object_ref (device));

	/* update status */
	gcm_device_set_saved (device, FALSE);

	/* emit a signal */
	egg_debug ("emit added: %s", id);
	g_signal_emit (client, signals[SIGNAL_ADDED], 0, device);

	/* connect to the changed signal */
	g_signal_connect (device, "changed", G_CALLBACK (gcm_client_device_changed_cb), client);

	/* all okay */
	ret = TRUE;
out:
	if (device_tmp != NULL)
		g_object_unref (device_tmp);
	return ret;
}

/**
 * gcm_client_delete_device:
 **/
gboolean
gcm_client_delete_device (GcmClient *client, GcmDevice *device, GError **error)
{
	gboolean ret = FALSE;
	const gchar *id;
	gchar *data = NULL;
	gchar *filename = NULL;
	GKeyFile *keyfile = NULL;
	gboolean saved;
	gboolean connected;

	g_return_val_if_fail (GCM_IS_CLIENT (client), FALSE);
	g_return_val_if_fail (GCM_IS_DEVICE (device), FALSE);

	/* check device is not connected */
	connected = gcm_device_get_connected (device);
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

	/* check device is saved */
	id = gcm_device_get_id (device);
	saved = gcm_device_get_saved (device);
	if (!saved)
		goto not_saved;

	/* get the config file */
	filename = gcm_utils_get_default_config_location ();
	egg_debug ("removing %s from %s", id, filename);

	/* load the config file */
	keyfile = g_key_file_new ();
	ret = g_key_file_load_from_file (keyfile, filename, G_KEY_FILE_NONE, error);
	if (!ret)
		goto out;

	/* remove from the config file */
	ret = g_key_file_remove_group (keyfile, id, error);
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
	gcm_device_set_saved (device, FALSE);

not_saved:
	/* emit a signal */
	egg_debug ("emit removed: %s", id);
	g_signal_emit (client, signals[SIGNAL_REMOVED], 0, device);
out:
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
	case PROP_LOADING:
		g_value_set_boolean (value, priv->loading);
		break;
	case PROP_USE_THREADS:
		g_value_set_boolean (value, priv->use_threads);
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
	case PROP_USE_THREADS:
		priv->use_threads = g_value_get_boolean (value);
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
	 * GcmClient:loading:
	 */
	pspec = g_param_spec_boolean ("loading", NULL, NULL,
				      TRUE,
				      G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_LOADING, pspec);

	/**
	 * GcmClient:use-threads:
	 */
	pspec = g_param_spec_boolean ("use-threads", NULL, NULL,
				      TRUE,
				      G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_USE_THREADS, pspec);

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

	/**
	 * GcmClient::changed
	 **/
	signals[SIGNAL_CHANGED] =
		g_signal_new ("changed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GcmClientClass, changed),
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
	client->priv->loading_refcount = 0;
	client->priv->use_threads = FALSE;
	client->priv->init_cups = FALSE;
	client->priv->init_sane = FALSE;
	client->priv->settings = g_settings_new (GCM_SETTINGS_SCHEMA);
	client->priv->array = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	client->priv->screen = gcm_screen_new ();
	g_signal_connect (client->priv->screen, "outputs-changed",
			  G_CALLBACK (gcm_client_randr_event_cb), client);

	/* use GUdev to find devices */
	client->priv->gudev_client = g_udev_client_new (subsystems);
	g_signal_connect (client->priv->gudev_client, "uevent",
			  G_CALLBACK (gcm_client_uevent_cb), client);
}

/**
 * gcm_client_finalize:
 **/
static void
gcm_client_finalize (GObject *object)
{
	GcmClient *client = GCM_CLIENT (object);
	GcmClientPrivate *priv = client->priv;
	GcmDevice *device;
	guint i;

	/* do not respond to changed events */
	for (i=0; i<priv->array->len; i++) {
		device = g_ptr_array_index (priv->array, i);
		g_signal_handlers_disconnect_by_func (device, G_CALLBACK (gcm_client_device_changed_cb), client);
	}

	g_free (priv->display_name);
	g_ptr_array_unref (priv->array);
	g_object_unref (priv->gudev_client);
	g_object_unref (priv->screen);
	g_object_unref (priv->settings);
	if (client->priv->init_cups)
		httpClose (priv->http);
#ifdef GCM_USE_SANE
	if (client->priv->init_sane)
		sane_exit ();
#endif

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
	if (gcm_client_object != NULL) {
		g_object_ref (gcm_client_object);
	} else {
		gcm_client_object = g_object_new (GCM_TYPE_CLIENT, NULL);
		g_object_add_weak_pointer (gcm_client_object, &gcm_client_object);
	}
	return GCM_CLIENT (gcm_client_object);
}


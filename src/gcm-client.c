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

#include "gcm-client.h"
#include "gcm-x11-screen.h"
#include "gcm-utils.h"

static void     gcm_client_finalize	(GObject     *object);

#define GCM_CLIENT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GCM_TYPE_CLIENT, GcmClientPrivate))

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
	gboolean			 loading;
	guint				 loading_refcount;
	guint				 refresh_id;
	guint				 emit_added_id;
};

enum {
	PROP_0,
	PROP_DISPLAY_NAME,
	PROP_LOADING,
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
	g_debug ("emit changed: %s", gcm_device_get_id (device));
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
 * gcm_client_remove_device_internal:
 **/
static gboolean
gcm_client_remove_device_internal (GcmClient *client, GcmDevice *device, gboolean emit_signal, GError **error)
{
	gboolean ret = FALSE;
	const gchar *device_id;
	static GStaticMutex mutex = G_STATIC_MUTEX_INIT;

	g_return_val_if_fail (GCM_IS_CLIENT (client), FALSE);
	g_return_val_if_fail (GCM_IS_DEVICE (device), FALSE);

	/* lock */
	g_static_mutex_lock (&mutex);

	/* check device is not connected */
	device_id = gcm_device_get_id (device);
	if (gcm_device_get_connected (device)) {
		g_set_error (error, 1, 0, "device %s is still connected", device_id);
		goto out;
	}

	/* try to remove from array */
	ret = g_ptr_array_remove (client->priv->array, device);
	if (!ret) {
		g_set_error_literal (error, 1, 0, "not found in device array");
		goto out;
	}

	/* ensure signal handlers are disconnected */
	g_signal_handlers_disconnect_by_func (device, G_CALLBACK (gcm_client_device_changed_cb), client);

	/* emit a signal */
	if (emit_signal) {
		g_debug ("emit removed: %s", device_id);
		g_signal_emit (client, signals[SIGNAL_REMOVED], 0, device);
	}
out:
	/* unlock */
	g_static_mutex_unlock (&mutex);

	return ret;
}

/**
 * gcm_client_gudev_remove:
 **/
static gboolean
gcm_client_gudev_remove (GcmClient *client, GUdevDevice *udev_device)
{
	GcmDevice *device;
	GError *error = NULL;
	gchar *id;
	gboolean ret = FALSE;

	/* generate id */
	id = gcm_client_get_id_for_udev_device (udev_device);

	/* do we have a device that matches */
	device = gcm_client_get_device_by_id (client, id);
	if (device == NULL) {
		g_debug ("no match for %s", id);
		goto out;
	}

	/* set disconnected */
	gcm_device_set_connected (device, FALSE);

	/* remove device as it never had a profile assigned */
	if (!gcm_device_get_saved (device)) {
		ret = gcm_client_remove_device_internal (client, device, TRUE, &error);
		if (!ret) {
			g_warning ("failed to remove: %s", error->message);
			g_error_free (error);
			goto out;
		}
	}

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
	gboolean ret = FALSE;
	const gchar *value;
	GError *error = NULL;

	/* matches our udev rules, which we can change without recompiling */
	value = g_udev_device_get_property (udev_device, "GCM_DEVICE");
	if (value == NULL)
		goto out;

	/* add device */
	ret = gcm_client_add_device (client, device, &error);
	if (!ret) {
		g_debug ("failed to set for device: %s", error->message);
		g_error_free (error);
		goto out;
	}
out:
	if (device != NULL)
		g_object_unref (device);
	return ret;
}

/**
 * gcm_client_uevent_cb:
 **/
static void
gcm_client_uevent_cb (GUdevClient *gudev_client, const gchar *action, GUdevDevice *udev_device, GcmClient *client)
{
	gboolean ret;
#ifdef HAVE_SANE
	const gchar *value;
	GcmDevice *device_tmp;
	guint i;
	gboolean enable;
	GcmClientPrivate *priv = client->priv;
#endif

	if (g_strcmp0 (action, "remove") == 0) {
		ret = gcm_client_gudev_remove (client, udev_device);
		if (ret)
			g_debug ("removed %s", g_udev_device_get_sysfs_path (udev_device));

#ifdef HAVE_SANE
		/* we need to remove scanner devices */
		value = g_udev_device_get_property (udev_device, "GCM_RESCAN");
		if (g_strcmp0 (value, "scanner") == 0) {

			/* are we ignoring scanners */
			enable = g_settings_get_boolean (client->priv->settings, GCM_SETTINGS_ENABLE_SANE);
			if (!enable)
				return;

			/* set all scanners as disconnected */
			for (i=0; i<priv->array->len; i++) {
				device_tmp = g_ptr_array_index (priv->array, i);
				if (gcm_device_get_kind (device_tmp) == CD_DEVICE_KIND_SCANNER)
					gcm_device_set_connected (device_tmp, FALSE);
			}

			/* find any others that might still be connected */
			if (priv->refresh_id != 0)
				g_source_remove (priv->refresh_id);
			priv->refresh_id = g_timeout_add (GCM_CLIENT_SANE_REMOVED_TIMEOUT,
							  (GSourceFunc) gcm_client_sane_refresh_cb,
							  client);
#if GLIB_CHECK_VERSION(2,25,8)
			g_source_set_name_by_id (priv->refresh_id, "[GcmClient] refresh due to device remove");
#endif
		}
#endif

	} else if (g_strcmp0 (action, "add") == 0) {
		ret = gcm_client_gudev_add (client, udev_device);
		if (ret)
			g_debug ("added %s", g_udev_device_get_sysfs_path (udev_device));

#ifdef HAVE_SANE
		/* we need to rescan scanner devices */
		value = g_udev_device_get_property (udev_device, "GCM_RESCAN");
		if (g_strcmp0 (value, "scanner") == 0) {

			/* are we ignoring scanners */
			enable = g_settings_get_boolean (client->priv->settings, GCM_SETTINGS_ENABLE_SANE);
			if (!enable)
				return;

			if (priv->refresh_id != 0)
				g_source_remove (priv->refresh_id);
			priv->refresh_id = g_timeout_add (GCM_CLIENT_SANE_REMOVED_TIMEOUT,
							  (GSourceFunc) gcm_client_sane_refresh_cb,
							  client);
#if GLIB_CHECK_VERSION(2,25,8)
			g_source_set_name_by_id (priv->refresh_id, "[GcmClient] refresh due to device add");
#endif
		}
#endif
	}
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
	g_debug ("overlap_x=%i,overlap_y=%i,covered=%f", overlap_x, overlap_y, covered);
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
	guint x, y;
	guint i;
	guint width, height;
	GcmX11Output *output;
	GcmX11Output *output_best = NULL;
	GPtrArray *outputs = NULL;
	GcmDevice *device = NULL;

	/* get the window parameters, in root co-ordinates */
	gdk_window_get_origin (window, &window_x, &window_y);
	window_width = gdk_window_get_width (window);
	window_height = gdk_window_get_height (window);

	/* get list of updates */
	//outputs = gcm_x11_screen_get_outputs (client->priv->screen, NULL);
	if (outputs == NULL)
		goto out;

	/* go through each option */
	for (i=0; i<outputs->len; i++) {

		/* not interesting */
		output = g_ptr_array_index (outputs, i);
		if (!gcm_x11_output_get_connected (output))
			continue;

		/* get details about the output */
		gcm_x11_output_get_position (output, &x, &y);
		gcm_x11_output_get_size (output, &width, &height);
		g_debug ("%s: %ix%i -> %ix%i (%ix%i -> %ix%i)", gcm_x11_output_get_name (output),
			   x, y, x+width, y+height,
			   window_x, window_y, window_x+window_width, window_y+window_height);

		/* get the fraction of how much the window is covered */
		covered = gcm_client_get_device_by_window_covered (x, y, width, height,
								   window_x, window_y, window_width, window_height);

		/* keep a running total of which one is best */
		if (covered > 0.01f && covered > covered_max) {
			output_best = output;

			/* optimize */
			if (covered > 0.99) {
				g_debug ("all in one window, no need to search other windows");
				goto out;
			}

			/* keep looking */
			covered_max = covered;
			g_debug ("personal best of %f for %s", covered, gcm_x11_output_get_name (output_best));
		}
	}
out:
	if (outputs != NULL)
		g_ptr_array_unref (outputs);
	/* if we found an output, get the device */
	if (output_best != NULL) {
		GcmDevice *device_tmp;
//		device_tmp = gcm_device_xrandr_new ();
//		gcm_device_xrandr_set_from_output (device_tmp, output_best, NULL);
//		device = gcm_client_get_device_by_id (client, gcm_device_get_id (device_tmp));
		g_object_unref (device_tmp);
	}
	return device;
}

#ifdef HAVE_SANE
/**
 * gcm_client_sane_add:
 **/
static void
gcm_client_sane_add (GcmClient *client, const SANE_Device *sane_device)
{
	gboolean ret;
	GError *error = NULL;
	GcmDevice *device = NULL;

	/* create new device */
	device = gcm_device_sane_new ();
	ret = gcm_device_sane_set_from_device (device, sane_device, &error);
	if (!ret) {
		g_debug ("failed to set for output: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* add device */
	ret = gcm_client_add_device (client, device, &error);
	if (!ret) {
		g_debug ("failed to set for device: %s", error->message);
		g_error_free (error);
		goto out;
	}
out:
	if (device != NULL)
		g_object_unref (device);
}

#endif

typedef struct {
	GcmClient *client;
	GcmDevice *device;
} GcmClientEmitHelper;

/**
 * gcm_client_emit_added_idle_cb:
 **/
static gboolean
gcm_client_emit_added_idle_cb (GcmClientEmitHelper *helper)
{
	/* emit a signal */
	g_debug ("emit added: %s", gcm_device_get_id (helper->device));
	g_signal_emit (helper->client, signals[SIGNAL_ADDED], 0, helper->device);
	helper->client->priv->emit_added_id = 0;

	g_object_unref (helper->client);
	g_object_unref (helper->device);
	g_free (helper);
	return FALSE;
}

/**
 * gcm_client_emit_added_idle:
 **/
static void
gcm_client_emit_added_idle (GcmClient *client, GcmDevice *device)
{
	GcmClientEmitHelper *helper;
	helper = g_new0 (GcmClientEmitHelper, 1);
	helper->client = g_object_ref (client);
	helper->device = g_object_ref (device);
	client->priv->emit_added_id = g_idle_add ((GSourceFunc) gcm_client_emit_added_idle_cb, helper);
#if GLIB_CHECK_VERSION(2,25,8)
	g_source_set_name_by_id (client->priv->emit_added_id, "[GcmClient] emit added for device");
#endif
}

/**
 * gcm_client_add_device:
 **/
gboolean
gcm_client_add_device (GcmClient *client, GcmDevice *device, GError **error)
{
	gboolean ret = FALSE;
	const gchar *device_id;
	GcmDevice *device_tmp = NULL;
	GPtrArray *array;
	static GStaticMutex mutex = G_STATIC_MUTEX_INIT;

	g_return_val_if_fail (GCM_IS_CLIENT (client), FALSE);
	g_return_val_if_fail (GCM_IS_DEVICE (device), FALSE);

	/* lock */
	g_static_mutex_lock (&mutex);

	/* look to see if device already exists */
	device_id = gcm_device_get_id (device);
	device_tmp = gcm_client_get_device_by_id (client, device_id);
	if (device_tmp != NULL) {
		g_debug ("already exists, copy settings and remove old device: %s", device_id);
		array = gcm_device_get_profiles (device_tmp);
		if (array != NULL) {
			gcm_device_set_profiles (device, array);
			g_ptr_array_unref (array);
		}
		gcm_device_set_saved (device, gcm_device_get_saved (device_tmp));
		ret = gcm_client_remove_device_internal (client, device_tmp, FALSE, error);
		if (!ret)
			goto out;
	}

	/* load the device */
	ret = gcm_device_load (device, error);
	if (!ret)
		goto out;

	/* add to the array */
	g_ptr_array_add (client->priv->array, g_object_ref (device));
	g_signal_connect (device, "changed", G_CALLBACK (gcm_client_device_changed_cb), client);

	/* emit a signal */
	gcm_client_emit_added_idle (client, device);

	/* all okay */
	ret = TRUE;
out:
	/* unlock */
	g_static_mutex_unlock (&mutex);

	if (device_tmp != NULL)
		g_object_unref (device_tmp);
	return ret;
}

/**
 * gcm_client_remove_device:
 **/
gboolean
gcm_client_remove_device (GcmClient *client, GcmDevice *device, GError **error)
{
	return gcm_client_remove_device_internal (client, device, TRUE, error);
}

/**
 * gcm_client_delete_device:
 **/
gboolean
gcm_client_delete_device (GcmClient *client, GcmDevice *device, GError **error)
{
	gboolean ret = FALSE;
	const gchar *device_id;
	gchar *data = NULL;
	gchar *filename = NULL;
	GKeyFile *keyfile = NULL;

	g_return_val_if_fail (GCM_IS_CLIENT (client), FALSE);
	g_return_val_if_fail (GCM_IS_DEVICE (device), FALSE);

	/* check device is saved */
	device_id = gcm_device_get_id (device);
	if (!gcm_device_get_saved (device))
		goto out;

	/* get the config file */
	filename = gcm_utils_get_default_config_location ();
	g_debug ("removing %s from %s", device_id, filename);

	/* load the config file */
	keyfile = g_key_file_new ();
	ret = g_key_file_load_from_file (keyfile, filename, G_KEY_FILE_NONE, error);
	if (!ret)
		goto out;

	/* remove from the config file */
	ret = g_key_file_remove_group (keyfile, device_id, error);
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

	/* remove device */
	ret = gcm_client_remove_device_internal (client, device, TRUE, error);
	if (!ret)
		goto out;
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
	client->priv->settings = g_settings_new (GCM_SETTINGS_SCHEMA);
	client->priv->array = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);

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

	/* disconnect anything that's about to fire */
	if (priv->refresh_id != 0)
		g_source_remove (priv->refresh_id);
	if (priv->emit_added_id != 0)
		g_source_remove (priv->emit_added_id);

	/* do not respond to changed events */
	for (i=0; i<priv->array->len; i++) {
		device = g_ptr_array_index (priv->array, i);
		g_signal_handlers_disconnect_by_func (device, G_CALLBACK (gcm_client_device_changed_cb), client);
	}

	g_free (priv->display_name);
	g_ptr_array_unref (priv->array);
	g_object_unref (priv->gudev_client);
	g_object_unref (priv->settings);

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


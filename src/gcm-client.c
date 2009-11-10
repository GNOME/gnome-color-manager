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
 * GNU General Public License for more client.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:gcm-client
 * @short_description: Client object to hold an array of devices.
 *
 * This object holds an array of %GcmDevices, and watches both udev and xorg
 * for changes. If a device is added or removed then a signal is fired.
 */

#include "config.h"

#include <glib-object.h>
#include <gudev/gudev.h>
#include <libgnomeui/gnome-rr.h>

#include "gcm-client.h"
#include "gcm-utils.h"
#include "gcm-edid.h"

#include "egg-debug.h"

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
	GnomeRRScreen			*rr_screen;
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
	GcmClientPrivate *priv = client->priv;

	/* generate id */
	id = gcm_client_get_id_for_udev_device (udev_device);
	device = gcm_client_get_device_by_id (client, id);
	if (device != NULL) {
		/* emit before we remove so the device is valid */
		egg_debug ("emit removed: %s", id);
		g_signal_emit (client, signals[SIGNAL_REMOVED], 0, device);
		g_ptr_array_remove (priv->array, device);
		g_object_unref (device);
	}
	g_free (id);
}

/**
 * gcm_client_gudev_add_type:
 **/
static void
gcm_client_gudev_add_type (GcmClient *client, GUdevDevice *udev_device, GcmDeviceType type)
{
	gchar *title;
	GcmDevice *device = NULL;
	gchar *id;
	gboolean ret;
	GError *error = NULL;
	const gchar *sysfs_path;
	GcmClientPrivate *priv = client->priv;

	/* add new device */
	id = gcm_client_get_id_for_udev_device (udev_device);
	title = g_strdup_printf ("%s - %s",
				g_udev_device_get_property (udev_device, "ID_VENDOR"),
				g_udev_device_get_property (udev_device, "ID_MODEL"));

	/* turn space delimiters into spaces */
	g_strdelimit (title, "_", ' ');

	/* get sysfs path */
	sysfs_path = g_udev_device_get_sysfs_path (udev_device);

	/* create device */
	device = gcm_device_new ();
	g_object_set (device,
		      "type", type,
		      "id", id,
		      "title", title,
		      "native-device-sysfs", sysfs_path,
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
	g_free (id);
	g_free (title);
}

/**
 * gcm_client_gudev_add:
 **/
static void
gcm_client_gudev_add (GcmClient *client, GUdevDevice *udev_device)
{
	const gchar *value;

	/* only matches HP printers, need to expand to all of CUPS printers */
	value = g_udev_device_get_property (udev_device, "ID_HPLIP");
	if (value != NULL) {
		egg_debug ("found printer device: %s", g_udev_device_get_sysfs_path (udev_device));
		gcm_client_gudev_add_type (client, udev_device, GCM_DEVICE_TYPE_PRINTER);
	}

	/* sane is slightly odd in a lowercase property, and "yes" as a value rather than "1" */
	value = g_udev_device_get_property (udev_device, "libsane_matched");
	if (value != NULL) {
		egg_debug ("found scanner device: %s", g_udev_device_get_sysfs_path (udev_device));
		gcm_client_gudev_add_type (client, udev_device, GCM_DEVICE_TYPE_SCANNER);
	}

	/* only matches cameras with gphoto drivers */
	value = g_udev_device_get_property (udev_device, "ID_GPHOTO2");
	if (value != NULL) {
		egg_debug ("found camera device: %s", g_udev_device_get_sysfs_path (udev_device));
		gcm_client_gudev_add_type (client, udev_device, GCM_DEVICE_TYPE_CAMERA);
	}
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
 * gcm_client_coldplug_devices_usb:
 **/
static gboolean
gcm_client_coldplug_devices_usb (GcmClient *client, GError **error)
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

	g_list_foreach (devices, (GFunc) g_object_unref, NULL);
	g_list_free (devices);
	return TRUE;
}

/**
 * gcm_utils_get_output_name:
 *
 * Return value: the output name, free with g_free().
 **/
static gchar *
gcm_utils_get_output_name (GnomeRROutput *output)
{
	const guint8 *data;
	const gchar *output_name;
	gchar *name = NULL;
	GcmEdid *edid = NULL;
	gboolean ret;

	/* if nothing connected then fall back to the connector name */
	ret = gnome_rr_output_is_connected (output);
	if (!ret)
		goto out;

	/* parse the EDID to get a crtc-specific name, not an output specific name */
	data = gnome_rr_output_get_edid_data (output);
	if (data == NULL)
		goto out;

	edid = gcm_edid_new ();
	ret = gcm_edid_parse (edid, data, NULL);
	if (!ret) {
		egg_warning ("failed to parse edid");
		goto out;
	}

	/* find the best option */
	g_object_get (edid, "monitor-name", &name, NULL);
	if (name == NULL)
		g_object_get (edid, "ascii-string", &name, NULL);
	if (name == NULL)
		g_object_get (edid, "serial-number", &name, NULL);
	if (name == NULL)
		g_object_get (edid, "vendor-name", &name, NULL);

out:
	/* fallback to the output name */
	if (name == NULL) {
		output_name = gnome_rr_output_get_name (output);
		ret = gcm_utils_output_is_lcd_internal (output_name);
		if (ret)
			output_name = "Internal LCD";
		name = g_strdup (output_name);
	}

	if (edid != NULL)
		g_object_unref (edid);
	return name;
}

/**
 * gcm_client_get_id_for_xrandr_device:
 **/
static gchar *
gcm_client_get_id_for_xrandr_device (GnomeRROutput *output)
{
	gchar *id;
	gchar *name;

	/* get id */
	name = gcm_utils_get_output_name (output);
	id = g_strdup_printf ("xrandr_%s", name);

	/* replace unsafe chars */
	gcm_utils_alphanum_lcase (id);

	g_free (name);
	return id;
}

/**
 * gcm_client_xrandr_add:
 **/
static void
gcm_client_xrandr_add (GcmClient *client, GnomeRROutput *output)
{
	gchar *title = NULL;
	gchar *id;
	gboolean ret;
	GcmDevice *device = NULL;
	GError *error = NULL;
	const gchar *output_name;
	GcmClientPrivate *priv = client->priv;

	/* get details */
	id = gcm_client_get_id_for_xrandr_device (output);

	/* if nothing connected then ignore */
	ret = gnome_rr_output_is_connected (output);
	if (!ret) {
		egg_debug ("%s is not connected", id);
		goto out;
	}

	/* are we already in the list */
	device = gcm_client_get_device_by_id (client, id);
	if (device != NULL) {
		egg_debug ("%s already added", id);
		g_object_unref (device);
		goto out;
	}

	/* add new device */
	device = gcm_device_new ();
	title = gcm_utils_get_output_name (output);
	output_name = gnome_rr_output_get_name (output);
	g_object_set (device,
		      "type", GCM_DEVICE_TYPE_DISPLAY,
		      "id", id,
		      "title", title,
		      "native-device-xrandr", output_name,
		      NULL);

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
	egg_debug ("emit: added %s to device list", id);
	g_signal_emit (client, signals[SIGNAL_ADDED], 0, device);
out:
	if (device != NULL)
		g_object_unref (device);
	g_free (id);
	g_free (title);
}

/**
 * gcm_client_randr_event_cb:
 **/
static void
gcm_client_randr_event_cb (GnomeRRScreen *screen, GcmClient *client)
{
	GnomeRROutput **outputs;
	guint i;

	egg_debug ("screens may have changed");

	/* replug devices */
	outputs = gnome_rr_screen_list_outputs (client->priv->rr_screen);
	for (i=0; outputs[i] != NULL; i++)
		gcm_client_xrandr_add (client, outputs[i]);
}

/**
 * gcm_client_coldplug_devices_xrandr:
 **/
static gboolean
gcm_client_coldplug_devices_xrandr (GcmClient *client, GError **error)
{
	GnomeRROutput **outputs;
	guint i;
	GcmClientPrivate *priv = client->priv;

	outputs = gnome_rr_screen_list_outputs (priv->rr_screen);
	for (i=0; outputs[i] != NULL; i++)
		gcm_client_xrandr_add (client, outputs[i]);
	return TRUE;
}

/**
 * gcm_client_coldplug:
 **/
gboolean
gcm_client_coldplug (GcmClient *client, GError **error)
{
	gboolean ret;

	g_return_val_if_fail (GCM_IS_CLIENT (client), FALSE);

	/* usb */
	ret = gcm_client_coldplug_devices_usb (client, error);
	if (!ret)
		goto out;

	/* xorg */
	ret = gcm_client_coldplug_devices_xrandr (client, error);
	if (!ret)
		goto out;
out:
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
	GError *error = NULL;
	const gchar *subsystems[] = {"usb", NULL};

	client->priv = GCM_CLIENT_GET_PRIVATE (client);
	client->priv->display_name = NULL;
	client->priv->array = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);

	/* use GUdev to find devices */
	client->priv->gudev_client = g_udev_client_new (subsystems);
	g_signal_connect (client->priv->gudev_client, "uevent",
			  G_CALLBACK (gcm_client_uevent_cb), client);

	/* get screen */
	client->priv->rr_screen = gnome_rr_screen_new (gdk_screen_get_default (), (GnomeRRScreenChanged) gcm_client_randr_event_cb, client, &error);
	if (client->priv->rr_screen == NULL) {
		egg_error ("failed to get rr screen: %s", error->message);
		g_error_free (error);
	}
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
	gnome_rr_screen_destroy (priv->rr_screen);

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


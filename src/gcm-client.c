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

#include <glib/gi18n.h>
#include <glib-object.h>
#include <gudev/gudev.h>
#include <libgnomeui/gnome-rr.h>
#include <math.h>

#include "gcm-client.h"
#include "gcm-utils.h"
#include "gcm-edid.h"
#include "gcm-dmi.h"

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
	GcmEdid				*edid;
	GcmDmi				*dmi;
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
	gchar *serial = NULL;
	gchar *manufacturer = NULL;
	gchar *model = NULL;

	/* add new device */
	id = gcm_client_get_id_for_udev_device (udev_device);
	title = g_strdup_printf ("%s - %s",
				g_udev_device_get_property (udev_device, "ID_VENDOR"),
				g_udev_device_get_property (udev_device, "ID_MODEL"));

	/* turn space delimiters into spaces */
	g_strdelimit (title, "_", ' ');

	/* we might have a previous saved device with this ID, in which case nuke it */
	device = gcm_client_get_device_by_id (client, id);
	if (device != NULL) {
		g_ptr_array_remove (client->priv->array, device);
		g_object_unref (device);
	}

	/* get sysfs path */
	sysfs_path = g_udev_device_get_sysfs_path (udev_device);

	/* create device */
	device = gcm_device_new ();
	manufacturer = g_strdup (g_udev_device_get_property (udev_device, "ID_VENDOR"));
	model = g_strdup (g_udev_device_get_property (udev_device, "ID_MODEL"));
	serial = g_strdup (g_udev_device_get_property (udev_device, "ID_SERIAL"));

	/* get rid of underscores */
	if (manufacturer != NULL) {
		g_strdelimit (manufacturer, "_", ' ');
		g_strchomp (manufacturer);
	}
	if (model != NULL) {
		g_strdelimit (model, "_", ' ');
		g_strchomp (model);
	}
	if (serial != NULL) {
		g_strdelimit (serial, "_", ' ');
		g_strchomp (serial);
	}

	g_object_set (device,
		      "type", type,
		      "id", id,
		      "connected", TRUE,
		      "serial", serial,
		      "model", model,
		      "manufacturer", manufacturer,
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
	g_free (serial);
	g_free (manufacturer);
	g_free (model);
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
 * gcm_client_add_connected_devices_usb:
 **/
static gboolean
gcm_client_add_connected_devices_usb (GcmClient *client, GError **error)
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
 * gcm_client_get_output_name:
 *
 * Return value: the output name, free with g_free().
 **/
static gchar *
gcm_client_get_output_name (GcmClient *client, GnomeRROutput *output)
{
	const gchar *output_name;
	gchar *name = NULL;
	gchar *vendor = NULL;
	GString *string;
	gboolean ret;
	guint width = 0;
	guint height = 0;
	GcmClientPrivate *priv = client->priv;

	/* blank */
	string = g_string_new ("");

	/* if nothing connected then fall back to the connector name */
	ret = gnome_rr_output_is_connected (output);
	if (!ret)
		goto out;

	/* this is an internal panel, use the DMI data */
	output_name = gnome_rr_output_get_name (output);
	ret = gcm_utils_output_is_lcd_internal (output_name);
	if (ret) {
		/* find the machine details */
		g_object_get (priv->dmi,
			      "product-name", &name,
			      "product-version", &vendor,
			      NULL);

		/* TRANSLATORS: this is the name of the internal panel */
		output_name = _("Laptop LCD");
	} else {
		/* find the panel details */
		g_object_get (priv->edid,
			      "monitor-name", &name,
			      "vendor-name", &vendor,
			      NULL);
	}

	/* prepend vendor if available */
	if (vendor != NULL && name != NULL)
		g_string_append_printf (string, "%s - %s", vendor, name);
	else if (name != NULL)
		g_string_append (string, name);
	else
		g_string_append (string, output_name);

out:
	/* find the best option */
	g_object_get (priv->edid,
		      "width", &width,
		      "height", &height,
		      NULL);

	/* append size if available */
	if (width != 0 && height != 0) {
		guint diag;

		/* calculate the dialgonal using Pythagorean theorem */
		diag = sqrtf ((powf (width,2)) + (powf (height, 2)));

		/* print it in inches */
		g_string_append_printf (string, " - %i\"", (guint) ((gfloat) diag * 0.393700787f));
	}

	g_free (name);
	g_free (vendor);
	return g_string_free (string, FALSE);
}

/**
 * gcm_client_get_output_id:
 *
 * Return value: the output name, free with g_free().
 **/
static gchar *
gcm_client_get_output_id (GcmClient *client, GnomeRROutput *output)
{
	const gchar *output_name;
	gchar *name = NULL;
	gchar *vendor = NULL;
	GString *string;
	gboolean ret;
	guint width = 0;
	guint height = 0;
	GcmClientPrivate *priv = client->priv;

	/* blank */
	string = g_string_new ("");

	/* if nothing connected then fall back to the connector name */
	ret = gnome_rr_output_is_connected (output);
	if (!ret)
		goto out;

	/* find the best option */
	g_object_get (priv->edid,
		      "width", &width,
		      "height", &height,
		      "monitor-name", &name,
		      "vendor-name", &vendor,
		      NULL);

	/* find the best option */
	if (name == NULL)
		g_object_get (priv->edid, "ascii-string", &name, NULL);
	if (name == NULL)
		g_object_get (priv->edid, "serial-number", &name, NULL);

	/* prepend vendor if available */
	if (vendor != NULL && name != NULL)
		g_string_append_printf (string, "%s - %s", vendor, name);
	else if (name != NULL)
		g_string_append_printf (string, "%s", name);

out:
	/* fallback to the output name */
	if (string->len == 0) {
		output_name = gnome_rr_output_get_name (output);
		ret = gcm_utils_output_is_lcd_internal (output_name);
		if (ret)
			output_name = "LVDS";
		g_string_append (string, output_name);
	}

	/* append size if available */
	if (width != 0 && height != 0) {
		guint diag;

		/* calculate the dialgonal using Pythagorean theorem */
		diag = sqrtf ((powf (width,2)) + (powf (height, 2)));

		/* print it in inches */
		g_string_append_printf (string, " %i", (guint) ((gfloat) diag * 0.393700787f));
	}

	g_free (name);
	g_free (vendor);
	return g_string_free (string, FALSE);
}

/**
 * gcm_client_get_id_for_xrandr_device:
 **/
static gchar *
gcm_client_get_id_for_xrandr_device (GcmClient *client, GnomeRROutput *output)
{
	gchar *id;
	gchar *name;

	/* get id */
	name = gcm_client_get_output_id (client, output);
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
	gchar *id = NULL;
	gboolean ret;
	GcmDevice *device = NULL;
	GError *error = NULL;
	const gchar *output_name;
	gchar *serial = NULL;
	gchar *manufacturer = NULL;
	gchar *model = NULL;
	const guint8 *data;
	GcmClientPrivate *priv = client->priv;

	/* if nothing connected then ignore */
	ret = gnome_rr_output_is_connected (output);
	if (!ret) {
		egg_debug ("%s is not connected", gnome_rr_output_get_name (output));
		goto out;
	}

	/* get details */
	id = gcm_client_get_id_for_xrandr_device (client, output);

	/* we might have a previous saved device with this ID, in which case nuke it */
	device = gcm_client_get_device_by_id (client, id);
	if (device != NULL) {
		g_ptr_array_remove (client->priv->array, device);
		g_object_unref (device);
	}

	/* parse the EDID to get a crtc-specific name, not an output specific name */
	data = gnome_rr_output_get_edid_data (output);
	if (data != NULL) {
		ret = gcm_edid_parse (priv->edid, data, NULL);
		if (!ret) {
			egg_warning ("failed to parse edid");
			goto out;
		}

		/* get data about the display */
		g_object_get (priv->edid,
			      "monitor-name", &model,
			      "vendor-name", &manufacturer,
			      "serial-number", &serial,
			      NULL);
	}

	/* refine data if it's missing */
	output_name = gnome_rr_output_get_name (output);
	ret = gcm_utils_output_is_lcd_internal (output_name);
	if (ret && model == NULL) {
		g_object_get (priv->dmi,
			      "product-version", &model,
			      NULL);
	}

	/* add new device */
	device = gcm_device_new ();
	title = gcm_client_get_output_name (client, output);
	g_object_set (device,
		      "type", GCM_DEVICE_TYPE_DISPLAY,
		      "id", id,
		      "connected", TRUE,
		      "serial", serial,
		      "model", model,
		      "manufacturer", manufacturer,
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
	g_free (serial);
	g_free (manufacturer);
	g_free (model);
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
 * gcm_client_add_connected_devices_xrandr:
 **/
static gboolean
gcm_client_add_connected_devices_xrandr (GcmClient *client, GError **error)
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
 * gcm_client_add_unconnected_device:
 **/
static void
gcm_client_add_unconnected_device (GcmClient *client, GKeyFile *keyfile, const gchar *id)
{
	gchar *title;
	gchar *type_text = NULL;
	GcmDeviceType type;
	GcmDevice *device = NULL;
	gboolean ret;
	GError *error = NULL;
	GcmClientPrivate *priv = client->priv;

	/* add new device */
	title = g_key_file_get_string (keyfile, id, "title", NULL);
	if (title == NULL)
		goto out;
	type_text = g_key_file_get_string (keyfile, id, "type", NULL);
	type = gcm_device_type_from_text (type_text);

	/* create device */
	device = gcm_device_new ();
	g_object_set (device,
		      "type", type,
		      "id", id,
		      "connected", FALSE,
		      "title", title,
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
		if (error != NULL)
			*error = g_error_new (1, 0, "failed to get groups");
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

	/* usb */
	ret = gcm_client_add_connected_devices_usb (client, error);
	if (!ret)
		goto out;

	/* xorg */
	ret = gcm_client_add_connected_devices_xrandr (client, error);
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
		if (error != NULL)
			*error = g_error_new (1, 0, "device is still connected");
		goto out;
	}

	/* try to remove from array */
	ret = g_ptr_array_remove (client->priv->array, device);
	if (!ret) {
		if (error != NULL)
			*error = g_error_new (1, 0, "not found in device array");
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
	client->priv->edid = gcm_edid_new ();
	client->priv->dmi = gcm_dmi_new ();

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
	g_object_unref (priv->edid);
	g_object_unref (priv->dmi);
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


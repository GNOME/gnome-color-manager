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
 * SECTION:gcm-device
 * @short_description: Color managed device object
 *
 * This object represents a device that can be colour managed.
 */

#include "config.h"

#include <glib-object.h>

#include "gcm-device.h"
#include "gcm-profile.h"
#include "gcm-utils.h"

#include "egg-debug.h"

static void     gcm_device_finalize	(GObject     *object);

#define GCM_DEVICE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GCM_TYPE_DEVICE, GcmDevicePrivate))

/**
 * GcmDevicePrivate:
 *
 * Private #GcmDevice data
 **/
struct _GcmDevicePrivate
{
	gboolean		 connected;
	gboolean		 virtual;
	gboolean		 saved;
	gfloat			 gamma;
	gfloat			 brightness;
	gfloat			 contrast;
	GcmDeviceKind		 kind;
	gchar			*id;
	gchar			*serial;
	gchar			*manufacturer;
	gchar			*model;
	gchar			*profile_filename;
	gchar			*title;
	GSettings		*settings;
	GcmColorspace		 colorspace;
	guint			 changed_id;
	glong			 modified_time;
};

enum {
	PROP_0,
	PROP_KIND,
	PROP_ID,
	PROP_CONNECTED,
	PROP_VIRTUAL,
	PROP_SAVED,
	PROP_SERIAL,
	PROP_MODEL,
	PROP_MANUFACTURER,
	PROP_GAMMA,
	PROP_BRIGHTNESS,
	PROP_CONTRAST,
	PROP_PROFILE_FILENAME,
	PROP_TITLE,
	PROP_COLORSPACE,
	PROP_LAST
};

enum {
	SIGNAL_CHANGED,
	SIGNAL_LAST
};

static guint signals[SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE (GcmDevice, gcm_device, G_TYPE_OBJECT)

#define GCM_DEVICE_CHANGED_SUPRESS_TIMEOUT	10	/* ms */

/**
 * gcm_device_changed_cb:
 **/
static gboolean
gcm_device_changed_cb (GcmDevice *device)
{
	/* emit a signal */
	egg_debug ("emit changed: %s", gcm_device_get_id (device));
	g_signal_emit (device, signals[SIGNAL_CHANGED], 0);
	device->priv->changed_id = 0;
	return FALSE;
}

/**
 * gcm_device_changed:
 **/
static void
gcm_device_changed (GcmDevice *device)
{
	static GStaticMutex mutex = G_STATIC_MUTEX_INIT;

	/* lock */
	g_static_mutex_lock (&mutex);

	/* already queued, so ignoring */
	if (device->priv->changed_id != 0)
		goto out;

	/* adding to queue */
	device->priv->changed_id = g_timeout_add_full (G_PRIORITY_DEFAULT_IDLE,
						       GCM_DEVICE_CHANGED_SUPRESS_TIMEOUT,
						       (GSourceFunc) gcm_device_changed_cb,
						       g_object_ref (device),
						       (GDestroyNotify) g_object_unref);
out:
	/* unlock */
	g_static_mutex_unlock (&mutex);
}

/**
 * gcm_device_kind_from_string:
 **/
GcmDeviceKind
gcm_device_kind_from_string (const gchar *kind)
{
	if (g_strcmp0 (kind, "display") == 0)
		return GCM_DEVICE_KIND_DISPLAY;
	if (g_strcmp0 (kind, "scanner") == 0)
		return GCM_DEVICE_KIND_SCANNER;
	if (g_strcmp0 (kind, "printer") == 0)
		return GCM_DEVICE_KIND_PRINTER;
	if (g_strcmp0 (kind, "camera") == 0)
		return GCM_DEVICE_KIND_CAMERA;
	return GCM_DEVICE_KIND_UNKNOWN;
}

/**
 * gcm_device_kind_to_string:
 **/
const gchar *
gcm_device_kind_to_string (GcmDeviceKind kind)
{
	if (kind == GCM_DEVICE_KIND_DISPLAY)
		return "display";
	if (kind == GCM_DEVICE_KIND_SCANNER)
		return "scanner";
	if (kind == GCM_DEVICE_KIND_PRINTER)
		return "printer";
	if (kind == GCM_DEVICE_KIND_CAMERA)
		return "camera";
	return "unknown";
}

/**
 * gcm_device_load_from_profile:
 **/
static gboolean
gcm_device_load_from_profile (GcmDevice *device, GError **error)
{
	gboolean ret = TRUE;
	GcmDevicePrivate *priv = device->priv;

	g_return_val_if_fail (GCM_IS_DEVICE (device), FALSE);

	/* no profile to load */
	if (priv->profile_filename == NULL)
		goto out;

	/* load the profile if it's set */
	if (priv->profile_filename != NULL) {

		/* if the profile was deleted */
		ret = g_file_test (priv->profile_filename, G_FILE_TEST_EXISTS);
		if (!ret) {
			egg_warning ("the file was deleted and can't be loaded: %s", priv->profile_filename);
			/* this is not fatal */
			ret = TRUE;
			goto out;
		}
	}
out:
	return ret;
}

/**
 * gcm_device_get_kind:
 **/
GcmDeviceKind
gcm_device_get_kind (GcmDevice *device)
{
	g_return_val_if_fail (GCM_IS_DEVICE (device), GCM_DEVICE_KIND_UNKNOWN);
	return device->priv->kind;
}

/**
 * gcm_device_set_kind:
 **/
void
gcm_device_set_kind (GcmDevice *device, GcmDeviceKind kind)
{
	g_return_if_fail (GCM_IS_DEVICE (device));
	if (device->priv->kind != kind) {
		device->priv->kind = kind;
		gcm_device_changed (device);
	}
}

/**
 * gcm_device_get_connected:
 **/
gboolean
gcm_device_get_connected (GcmDevice *device)
{
	g_return_val_if_fail (GCM_IS_DEVICE (device), FALSE);
	return device->priv->connected;
}

/**
 * gcm_device_set_connected:
 **/
void
gcm_device_set_connected (GcmDevice *device, gboolean connected)
{
	g_return_if_fail (GCM_IS_DEVICE (device));
	if (device->priv->connected != connected) {
		device->priv->connected = connected;
		gcm_device_changed (device);
	}
}

/**
 * gcm_device_get_virtual:
 **/
gboolean
gcm_device_get_virtual (GcmDevice *device)
{
	g_return_val_if_fail (GCM_IS_DEVICE (device), FALSE);
	return device->priv->virtual;
}

/**
 * gcm_device_set_virtual:
 **/
void
gcm_device_set_virtual (GcmDevice *device, gboolean virtual)
{
	g_return_if_fail (GCM_IS_DEVICE (device));
	device->priv->virtual = virtual;
	gcm_device_changed (device);
}

/**
 * gcm_device_get_saved:
 **/
gboolean
gcm_device_get_saved (GcmDevice *device)
{
	g_return_val_if_fail (GCM_IS_DEVICE (device), FALSE);
	return device->priv->saved;
}

/**
 * gcm_device_set_saved:
 **/
void
gcm_device_set_saved (GcmDevice *device, gboolean saved)
{
	g_return_if_fail (GCM_IS_DEVICE (device));
	device->priv->saved = saved;
	gcm_device_changed (device);
}

/**
 * gcm_device_get_gamma:
 **/
gfloat
gcm_device_get_gamma (GcmDevice *device)
{
	g_return_val_if_fail (GCM_IS_DEVICE (device), 0.0f);
	return device->priv->gamma;
}

/**
 * gcm_device_set_gamma:
 **/
void
gcm_device_set_gamma (GcmDevice *device, gfloat gamma)
{
	g_return_if_fail (GCM_IS_DEVICE (device));
	device->priv->gamma = gamma;
	gcm_device_changed (device);
}

/**
 * gcm_device_get_brightness:
 **/
gfloat
gcm_device_get_brightness (GcmDevice *device)
{
	g_return_val_if_fail (GCM_IS_DEVICE (device), 0.0f);
	return device->priv->brightness;
}

/**
 * gcm_device_set_brightness:
 **/
void
gcm_device_set_brightness (GcmDevice *device, gfloat brightness)
{
	g_return_if_fail (GCM_IS_DEVICE (device));
	device->priv->brightness = brightness;
	gcm_device_changed (device);
}

/**
 * gcm_device_get_contrast:
 **/
gfloat
gcm_device_get_contrast (GcmDevice *device)
{
	g_return_val_if_fail (GCM_IS_DEVICE (device), 0.0f);
	return device->priv->contrast;
}

/**
 * gcm_device_set_contrast:
 **/
void
gcm_device_set_contrast (GcmDevice *device, gfloat contrast)
{
	g_return_if_fail (GCM_IS_DEVICE (device));
	device->priv->contrast = contrast;
	gcm_device_changed (device);
}

/**
 * gcm_device_get_colorspace:
 **/
GcmColorspace
gcm_device_get_colorspace (GcmDevice *device)
{
	g_return_val_if_fail (GCM_IS_DEVICE (device), GCM_COLORSPACE_UNKNOWN);
	return device->priv->colorspace;
}

/**
 * gcm_device_set_colorspace:
 **/
void
gcm_device_set_colorspace (GcmDevice *device, GcmColorspace colorspace)
{
	g_return_if_fail (GCM_IS_DEVICE (device));
	device->priv->colorspace = colorspace;
	gcm_device_changed (device);
}

/**
 * gcm_device_get_id:
 **/
const gchar *
gcm_device_get_id (GcmDevice *device)
{
	g_return_val_if_fail (GCM_IS_DEVICE (device), NULL);
	return device->priv->id;
}

/**
 * gcm_device_set_id:
 **/
void
gcm_device_set_id (GcmDevice *device, const gchar *id)
{
	g_return_if_fail (GCM_IS_DEVICE (device));
	g_free (device->priv->id);
	device->priv->id = g_strdup (id);
	gcm_device_changed (device);
}

/**
 * gcm_device_get_serial:
 **/
const gchar *
gcm_device_get_serial (GcmDevice *device)
{
	g_return_val_if_fail (GCM_IS_DEVICE (device), NULL);
	return device->priv->serial;
}

/**
 * gcm_device_set_serial:
 **/
void
gcm_device_set_serial (GcmDevice *device, const gchar *serial)
{
	g_return_if_fail (GCM_IS_DEVICE (device));
	g_free (device->priv->serial);
	device->priv->serial = g_strdup (serial);
	gcm_device_changed (device);
}

/**
 * gcm_device_get_manufacturer:
 **/
const gchar *
gcm_device_get_manufacturer (GcmDevice *device)
{
	g_return_val_if_fail (GCM_IS_DEVICE (device), NULL);
	return device->priv->manufacturer;
}

/**
 * gcm_device_set_manufacturer:
 **/
void
gcm_device_set_manufacturer (GcmDevice *device, const gchar *manufacturer)
{
	g_return_if_fail (GCM_IS_DEVICE (device));
	g_free (device->priv->manufacturer);
	device->priv->manufacturer = g_strdup (manufacturer);
	gcm_device_changed (device);
}

/**
 * gcm_device_get_model:
 **/
const gchar *
gcm_device_get_model (GcmDevice *device)
{
	g_return_val_if_fail (GCM_IS_DEVICE (device), NULL);
	return device->priv->model;
}

/**
 * gcm_device_set_model:
 **/
void
gcm_device_set_model (GcmDevice *device, const gchar *model)
{
	g_return_if_fail (GCM_IS_DEVICE (device));
	g_free (device->priv->model);
	device->priv->model = g_strdup (model);
	gcm_device_changed (device);
}

/**
 * gcm_device_get_title:
 **/
const gchar *
gcm_device_get_title (GcmDevice *device)
{
	g_return_val_if_fail (GCM_IS_DEVICE (device), NULL);
	return device->priv->title;
}

/**
 * gcm_device_set_title:
 **/
void
gcm_device_set_title (GcmDevice *device, const gchar *title)
{
	g_return_if_fail (GCM_IS_DEVICE (device));
	g_free (device->priv->title);
	device->priv->title = g_strdup (title);
	gcm_device_changed (device);
}

/**
 * gcm_device_get_profile_filename:
 **/
const gchar *
gcm_device_get_profile_filename (GcmDevice *device)
{
	g_return_val_if_fail (GCM_IS_DEVICE (device), NULL);
	return device->priv->profile_filename;
}

/**
 * gcm_device_set_profile_filename:
 **/
void
gcm_device_set_profile_filename (GcmDevice *device, const gchar *profile_filename)
{
	g_return_if_fail (GCM_IS_DEVICE (device));
	g_free (device->priv->profile_filename);
	device->priv->profile_filename = g_strdup (profile_filename);
	gcm_device_changed (device);
}

/**
 * gcm_device_get_modified_time:
 **/
glong
gcm_device_get_modified_time (GcmDevice *device)
{
	g_return_val_if_fail (GCM_IS_DEVICE (device), 0);
	return device->priv->modified_time;
}

/**
 * gcm_device_load:
 **/
gboolean
gcm_device_load (GcmDevice *device, GError **error)
{
	gboolean ret;
	GKeyFile *file = NULL;
	GError *error_local = NULL;
	gchar *filename = NULL;
	GTimeVal timeval;
	gchar *iso_date = NULL;
	GcmDevicePrivate *priv = device->priv;

	g_return_val_if_fail (GCM_IS_DEVICE (device), FALSE);
	g_return_val_if_fail (priv->id != NULL, FALSE);

	/* get default config */
	filename = gcm_utils_get_default_config_location ();

	/* check we have a config, or is this first start */
	ret = g_file_test (filename, G_FILE_TEST_EXISTS);
	if (!ret) {
		/* we have no profile to load from */
		ret = TRUE;
		goto out;
	}

	/* load existing file */
	file = g_key_file_new ();
	ret = g_key_file_load_from_file (file, filename, G_KEY_FILE_NONE, &error_local);
	if (!ret) {
		/* not fatal */
		egg_warning ("failed to load from file: %s", error_local->message);
		g_error_free (error_local);
		ret = TRUE;
		goto out;
	}

	/* has key */
	ret = g_key_file_has_group (file, priv->id);
	if (!ret) {
		/* not fatal */
		egg_debug ("failed to find saved parameters for %s", priv->id);
		ret = TRUE;
		goto out;
	}

	/* we are backed by a keyfile */
	gcm_device_set_saved (device, TRUE);

	/* load data */
	g_free (priv->profile_filename);
	priv->profile_filename = g_key_file_get_string (file, priv->id, "profile", NULL);
	if (priv->serial == NULL)
		priv->serial = g_key_file_get_string (file, priv->id, "serial", NULL);
	if (priv->model == NULL)
		priv->model = g_key_file_get_string (file, priv->id, "model", NULL);
	if (priv->manufacturer == NULL)
		priv->manufacturer = g_key_file_get_string (file, priv->id, "manufacturer", NULL);
	priv->gamma = g_key_file_get_double (file, priv->id, "gamma", &error_local);
	if (error_local != NULL) {
		priv->gamma = g_settings_get_double (priv->settings, "default_gamma");
		if (priv->gamma < 0.1f)
			priv->gamma = 1.0f;
		g_clear_error (&error_local);
	}
	priv->brightness = g_key_file_get_double (file, priv->id, "brightness", &error_local);
	if (error_local != NULL) {
		priv->brightness = 0.0f;
		g_clear_error (&error_local);
	}
	priv->contrast = g_key_file_get_double (file, priv->id, "contrast", &error_local);
	if (error_local != NULL) {
		priv->contrast = 100.0f;
		g_clear_error (&error_local);
	}

	/* get modified time */
	iso_date = g_key_file_get_string (file, priv->id, "modified", NULL);
	if (iso_date != NULL) {
		ret = g_time_val_from_iso8601 (iso_date, &timeval);
		if (!ret) {
			egg_warning ("failed to parse: %s", iso_date);
			g_get_current_time (&timeval);
		}
	} else {
		/* just use the current time */
		g_get_current_time (&timeval);
	}
	priv->modified_time = timeval.tv_sec;

	/* load this */
	ret = gcm_device_load_from_profile (device, &error_local);
	if (!ret) {

		/* just print a warning, this is not fatal */
		egg_warning ("failed to load profile %s: %s", priv->profile_filename, error_local->message);
		g_error_free (error_local);

		/* recover as the file might have been corrupted */
		g_free (priv->profile_filename);
		priv->profile_filename = NULL;
		ret = TRUE;
	}
out:
	g_free (iso_date);
	g_free (filename);
	if (file != NULL)
		g_key_file_free (file);
	return ret;
}

/**
 * gcm_device_save:
 **/
gboolean
gcm_device_save (GcmDevice *device, GError **error)
{
	GKeyFile *keyfile = NULL;
	gboolean ret;
	gchar *data = NULL;
	gchar *dirname;
	GFile *file = NULL;
	gchar *filename = NULL;
	gchar *timespec = NULL;
	GError *error_local = NULL;
	GTimeVal timeval;
	GcmDevicePrivate *priv = device->priv;

	g_return_val_if_fail (GCM_IS_DEVICE (device), FALSE);
	g_return_val_if_fail (priv->id != NULL, FALSE);

	/* get default config */
	filename = gcm_utils_get_default_config_location ();

	/* directory exists? */
	dirname = g_path_get_dirname (filename);
	ret = g_file_test (dirname, G_FILE_TEST_IS_DIR);
	if (!ret) {
		file = g_file_new_for_path (dirname);
		ret = g_file_make_directory_with_parents (file, NULL, &error_local);
		if (!ret) {
			g_set_error (error, 1, 0, "failed to create config directory: %s", error_local->message);
			g_error_free (error_local);
			goto out;
		}
	}

	/* if not ever created, then just create a dummy file */
	ret = g_file_test (filename, G_FILE_TEST_EXISTS);
	if (!ret) {
		ret = g_file_set_contents (filename, "#created today", -1, &error_local);
		if (!ret) {
			g_set_error (error, 1, 0, "failed to create dummy header: %s", error_local->message);
			g_error_free (error_local);
			goto out;
		}
	}

	/* load existing file */
	keyfile = g_key_file_new ();
	ret = g_key_file_load_from_file (keyfile, filename, G_KEY_FILE_NONE, &error_local);
	if (!ret) {
		/* empty or corrupt */
		if (error_local->code == G_KEY_FILE_ERROR_PARSE) {
			/* ignore */
			g_clear_error (&error_local);
		} else {
			g_set_error (error, 1, 0, "failed to load existing config: %s", error_local->message);
			g_error_free (error_local);
			goto out;
		}
	}

	/* get current date and time */
	g_get_current_time (&timeval);
	timespec = g_time_val_to_iso8601 (&timeval);

	/* the device does not have a created date and time */
	ret = g_key_file_has_key (keyfile, priv->id, "created", NULL);
	if (!ret)
		g_key_file_set_string (keyfile, priv->id, "created", timespec);

	/* add modified date */
	g_key_file_set_string (keyfile, priv->id, "modified", timespec);

	/* save data */
	if (priv->profile_filename == NULL)
		g_key_file_remove_key (keyfile, priv->id, "profile", NULL);
	else
		g_key_file_set_string (keyfile, priv->id, "profile", priv->profile_filename);

	/* save device specific data */
	if (priv->serial == NULL)
		g_key_file_remove_key (keyfile, priv->id, "serial", NULL);
	else
		g_key_file_set_string (keyfile, priv->id, "serial", priv->serial);
	if (priv->model == NULL)
		g_key_file_remove_key (keyfile, priv->id, "model", NULL);
	else
		g_key_file_set_string (keyfile, priv->id, "model", priv->model);
	if (priv->manufacturer == NULL)
		g_key_file_remove_key (keyfile, priv->id, "manufacturer", NULL);
	else
		g_key_file_set_string (keyfile, priv->id, "manufacturer", priv->manufacturer);

	/* only save gamma if not the default */
	if (priv->gamma > 0.99 && priv->gamma < 1.01)
		g_key_file_remove_key (keyfile, priv->id, "gamma", NULL);
	else
		g_key_file_set_double (keyfile, priv->id, "gamma", priv->gamma);

	/* only save brightness if not the default */
	if (priv->brightness > -0.01 && priv->brightness < 0.01)
		g_key_file_remove_key (keyfile, priv->id, "brightness", NULL);
	else
		g_key_file_set_double (keyfile, priv->id, "brightness", priv->brightness);

	/* only save contrast if not the default */
	if (priv->contrast > 99.9 && priv->contrast < 100.1)
		g_key_file_remove_key (keyfile, priv->id, "contrast", NULL);
	else
		g_key_file_set_double (keyfile, priv->id, "contrast", priv->contrast);

	/* save other properties we'll need if we add this device offline */
	if (priv->title != NULL)
		g_key_file_set_string (keyfile, priv->id, "title", priv->title);
	g_key_file_set_string (keyfile, priv->id, "type", gcm_device_kind_to_string (priv->kind));

	/* add colorspace */
	g_key_file_set_string (keyfile, priv->id, "colorspace", gcm_colorspace_to_string (priv->colorspace));

	/* add virtual */
	if (priv->virtual)
		g_key_file_set_boolean (keyfile, priv->id, "virtual", TRUE);

	/* convert to string */
	data = g_key_file_to_data (keyfile, NULL, &error_local);
	if (data == NULL) {
		ret = FALSE;
		g_set_error (error, 1, 0, "failed to convert config: %s", error_local->message);
		g_error_free (error_local);
		goto out;
	}

	/* save contents */
	ret = g_file_set_contents (filename, data, -1, &error_local);
	if (!ret) {
		g_set_error (error, 1, 0, "failed to save config: %s", error_local->message);
		g_error_free (error_local);
		goto out;
	}

	/* update status */
	gcm_device_set_saved (device, TRUE);
out:
	g_free (timespec);
	g_free (data);
	g_free (filename);
	g_free (dirname);
	if (file != NULL)
		g_object_unref (file);
	if (keyfile != NULL)
		g_key_file_free (keyfile);
	return ret;
}

/**
 * gcm_device_apply:
 **/
gboolean
gcm_device_apply (GcmDevice *device, GError **error)
{
	gboolean ret = TRUE;
	GcmDeviceClass *klass = GCM_DEVICE_GET_CLASS (device);

	/* no support */
	if (klass->apply == NULL) {
		egg_debug ("no klass support for %s", device->priv->id);
		goto out;
	}

	/* run the callback */
	ret = klass->apply (device, error);
out:
	return ret;
}

/**
 * gcm_device_get_property:
 **/
static void
gcm_device_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GcmDevice *device = GCM_DEVICE (object);
	GcmDevicePrivate *priv = device->priv;

	switch (prop_id) {
	case PROP_KIND:
		g_value_set_uint (value, priv->kind);
		break;
	case PROP_ID:
		g_value_set_string (value, priv->id);
		break;
	case PROP_CONNECTED:
		g_value_set_boolean (value, priv->connected);
		break;
	case PROP_VIRTUAL:
		g_value_set_boolean (value, priv->virtual);
		break;
	case PROP_SAVED:
		g_value_set_boolean (value, priv->saved);
		break;
	case PROP_SERIAL:
		g_value_set_string (value, priv->serial);
		break;
	case PROP_MODEL:
		g_value_set_string (value, priv->model);
		break;
	case PROP_MANUFACTURER:
		g_value_set_string (value, priv->manufacturer);
		break;
	case PROP_GAMMA:
		g_value_set_float (value, priv->gamma);
		break;
	case PROP_BRIGHTNESS:
		g_value_set_float (value, priv->brightness);
		break;
	case PROP_CONTRAST:
		g_value_set_float (value, priv->contrast);
		break;
	case PROP_PROFILE_FILENAME:
		g_value_set_string (value, priv->profile_filename);
		break;
	case PROP_TITLE:
		g_value_set_string (value, priv->title);
		break;
	case PROP_COLORSPACE:
		g_value_set_uint (value, priv->colorspace);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gcm_device_set_property:
 **/
static void
gcm_device_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	GcmDevice *device = GCM_DEVICE (object);

	switch (prop_id) {
	case PROP_KIND:
		gcm_device_set_kind (device, g_value_get_uint (value));
		break;
	case PROP_ID:
		gcm_device_set_id (device, g_value_get_string (value));
		break;
	case PROP_CONNECTED:
		gcm_device_set_connected (device, g_value_get_boolean (value));
		break;
	case PROP_VIRTUAL:
		gcm_device_set_virtual (device, g_value_get_boolean (value));
		break;
	case PROP_SAVED:
		gcm_device_set_saved (device, g_value_get_boolean (value));
		break;
	case PROP_SERIAL:
		gcm_device_set_serial (device, g_value_get_string (value));
		break;
	case PROP_MODEL:
		gcm_device_set_model (device, g_value_get_string (value));
		break;
	case PROP_MANUFACTURER:
		gcm_device_set_manufacturer (device, g_value_get_string (value));
		break;
	case PROP_TITLE:
		gcm_device_set_title (device, g_value_get_string (value));
		break;
	case PROP_PROFILE_FILENAME:
		gcm_device_set_profile_filename (device, g_value_get_string (value));
		break;
	case PROP_GAMMA:
		gcm_device_set_gamma (device, g_value_get_float (value));
		break;
	case PROP_BRIGHTNESS:
		gcm_device_set_brightness (device, g_value_get_float (value));
		break;
	case PROP_CONTRAST:
		gcm_device_set_contrast (device, g_value_get_float (value));
		break;
	case PROP_COLORSPACE:
		gcm_device_set_colorspace (device, g_value_get_uint (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gcm_device_class_init:
 **/
static void
gcm_device_class_init (GcmDeviceClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gcm_device_finalize;
	object_class->get_property = gcm_device_get_property;
	object_class->set_property = gcm_device_set_property;

	/**
	 * GcmDevice:kind:
	 */
	pspec = g_param_spec_uint ("kind", NULL, NULL,
				   0, G_MAXUINT, 0,
				   G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_KIND, pspec);

	/**
	 * GcmDevice:id:
	 */
	pspec = g_param_spec_string ("id", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_ID, pspec);

	/**
	 * GcmDevice:connected:
	 */
	pspec = g_param_spec_boolean ("connected", NULL, NULL,
				      FALSE,
				      G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_CONNECTED, pspec);

	/**
	 * GcmDevice:virtual:
	 */
	pspec = g_param_spec_boolean ("virtual", NULL, NULL,
				      FALSE,
				      G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_VIRTUAL, pspec);

	/**
	 * GcmDevice:saved:
	 */
	pspec = g_param_spec_boolean ("saved", NULL, NULL,
				      FALSE,
				      G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_SAVED, pspec);

	/**
	 * GcmCalibrate:serial:
	 */
	pspec = g_param_spec_string ("serial", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_SERIAL, pspec);

	/**
	 * GcmCalibrate:model:
	 */
	pspec = g_param_spec_string ("model", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_MODEL, pspec);

	/**
	 * GcmCalibrate:manufacturer:
	 */
	pspec = g_param_spec_string ("manufacturer", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_MANUFACTURER, pspec);

	/**
	 * GcmDevice:gamma:
	 */
	pspec = g_param_spec_float ("gamma", NULL, NULL,
				    0.0, G_MAXFLOAT, 1.01,
				    G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_GAMMA, pspec);

	/**
	 * GcmDevice:brightness:
	 */
	pspec = g_param_spec_float ("brightness", NULL, NULL,
				    0.0, G_MAXFLOAT, 1.02,
				    G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_BRIGHTNESS, pspec);

	/**
	 * GcmDevice:contrast:
	 */
	pspec = g_param_spec_float ("contrast", NULL, NULL,
				    0.0, G_MAXFLOAT, 1.03,
				    G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_CONTRAST, pspec);

	/**
	 * GcmDevice:profile-filename:
	 */
	pspec = g_param_spec_string ("profile-filename", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_PROFILE_FILENAME, pspec);

	/**
	 * GcmDevice:title:
	 */
	pspec = g_param_spec_string ("title", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_TITLE, pspec);

	/**
	 * GcmDevice:colorspace:
	 */
	pspec = g_param_spec_uint ("colorspace", NULL, NULL,
				   0, GCM_COLORSPACE_LAST, GCM_COLORSPACE_UNKNOWN,
				   G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_COLORSPACE, pspec);

	/**
	 * GcmDevice::changed:
	 **/
	signals[SIGNAL_CHANGED] =
		g_signal_new ("changed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GcmDeviceClass, changed),
			      NULL, NULL, g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	g_type_class_add_private (klass, sizeof (GcmDevicePrivate));
}

/**
 * gcm_device_init:
 **/
static void
gcm_device_init (GcmDevice *device)
{
	device->priv = GCM_DEVICE_GET_PRIVATE (device);
	device->priv->changed_id = 0;
	device->priv->id = NULL;
	device->priv->saved = FALSE;
	device->priv->connected = FALSE;
	device->priv->virtual = FALSE;
	device->priv->serial = NULL;
	device->priv->manufacturer = NULL;
	device->priv->model = NULL;
	device->priv->profile_filename = NULL;
	device->priv->modified_time = 0;
	device->priv->settings = g_settings_new (GCM_SETTINGS_SCHEMA);
	device->priv->gamma = g_settings_get_double (device->priv->settings, GCM_SETTINGS_DEFAULT_GAMMA);
	if (device->priv->gamma < 0.01)
		device->priv->gamma = 1.0f;
	device->priv->brightness = 0.0f;
	device->priv->contrast = 100.f;
	device->priv->colorspace = GCM_COLORSPACE_UNKNOWN;
}

/**
 * gcm_device_finalize:
 **/
static void
gcm_device_finalize (GObject *object)
{
	GcmDevice *device = GCM_DEVICE (object);
	GcmDevicePrivate *priv = device->priv;

	/* remove any pending signal */
	if (priv->changed_id != 0)
		g_source_remove (priv->changed_id);

	g_free (priv->title);
	g_free (priv->id);
	g_free (priv->serial);
	g_free (priv->manufacturer);
	g_free (priv->model);
	g_free (priv->profile_filename);
	g_object_unref (priv->settings);

	G_OBJECT_CLASS (gcm_device_parent_class)->finalize (object);
}


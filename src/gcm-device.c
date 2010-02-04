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
 * SECTION:gcm-device
 * @short_description: Color managed device object
 *
 * This object represents a device that can be colour managed.
 */

#include "config.h"

#include <glib-object.h>
#include <gconf/gconf-client.h>

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
	gboolean			 connected;
	gboolean			 saved;
	gfloat				 gamma;
	gfloat				 brightness;
	gfloat				 contrast;
	GcmDeviceTypeEnum		 type;
	gchar				*id;
	gchar				*serial;
	gchar				*manufacturer;
	gchar				*model;
	gchar				*profile_filename;
	gchar				*title;
	GConfClient			*gconf_client;
};

enum {
	PROP_0,
	PROP_TYPE,
	PROP_ID,
	PROP_CONNECTED,
	PROP_SAVED,
	PROP_SERIAL,
	PROP_MODEL,
	PROP_MANUFACTURER,
	PROP_GAMMA,
	PROP_BRIGHTNESS,
	PROP_CONTRAST,
	PROP_PROFILE_FILENAME,
	PROP_TITLE,
	PROP_NATIVE_DEVICE_UDEV,
	PROP_LAST
};

G_DEFINE_TYPE (GcmDevice, gcm_device, G_TYPE_OBJECT)

/**
 * gcm_device_type_enum_from_string:
 **/
GcmDeviceTypeEnum
gcm_device_type_enum_from_string (const gchar *type)
{
	if (g_strcmp0 (type, "display") == 0)
		return GCM_DEVICE_TYPE_ENUM_DISPLAY;
	if (g_strcmp0 (type, "scanner") == 0)
		return GCM_DEVICE_TYPE_ENUM_SCANNER;
	if (g_strcmp0 (type, "printer") == 0)
		return GCM_DEVICE_TYPE_ENUM_PRINTER;
	if (g_strcmp0 (type, "camera") == 0)
		return GCM_DEVICE_TYPE_ENUM_CAMERA;
	return GCM_DEVICE_TYPE_ENUM_UNKNOWN;
}

/**
 * gcm_device_type_enum_to_string:
 **/
const gchar *
gcm_device_type_enum_to_string (GcmDeviceTypeEnum type)
{
	if (type == GCM_DEVICE_TYPE_ENUM_DISPLAY)
		return "display";
	if (type == GCM_DEVICE_TYPE_ENUM_SCANNER)
		return "scanner";
	if (type == GCM_DEVICE_TYPE_ENUM_PRINTER)
		return "printer";
	if (type == GCM_DEVICE_TYPE_ENUM_CAMERA)
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

	g_return_val_if_fail (GCM_IS_DEVICE (device), FALSE);

	/* no profile to load */
	if (device->priv->profile_filename == NULL)
		goto out;

	/* load the profile if it's set */
	if (device->priv->profile_filename != NULL) {

		/* if the profile was deleted */
		ret = g_file_test (device->priv->profile_filename, G_FILE_TEST_EXISTS);
		if (!ret) {
			egg_warning ("the file was deleted and can't be loaded: %s", device->priv->profile_filename);
			/* this is not fatal */
			ret = TRUE;
			goto out;
		}
	}
out:
	return ret;
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
 * gcm_device_load:
 **/
gboolean
gcm_device_load (GcmDevice *device, GError **error)
{
	gboolean ret;
	GKeyFile *file = NULL;
	GError *error_local = NULL;
	gchar *filename = NULL;

	g_return_val_if_fail (GCM_IS_DEVICE (device), FALSE);
	g_return_val_if_fail (device->priv->id != NULL, FALSE);

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
	ret = g_key_file_has_group (file, device->priv->id);
	if (!ret) {
		/* not fatal */
		egg_warning ("failed to find parameters for %s", device->priv->id);
		ret = TRUE;
		goto out;
	}

	/* we are backed by a keyfile */
	g_object_set (device,
		      "saved", TRUE,
		      NULL);

	/* load data */
	g_free (device->priv->profile_filename);
	device->priv->profile_filename = g_key_file_get_string (file, device->priv->id, "profile", NULL);
	if (device->priv->serial == NULL)
		device->priv->serial = g_key_file_get_string (file, device->priv->id, "serial", NULL);
	if (device->priv->model == NULL)
		device->priv->model = g_key_file_get_string (file, device->priv->id, "model", NULL);
	if (device->priv->manufacturer == NULL)
		device->priv->manufacturer = g_key_file_get_string (file, device->priv->id, "manufacturer", NULL);
	device->priv->gamma = g_key_file_get_double (file, device->priv->id, "gamma", &error_local);
	if (error_local != NULL) {
		device->priv->gamma = gconf_client_get_float (device->priv->gconf_client, "/apps/gnome-color-manager/default_gamma", NULL);
		if (device->priv->gamma < 0.1f)
			device->priv->gamma = 1.0f;
		g_clear_error (&error_local);
	}
	device->priv->brightness = g_key_file_get_double (file, device->priv->id, "brightness", &error_local);
	if (error_local != NULL) {
		device->priv->brightness = 0.0f;
		g_clear_error (&error_local);
	}
	device->priv->contrast = g_key_file_get_double (file, device->priv->id, "contrast", &error_local);
	if (error_local != NULL) {
		device->priv->contrast = 100.0f;
		g_clear_error (&error_local);
	}

	/* load this */
	ret = gcm_device_load_from_profile (device, &error_local);
	if (!ret) {

		/* just print a warning, this is not fatal */
		egg_warning ("failed to load profile %s: %s", device->priv->profile_filename, error_local->message);
		g_error_free (error_local);

		/* recover as the file might have been corrupted */
		g_free (device->priv->profile_filename);
		device->priv->profile_filename = NULL;
		ret = TRUE;
	}
out:
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
	GError *error_local = NULL;

	g_return_val_if_fail (GCM_IS_DEVICE (device), FALSE);
	g_return_val_if_fail (device->priv->id != NULL, FALSE);

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

	/* save data */
	if (device->priv->profile_filename == NULL)
		g_key_file_remove_key (keyfile, device->priv->id, "profile", NULL);
	else
		g_key_file_set_string (keyfile, device->priv->id, "profile", device->priv->profile_filename);

	/* save device specific data */
	if (device->priv->serial == NULL)
		g_key_file_remove_key (keyfile, device->priv->id, "serial", NULL);
	else
		g_key_file_set_string (keyfile, device->priv->id, "serial", device->priv->serial);
	if (device->priv->model == NULL)
		g_key_file_remove_key (keyfile, device->priv->id, "model", NULL);
	else
		g_key_file_set_string (keyfile, device->priv->id, "model", device->priv->model);
	if (device->priv->manufacturer == NULL)
		g_key_file_remove_key (keyfile, device->priv->id, "manufacturer", NULL);
	else
		g_key_file_set_string (keyfile, device->priv->id, "manufacturer", device->priv->manufacturer);

	/* only save gamma if not the default */
	if (device->priv->gamma > 0.99 && device->priv->gamma < 1.01)
		g_key_file_remove_key (keyfile, device->priv->id, "gamma", NULL);
	else
		g_key_file_set_double (keyfile, device->priv->id, "gamma", device->priv->gamma);

	/* only save brightness if not the default */
	if (device->priv->brightness > -0.01 && device->priv->brightness < 0.01)
		g_key_file_remove_key (keyfile, device->priv->id, "brightness", NULL);
	else
		g_key_file_set_double (keyfile, device->priv->id, "brightness", device->priv->brightness);

	/* only save contrast if not the default */
	if (device->priv->contrast > 99.9 && device->priv->contrast < 100.1)
		g_key_file_remove_key (keyfile, device->priv->id, "contrast", NULL);
	else
		g_key_file_set_double (keyfile, device->priv->id, "contrast", device->priv->contrast);

	/* save other properties we'll need if we add this device offline */
	if (device->priv->title != NULL)
		g_key_file_set_string (keyfile, device->priv->id, "title", device->priv->title);
	g_key_file_set_string (keyfile, device->priv->id, "type", gcm_device_type_enum_to_string (device->priv->type));

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
	g_object_set (device,
		      "saved", TRUE,
		      NULL);
out:
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
 * gcm_device_get_property:
 **/
static void
gcm_device_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GcmDevice *device = GCM_DEVICE (object);
	GcmDevicePrivate *priv = device->priv;

	switch (prop_id) {
	case PROP_TYPE:
		g_value_set_uint (value, priv->type);
		break;
	case PROP_ID:
		g_value_set_string (value, priv->id);
		break;
	case PROP_CONNECTED:
		g_value_set_boolean (value, priv->connected);
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
	GcmDevicePrivate *priv = device->priv;

	switch (prop_id) {
	case PROP_TYPE:
		priv->type = g_value_get_uint (value);
		break;
	case PROP_ID:
		g_free (priv->id);
		priv->id = g_strdup (g_value_get_string (value));
		break;
	case PROP_CONNECTED:
		priv->connected = g_value_get_boolean (value);
		break;
	case PROP_SAVED:
		priv->saved = g_value_get_boolean (value);
		break;
	case PROP_SERIAL:
		g_free (priv->serial);
		priv->serial = g_strdup (g_value_get_string (value));
		break;
	case PROP_MODEL:
		g_free (priv->model);
		priv->model = g_strdup (g_value_get_string (value));
		break;
	case PROP_MANUFACTURER:
		g_free (priv->manufacturer);
		priv->manufacturer = g_strdup (g_value_get_string (value));
		break;
	case PROP_TITLE:
		g_free (priv->title);
		priv->title = g_strdup (g_value_get_string (value));
		break;
	case PROP_PROFILE_FILENAME:
		g_free (priv->profile_filename);
		priv->profile_filename = g_strdup (g_value_get_string (value));
		break;
	case PROP_GAMMA:
		priv->gamma = g_value_get_float (value);
		break;
	case PROP_BRIGHTNESS:
		priv->brightness = g_value_get_float (value);
		break;
	case PROP_CONTRAST:
		priv->contrast = g_value_get_float (value);
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
	 * GcmDevice:type:
	 */
	pspec = g_param_spec_uint ("type", NULL, NULL,
				   0, G_MAXUINT, 0,
				   G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_TYPE, pspec);

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

	g_type_class_add_private (klass, sizeof (GcmDevicePrivate));
}

/**
 * gcm_device_init:
 **/
static void
gcm_device_init (GcmDevice *device)
{
	GError *error = NULL;
	device->priv = GCM_DEVICE_GET_PRIVATE (device);
	device->priv->id = NULL;
	device->priv->saved = FALSE;
	device->priv->connected = FALSE;
	device->priv->serial = NULL;
	device->priv->manufacturer = NULL;
	device->priv->model = NULL;
	device->priv->profile_filename = NULL;
	device->priv->gconf_client = gconf_client_get_default ();
	device->priv->gamma = gconf_client_get_float (device->priv->gconf_client, GCM_SETTINGS_DEFAULT_GAMMA, &error);
	if (error != NULL) {
		egg_warning ("failed to get setup parameters: %s", error->message);
		g_error_free (error);
	}
	if (device->priv->gamma < 0.01)
		device->priv->gamma = 1.0f;
	device->priv->brightness = 0.0f;
	device->priv->contrast = 100.f;
}

/**
 * gcm_device_finalize:
 **/
static void
gcm_device_finalize (GObject *object)
{
	GcmDevice *device = GCM_DEVICE (object);
	GcmDevicePrivate *priv = device->priv;

	g_free (priv->title);
	g_free (priv->id);
	g_free (priv->serial);
	g_free (priv->manufacturer);
	g_free (priv->model);
	g_object_unref (priv->gconf_client);

	G_OBJECT_CLASS (gcm_device_parent_class)->finalize (object);
}

/**
 * gcm_device_new:
 *
 * Return value: a new GcmDevice object.
 **/
GcmDevice *
gcm_device_new (void)
{
	GcmDevice *device;
	device = g_object_new (GCM_TYPE_DEVICE, NULL);
	return GCM_DEVICE (device);
}

/***************************************************************************
 ***                          MAKE CHECK TESTS                           ***
 ***************************************************************************/
#ifdef EGG_TEST
#include "egg-test.h"

#include "gcm-device-udev.h"

void
gcm_device_test (EggTest *test)
{
	GcmDevice *device;
	gboolean ret;
	GError *error = NULL;
	gchar *filename;
	gchar *profile;
	gchar *data;
	const gchar *type;
	GcmDeviceTypeEnum type_enum;

	if (!egg_test_start (test, "GcmDevice"))
		return;

	/************************************************************/
	egg_test_title (test, "get a device object");
	device = gcm_device_udev_new ();
	egg_test_assert (test, device != NULL);

	/************************************************************/
	egg_test_title (test, "convert to recognised enum");
	type_enum = gcm_device_type_enum_from_string ("scanner");
	egg_test_assert (test, (type_enum == GCM_DEVICE_TYPE_ENUM_SCANNER));

	/************************************************************/
	egg_test_title (test, "convert to unrecognised enum");
	type_enum = gcm_device_type_enum_from_string ("xxx");
	egg_test_assert (test, (type_enum == GCM_DEVICE_TYPE_ENUM_UNKNOWN));

	/************************************************************/
	egg_test_title (test, "convert from recognised enum");
	type = gcm_device_type_enum_to_string (GCM_DEVICE_TYPE_ENUM_SCANNER);
	if (g_strcmp0 (type, "scanner") == 0)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "invalid value: %s", type);

	/************************************************************/
	egg_test_title (test, "convert from unrecognised enum");
	type = gcm_device_type_enum_to_string (GCM_DEVICE_TYPE_ENUM_UNKNOWN);
	if (g_strcmp0 (type, "unknown") == 0)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "invalid value: %s", type);

	/* set some properties */
	g_object_set (device,
		      "type", GCM_DEVICE_TYPE_ENUM_SCANNER,
		      "id", "sysfs_dummy_device",
		      "connected", TRUE,
		      "serial", "0123456789",
		      NULL);

	/************************************************************/
	egg_test_title (test, "get id");
	type = gcm_device_get_id (device);
	if (g_strcmp0 (type, "sysfs_dummy_device") == 0)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "invalid id: %s", type);

	/* ensure the file is nuked */
	filename = gcm_utils_get_default_config_location ();
	g_unlink (filename);

	/************************************************************/
	egg_test_title (test, "load from missing file");
	ret = gcm_device_load (device, &error);
	if (ret)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "failed to load: %s", error->message);

	/* get some properties */
	g_object_get (device,
		      "profile-filename", &profile,
		      NULL);

	/************************************************************/
	egg_test_title (test, "get profile filename");
	if (g_strcmp0 (profile, NULL) == 0)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "invalid profile: %s", profile);
	g_free (profile);

	/* empty file that exists */
	g_file_set_contents (filename, "", -1, NULL);

	/************************************************************/
	egg_test_title (test, "load from empty file");
	ret = gcm_device_load (device, &error);
	if (ret)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "failed to load: %s", error->message);

	/* set default file */
	g_file_set_contents (filename, "[sysfs_dummy_device]\ntitle=Canon - CanoScan\ntype=scanner\nprofile=/srv/sysfs_canon_canoscan.icc\n", -1, NULL);

	/************************************************************/
	egg_test_title (test, "load from configured file");
	ret = gcm_device_load (device, &error);
	if (ret)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "failed to load: %s", error->message);

	/* get some properties */
	g_object_get (device,
		      "profile-filename", &profile,
		      NULL);

	/************************************************************/
	egg_test_title (test, "get profile filename");
	if (g_strcmp0 (profile, "/srv/sysfs_canon_canoscan.icc") == 0)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "invalid profile: %s", profile);
	g_free (profile);

	/* set some properties */
	g_object_set (device,
		      "profile-filename", "/srv/sysfs_canon_canoscan.icc",
		      NULL);

	/* ensure the file is nuked, again */
	g_unlink (filename);

	/************************************************************/
	egg_test_title (test, "save to empty file");
	ret = gcm_device_save (device, &error);
	if (ret)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "failed to load: %s", error->message);

	/************************************************************/
	egg_test_title (test, "get contents of saved file");
	ret = g_file_get_contents (filename, &data, NULL, NULL);
	egg_test_assert (test, ret);

	/************************************************************/
	egg_test_title (test, "check data");
	if (g_strcmp0 (data, "\n[sysfs_dummy_device]\nprofile=/srv/sysfs_canon_canoscan.icc\nserial=0123456789\ntype=scanner\n") == 0)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "invalid data: %s", data);
	g_free (data);

	/* ensure the file is nuked, in case we are running in distcheck */
	g_unlink (filename);

	g_object_unref (device);
	g_free (filename);

	egg_test_end (test);
}
#endif


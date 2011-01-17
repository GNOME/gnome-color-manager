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
	gboolean		 use_edid_profile;
	gboolean		 saved;
	gfloat			 gamma;
	gfloat			 brightness;
	gfloat			 contrast;
	CdDeviceKind		 kind;
	gchar			*id;
	gchar			*serial;
	gchar			*manufacturer;
	gchar			*model;
	GPtrArray		*profiles;
	gchar			*title;
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
	PROP_USE_EDID_PROFILE,
	PROP_SAVED,
	PROP_SERIAL,
	PROP_MODEL,
	PROP_MANUFACTURER,
	PROP_GAMMA,
	PROP_BRIGHTNESS,
	PROP_CONTRAST,
	PROP_PROFILES,
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
	g_debug ("emit changed: %s", gcm_device_get_id (device));
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
#if GLIB_CHECK_VERSION(2,25,8)
	g_source_set_name_by_id (device->priv->changed_id, "[GcmDevice] device changed");
#endif
out:
	/* unlock */
	g_static_mutex_unlock (&mutex);
}

/**
 * cd_device_kind_from_string:
 **/
CdDeviceKind
cd_device_kind_from_string (const gchar *kind)
{
	if (g_strcmp0 (kind, "display") == 0)
		return CD_DEVICE_KIND_DISPLAY;
	if (g_strcmp0 (kind, "scanner") == 0)
		return CD_DEVICE_KIND_SCANNER;
	if (g_strcmp0 (kind, "printer") == 0)
		return CD_DEVICE_KIND_PRINTER;
	if (g_strcmp0 (kind, "camera") == 0)
		return CD_DEVICE_KIND_CAMERA;
	return CD_DEVICE_KIND_UNKNOWN;
}

/**
 * cd_device_kind_to_string:
 **/
const gchar *
cd_device_kind_to_string (CdDeviceKind kind)
{
	if (kind == CD_DEVICE_KIND_DISPLAY)
		return "display";
	if (kind == CD_DEVICE_KIND_SCANNER)
		return "scanner";
	if (kind == CD_DEVICE_KIND_PRINTER)
		return "printer";
	if (kind == CD_DEVICE_KIND_CAMERA)
		return "camera";
	return "unknown";
}

/**
 * gcm_device_get_kind:
 **/
CdDeviceKind
gcm_device_get_kind (GcmDevice *device)
{
	g_return_val_if_fail (GCM_IS_DEVICE (device), CD_DEVICE_KIND_UNKNOWN);
	return device->priv->kind;
}

/**
 * gcm_device_set_kind:
 **/
void
gcm_device_set_kind (GcmDevice *device, CdDeviceKind kind)
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
 * gcm_device_get_use_edid_profile:
 **/
gboolean
gcm_device_get_use_edid_profile (GcmDevice *device)
{
	g_return_val_if_fail (GCM_IS_DEVICE (device), FALSE);
	return device->priv->use_edid_profile;
}

/**
 * gcm_device_set_use_edid_profile:
 **/
void
gcm_device_set_use_edid_profile (GcmDevice *device, gboolean use_edid_profile)
{
	g_return_if_fail (GCM_IS_DEVICE (device));
	device->priv->use_edid_profile = use_edid_profile;
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
 * gcm_device_get_profiles:
 *
 * Return value: the profiles. Free with g_ptr_array_unref()
 **/
GPtrArray *
gcm_device_get_profiles (GcmDevice *device)
{
	g_return_val_if_fail (GCM_IS_DEVICE (device), NULL);
	return g_ptr_array_ref (device->priv->profiles);
}

/**
 * gcm_device_set_profiles:
 **/
void
gcm_device_set_profiles (GcmDevice *device, GPtrArray *profiles)
{
	g_return_if_fail (GCM_IS_DEVICE (device));
	g_return_if_fail (profiles != NULL);

	g_ptr_array_unref (device->priv->profiles);
	device->priv->profiles = g_ptr_array_ref (profiles);
	gcm_device_changed (device);
}

/**
 * gcm_device_profile_add:
 **/
gboolean
gcm_device_profile_add (GcmDevice *device, GcmProfile *profile, GError **error)
{
	guint i;
	gboolean ret = FALSE;
	const gchar *md5;
	GcmProfile *profile_tmp;

	g_return_val_if_fail (GCM_IS_DEVICE (device), FALSE);
	g_return_val_if_fail (profile != NULL, FALSE);

	/* check if already exists */
	md5 = gcm_profile_get_checksum (profile);
	for (i=0; i<device->priv->profiles->len; i++) {
		profile_tmp = g_ptr_array_index (device->priv->profiles, i);
		if (g_strcmp0 (md5, gcm_profile_get_checksum (profile_tmp)) == 0) {
			g_set_error (error, GCM_DEVICE_ERROR, GCM_DEVICE_ERROR_INTERNAL,
				     "already added %s", gcm_profile_get_filename (profile));
			goto out;
		}
	}

	/* add */
	g_ptr_array_add (device->priv->profiles, g_object_ref (profile));
	gcm_device_changed (device);
	ret = TRUE;
out:
	return ret;
}

/**
 * gcm_device_profile_remove:
 **/
gboolean
gcm_device_profile_remove (GcmDevice *device, GcmProfile *profile, GError **error)
{
	guint i;
	gboolean ret = TRUE;
	const gchar *md5;
	GcmProfile *profile_tmp;

	g_return_val_if_fail (GCM_IS_DEVICE (device), FALSE);
	g_return_val_if_fail (profile != NULL, FALSE);

	/* array empty */
	if (device->priv->profiles->len == 0) {
		g_set_error_literal (error, GCM_DEVICE_ERROR, GCM_DEVICE_ERROR_INTERNAL,
				     "the profile array is empty");
		ret = FALSE;
		goto out;
	}

	/* check if exists */
	md5 = gcm_profile_get_checksum (profile);
	for (i=0; i<device->priv->profiles->len; i++) {
		profile_tmp = g_ptr_array_index (device->priv->profiles, i);
		if (g_strcmp0 (md5, gcm_profile_get_checksum (profile_tmp)) == 0) {
			g_ptr_array_remove_index (device->priv->profiles, i);
			gcm_device_changed (device);
			goto out;
		}
	}

	/* not present */
	g_set_error (error, GCM_DEVICE_ERROR, GCM_DEVICE_ERROR_INTERNAL,
		     "asked to remove %s that does not exist",
		     gcm_profile_get_filename (profile));
	ret = FALSE;
out:
	return ret;
}

/**
 * gcm_device_profile_set_default:
 **/
gboolean
gcm_device_profile_set_default (GcmDevice *device, GcmProfile *profile, GError **error)
{
	guint i;
	gboolean ret = TRUE;
	const gchar *md5;
	GcmProfile *profile_tmp;
	gpointer tmp;

	g_return_val_if_fail (GCM_IS_DEVICE (device), FALSE);
	g_return_val_if_fail (profile != NULL, FALSE);

	/* array empty */
	if (device->priv->profiles->len == 0) {
		g_set_error_literal (error, GCM_DEVICE_ERROR, GCM_DEVICE_ERROR_INTERNAL,
				     "the profile array is empty");
		ret = FALSE;
		goto out;
	}

	/* only one item */
	if (device->priv->profiles->len == 1)
		goto out;

	/* check if exists */
	md5 = gcm_profile_get_checksum (profile);
	for (i=0; i<device->priv->profiles->len; i++) {
		profile_tmp = g_ptr_array_index (device->priv->profiles, i);
		if (g_strcmp0 (md5, gcm_profile_get_checksum (profile_tmp)) == 0) {
			if (i == 0) {
				g_set_error_literal (error, GCM_DEVICE_ERROR, GCM_DEVICE_ERROR_INTERNAL,
						     "profile already set default");
				ret = FALSE;
				goto out;
			}
			/* swap the two pointer locations */
			tmp = device->priv->profiles->pdata[0];
			device->priv->profiles->pdata[0] = device->priv->profiles->pdata[i];
			device->priv->profiles->pdata[i] = tmp;
			gcm_device_changed (device);
			goto out;
		}
	}

	/* not present */
	g_set_error (error, GCM_DEVICE_ERROR, GCM_DEVICE_ERROR_INTERNAL,
		     "asked to set default %s that does not exist in list",
		     gcm_profile_get_filename (profile));
	ret = FALSE;
out:
	return ret;
}

/**
 * gcm_device_get_default_profile_filename:
 **/
const gchar *
gcm_device_get_default_profile_filename (GcmDevice *device)
{
	GcmProfile *profile;
	g_return_val_if_fail (GCM_IS_DEVICE (device), NULL);
	if (device->priv->profiles->len == 0)
		return NULL;
	profile = g_ptr_array_index (device->priv->profiles, 0);
	return gcm_profile_get_filename (profile);
}

/**
 * gcm_device_get_default_profile:
 **/
GcmProfile *
gcm_device_get_default_profile (GcmDevice *device)
{
	GcmProfile *profile;
	g_return_val_if_fail (GCM_IS_DEVICE (device), NULL);
	if (device->priv->profiles->len == 0)
		return NULL;
	profile = g_ptr_array_index (device->priv->profiles, 0);
	return profile;
}

/**
 * gcm_device_set_default_profile_filename:
 **/
void
gcm_device_set_default_profile_filename (GcmDevice *device, const gchar *profile_filename)
{
	GcmProfile *profile;
	GPtrArray *array;
	gboolean ret;
	GFile *file;
	GError *error = NULL;

	g_return_if_fail (GCM_IS_DEVICE (device));

	/* create new list */
	array = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	profile = gcm_profile_new ();

	/* TODO: parse here? */
	file = g_file_new_for_path (profile_filename);
	ret = gcm_profile_parse (profile, file, &error);
	if (!ret) {
		g_warning ("failed to parse: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* FIXME: don't just nuke the list */
	g_ptr_array_add (array, g_object_ref (profile));
	gcm_device_set_profiles (device, array);
out:
	g_ptr_array_unref (array);
	g_object_unref (profile);
	g_object_unref (file);
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
 * gcm_device_apply:
 **/
gboolean
gcm_device_apply (GcmDevice *device, GError **error)
{
	gboolean ret = TRUE;
	GcmDeviceClass *klass = GCM_DEVICE_GET_CLASS (device);

	/* no support */
	if (klass->apply == NULL) {
		g_debug ("no klass support for %s", device->priv->id);
		goto out;
	}

	/* run the callback */
	ret = klass->apply (device, error);
out:
	return ret;
}

/**
 * gcm_device_generate_profile:
 **/
GcmProfile *
gcm_device_generate_profile (GcmDevice *device, GError **error)
{
	GcmProfile *profile = NULL;
	GcmDeviceClass *klass = GCM_DEVICE_GET_CLASS (device);

	/* no support */
	if (klass->generate_profile == NULL) {
		g_set_error (error, GCM_DEVICE_ERROR, GCM_DEVICE_ERROR_NO_SUPPPORT,
			     "no klass support for %s", device->priv->id);
		goto out;
	}

	/* run the callback */
	profile = klass->generate_profile (device, error);
out:
	return profile;
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
	case PROP_USE_EDID_PROFILE:
		g_value_set_boolean (value, priv->use_edid_profile);
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
	case PROP_PROFILES:
		g_value_set_pointer (value, priv->profiles);
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
	case PROP_USE_EDID_PROFILE:
		gcm_device_set_use_edid_profile (device, g_value_get_boolean (value));
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
	case PROP_PROFILES:
		gcm_device_set_profiles (device, g_value_get_pointer (value));
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
	 * GcmDevice:use-edid-profile:
	 */
	pspec = g_param_spec_boolean ("use-edid-profile", NULL, NULL,
				      FALSE,
				      G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_USE_EDID_PROFILE, pspec);

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
	 * GcmDevice:profiles:
	 */
	pspec = g_param_spec_pointer ("profiles", NULL, NULL,
				      G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_PROFILES, pspec);

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
	device->priv->use_edid_profile = TRUE;
	device->priv->serial = NULL;
	device->priv->manufacturer = NULL;
	device->priv->model = NULL;
	device->priv->profiles = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	device->priv->modified_time = 0;
	device->priv->gamma = 1.0;
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
	g_ptr_array_unref (priv->profiles);

	G_OBJECT_CLASS (gcm_device_parent_class)->finalize (object);
}


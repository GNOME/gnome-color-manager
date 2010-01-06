/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008-2009 Richard Hughes <richard@hughsie.com>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <glib/gi18n.h>
#include <dbus/dbus-glib.h>
#include <gconf/gconf-client.h>

#include "egg-debug.h"

#include "gcm-utils.h"
#include "gcm-dbus.h"
#include "gcm-client.h"
#include "gcm-profile.h"

static void     gcm_dbus_finalize	(GObject	*object);

#define GCM_DBUS_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GCM_TYPE_DBUS, GcmDbusPrivate))

struct GcmDbusPrivate
{
	GConfClient		*gconf_client;
	GcmClient		*client;
	GTimer			*timer;
	gchar			*rendering_intent_display;
	gchar			*rendering_intent_softproof;
	gchar			*colorspace_rgb;
	gchar			*colorspace_cmyk;
};

enum {
	PROP_0,
	PROP_RENDERING_INTENT_DISPLAY,
	PROP_RENDERING_INTENT_SOFTPROOF,
	PROP_COLORSPACE_RGB,
	PROP_COLORSPACE_CMYK,
	PROP_LAST
};

enum {
	SIGNAL_CHANGED,
	SIGNAL_LAST,
};

static guint signals[SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE (GcmDbus, gcm_dbus, G_TYPE_OBJECT)

/**
 * gcm_dbus_error_quark:
 * Return value: Our personal error quark.
 **/
GQuark
gcm_dbus_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("gcm_dbus_error");
	return quark;
}

/**
 * gcm_dbus_error_get_type:
 **/
#define ENUM_ENTRY(NAME, DESC) { NAME, "" #NAME "", DESC }
GType
gcm_dbus_error_get_type (void)
{
	static GType etype = 0;

	if (etype == 0) {
		static const GEnumValue values[] =
		{
			ENUM_ENTRY (GCM_DBUS_ERROR_FAILED, "Failed"),
			ENUM_ENTRY (GCM_DBUS_ERROR_INTERNAL_ERROR, "InternalError"),
			{ 0, NULL, NULL }
		};
		etype = g_enum_register_static ("GcmDbusError", values);
	}
	return etype;
}


/**
 * gcm_dbus_get_property:
 **/
static void
gcm_dbus_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GcmDbus *dbus = GCM_DBUS (object);
	switch (prop_id) {
	case PROP_RENDERING_INTENT_DISPLAY:
		g_value_set_string (value, dbus->priv->rendering_intent_display);
		break;
	case PROP_COLORSPACE_RGB:
		g_value_set_string (value, dbus->priv->colorspace_rgb);
		break;
	case PROP_COLORSPACE_CMYK:
		g_value_set_string (value, dbus->priv->colorspace_cmyk);
		break;
	case PROP_RENDERING_INTENT_SOFTPROOF:
		g_value_set_string (value, dbus->priv->rendering_intent_softproof);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}

	/* reset time */
	g_timer_reset (dbus->priv->timer);
}

/**
 * gcm_dbus_set_property:
 **/
static void
gcm_dbus_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gcm_dbus_get_idle_time:
 **/
guint
gcm_dbus_get_idle_time (GcmDbus	*dbus)
{
	guint idle;
	idle = (guint) g_timer_elapsed (dbus->priv->timer, NULL);
	egg_debug ("we've been idle for %is", idle);
	return idle;
}

#define GCM_DBUS_STRUCT_STRING_STRING (dbus_g_type_get_struct ("GValueArray", G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID))

/**
 * gcm_dbus_get_profiles_for_device_internal:
 **/
static GPtrArray *
gcm_dbus_get_profiles_for_device_internal (GcmDbus *dbus, const gchar *sysfs_path)
{
	gboolean ret;
	gchar *filename;
	gchar *sysfs_path_tmp;
	guint i;
	GcmDevice *device;
	GcmProfile *profile;
	GError *error = NULL;
	GPtrArray *array;
	GPtrArray *array_devices;

	/* create a temp array */
	array = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);

	/* get list */
	array_devices = gcm_client_get_devices (dbus->priv->client);
	for (i=0; i<array_devices->len; i++) {
		device = g_ptr_array_index (array_devices, i);

		/* get the native path of this device */
		g_object_get (device,
			      "native-device-sysfs", &sysfs_path_tmp,
			      NULL);

		/* wrong type of device */
		if (sysfs_path_tmp == NULL)
			continue;

		/* compare what we have against what we were given */
		egg_debug ("comparing %s with %s", sysfs_path_tmp, sysfs_path);
		if (g_strcmp0 (sysfs_path_tmp, sysfs_path) == 0) {
			g_object_get (device,
				      "profile-filename", &filename,
				      NULL);

			/* open and parse filename */
			profile = gcm_profile_default_new ();
			ret = gcm_profile_parse (profile, filename, &error);
			if (!ret) {
				egg_warning ("failed to parse %s: %s", filename, error->message);
				g_clear_error (&error);
			} else {
				g_ptr_array_add (array, g_object_ref (profile));
			}

			/* unref */
			g_object_unref (profile);
			g_free (filename);
		}
		g_free (sysfs_path_tmp);
	}

	/* unref list of devices */
	g_ptr_array_unref (array_devices);
	return array;
}

/**
 * gcm_dbus_get_profiles_for_type_internal:
 **/
static GPtrArray *
gcm_dbus_get_profiles_for_type_internal (GcmDbus *dbus, GcmDeviceType type)
{
	const gchar *filename;
	gboolean ret;
	guint i;
	GcmProfile *profile;
	GcmProfileType profile_type;
	GcmProfileType type_tmp;
	GError *error = NULL;
	GPtrArray *array;
	GPtrArray *array_devices;

	/* create a temp array */
	array = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);

	/* get the correct profile type for the device type */
	profile_type = gcm_utils_device_type_to_profile_type (type);

	/* get list */
	array_devices = gcm_utils_get_profile_filenames ();
	for (i=0; i<array_devices->len; i++) {
		filename = g_ptr_array_index (array_devices, i);

		/* open and parse filename */
		profile = gcm_profile_default_new ();
		ret = gcm_profile_parse (profile, filename, &error);
		if (!ret) {
			egg_warning ("failed to parse %s: %s", filename, error->message);
			g_clear_error (&error);
		} else {
			/* get the native path of this device */
			g_object_get (profile,
				      "type", &type_tmp,
				      NULL);

			/* compare what we have against what we were given */
			egg_debug ("comparing %i with %i", type_tmp, profile_type);
			if (type_tmp == profile_type)
				g_ptr_array_add (array, g_object_ref (profile));
		}

		/* unref */
		g_object_unref (profile);
	}

	/* unref list of devices */
	g_ptr_array_unref (array_devices);
	return array;
}

/**
 * gcm_dbus_get_profiles_for_device:
 **/
void
gcm_dbus_get_profiles_for_device (GcmDbus *dbus, const gchar *sysfs_path, const gchar *options, DBusGMethodInvocation *context)
{
	GPtrArray *array_profiles;
	GcmProfile *profile;
	gchar *title;
	gchar *filename;
	guint i;
	GPtrArray *array_structs;
	GValue *value;

	egg_debug ("getting profiles for %s", sysfs_path);

	/* get array of profile filenames */
	array_profiles = gcm_dbus_get_profiles_for_device_internal (dbus, sysfs_path);

	/* copy data to dbus struct */
	array_structs = g_ptr_array_sized_new (array_profiles->len);
	for (i=0; i<array_profiles->len; i++) {
		profile = (GcmProfile *) g_ptr_array_index (array_profiles, i);

		/* get the data */
		g_object_get (profile,
			      "description", &title,
			      "filename", &filename,
			      NULL);

		value = g_new0 (GValue, 1);
		g_value_init (value, GCM_DBUS_STRUCT_STRING_STRING);
		g_value_take_boxed (value, dbus_g_type_specialized_construct (GCM_DBUS_STRUCT_STRING_STRING));
		dbus_g_type_struct_set (value, 0, title, 1, filename, -1);
		g_ptr_array_add (array_structs, g_value_get_boxed (value));
		g_free (value);
		g_free (title);
		g_free (filename);
	}

	/* return profiles */
	dbus_g_method_return (context, array_structs);

	/* reset time */
	g_timer_reset (dbus->priv->timer);

	g_ptr_array_unref (array_profiles);
}

/**
 * gcm_dbus_get_profiles_for_type:
 **/
void
gcm_dbus_get_profiles_for_type (GcmDbus *dbus, const gchar *type, const gchar *options, DBusGMethodInvocation *context)
{
	GPtrArray *array_profiles;
	GcmProfile *profile;
	gchar *title;
	gchar *filename;
	guint i;
	GPtrArray *array_structs;
	GValue *value;
	GcmDeviceType type_enum;

	egg_debug ("getting profiles for %s", type);

	/* get array of profile filenames */
	type_enum = gcm_device_type_from_text (type);
	array_profiles = gcm_dbus_get_profiles_for_type_internal (dbus, type_enum);

	/* copy data to dbus struct */
	array_structs = g_ptr_array_sized_new (array_profiles->len);
	for (i=0; i<array_profiles->len; i++) {
		profile = (GcmProfile *) g_ptr_array_index (array_profiles, i);

		/* get the data */
		g_object_get (profile,
			      "description", &title,
			      "filename", &filename,
			      NULL);

		value = g_new0 (GValue, 1);
		g_value_init (value, GCM_DBUS_STRUCT_STRING_STRING);
		g_value_take_boxed (value, dbus_g_type_specialized_construct (GCM_DBUS_STRUCT_STRING_STRING));
		dbus_g_type_struct_set (value, 0, title, 1, filename, -1);
		g_ptr_array_add (array_structs, g_value_get_boxed (value));
		g_free (value);
		g_free (title);
		g_free (filename);
	}

	/* return profiles */
	dbus_g_method_return (context, array_structs);

	/* reset time */
	g_timer_reset (dbus->priv->timer);

	g_ptr_array_unref (array_profiles);
}

/**
 * gcm_dbus_class_init:
 * @klass: The GcmDbusClass
 **/
static void
gcm_dbus_class_init (GcmDbusClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gcm_dbus_finalize;
	object_class->get_property = gcm_dbus_get_property;
	object_class->set_property = gcm_dbus_set_property;
	g_type_class_add_private (klass, sizeof (GcmDbusPrivate));

	signals[SIGNAL_CHANGED] =
		g_signal_new ("changed",
			      G_OBJECT_CLASS_TYPE (klass),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
			      0, NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	/**
	 * GcmDbus:rendering-intent-display:
	 */
	g_object_class_install_property (object_class,
					 PROP_RENDERING_INTENT_DISPLAY,
					 g_param_spec_string ("rendering-intent-display",
							      NULL, NULL,
							      NULL,
							      G_PARAM_READABLE));

	/**
	 * GcmDbus:rendering-intent-softproof:
	 */
	g_object_class_install_property (object_class,
					 PROP_RENDERING_INTENT_SOFTPROOF,
					 g_param_spec_string ("rendering-intent-softproof",
							      NULL, NULL,
							      NULL,
							      G_PARAM_READABLE));
	/**
	 * GcmDbus:colorspace-rgb:
	 */
	g_object_class_install_property (object_class,
					 PROP_COLORSPACE_RGB,
					 g_param_spec_string ("colorspace-rgb",
							      NULL, NULL,
							      NULL,
							      G_PARAM_READABLE));
	/**
	 * GcmDbus:colorspace-cmyk:
	 */
	g_object_class_install_property (object_class,
					 PROP_COLORSPACE_CMYK,
					 g_param_spec_string ("colorspace-cmyk",
							      NULL, NULL,
							      NULL,
							      G_PARAM_READABLE));
}

/**
 * gcm_dbus_gconf_key_changed_cb:
 *
 * We might have to do things when the gconf keys change; do them here.
 **/
static void
gcm_dbus_gconf_key_changed_cb (GConfClient *client, guint cnxn_id, GConfEntry *entry, GcmDbus *dbus)
{
	/* just emit signal */
	g_signal_emit (dbus, signals[SIGNAL_CHANGED], 0);
}

/**
 * gcm_dbus_init:
 * @dbus: This class instance
 **/
static void
gcm_dbus_init (GcmDbus *dbus)
{
	gboolean ret;
	GError *error = NULL;

	dbus->priv = GCM_DBUS_GET_PRIVATE (dbus);
	dbus->priv->gconf_client = gconf_client_get_default ();
	dbus->priv->client = gcm_client_new ();
	dbus->priv->timer = g_timer_new ();

	/* notify on changes */
	gconf_client_notify_add (dbus->priv->gconf_client, GCM_SETTINGS_DIR,
				 (GConfClientNotifyFunc) gcm_dbus_gconf_key_changed_cb,
				 dbus, NULL, NULL);

	/* coldplug */
	dbus->priv->rendering_intent_display = gconf_client_get_string (dbus->priv->gconf_client, GCM_SETTINGS_RENDERING_INTENT_DISPLAY, NULL);
	dbus->priv->rendering_intent_softproof = gconf_client_get_string (dbus->priv->gconf_client, GCM_SETTINGS_RENDERING_INTENT_SOFTPROOF, NULL);
	dbus->priv->colorspace_rgb = gconf_client_get_string (dbus->priv->gconf_client, GCM_SETTINGS_COLORSPACE_RGB, NULL);
	dbus->priv->colorspace_cmyk = gconf_client_get_string (dbus->priv->gconf_client, GCM_SETTINGS_COLORSPACE_CMYK, NULL);

	/* get all devices */
	ret = gcm_client_add_connected (dbus->priv->client, &error);
	if (!ret) {
		egg_warning ("failed to coldplug: %s", error->message);
		g_error_free (error);
	}
}

/**
 * gcm_dbus_finalize:
 * @object: The object to finalize
 **/
static void
gcm_dbus_finalize (GObject *object)
{
	GcmDbus *dbus;
	g_return_if_fail (PK_IS_DBUS (object));

	dbus = GCM_DBUS (object);
	g_return_if_fail (dbus->priv != NULL);
	g_free (dbus->priv->rendering_intent_display);
	g_free (dbus->priv->rendering_intent_softproof);
	g_free (dbus->priv->colorspace_rgb);
	g_free (dbus->priv->colorspace_cmyk);
	g_object_unref (dbus->priv->client);
	g_object_unref (dbus->priv->gconf_client);
	g_timer_destroy (dbus->priv->timer);

	G_OBJECT_CLASS (gcm_dbus_parent_class)->finalize (object);
}

/**
 * gcm_dbus_new:
 *
 * Return value: a new GcmDbus object.
 **/
GcmDbus *
gcm_dbus_new (void)
{
	GcmDbus *dbus;
	dbus = g_object_new (GCM_TYPE_DBUS, NULL);
	return GCM_DBUS (dbus);
}


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

#include "gcm-dbus.h"
#include "gcm-client.h"

static void     gcm_dbus_finalize	(GObject	*object);

#define GCM_DBUS_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GCM_TYPE_DBUS, GcmDbusPrivate))

struct GcmDbusPrivate
{
	GConfClient		*gconf_client;
	GcmClient		*client;
	GTimer			*timer;
};

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

/**
 * gcm_dbus_get_profiles_for_device:
 **/
void
gcm_dbus_get_profiles_for_device (GcmDbus *dbus, const gchar *sysfs_path, const gchar *options, DBusGMethodInvocation *context)
{
	GPtrArray *array;
	GcmDevice *device;
	gchar **profiles;
	gchar *sysfs_path_tmp;
	guint i;

	egg_debug ("getting profiles for %s", sysfs_path);

	/* for now, hardcode one profile per device */
	profiles = g_new0 (gchar *, 2);

	/* get list */
	array = gcm_client_get_devices (dbus->priv->client);
	for (i=0; i<array->len; i++) {
		device = g_ptr_array_index (array, i);

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
				      "profile", &profiles[0],
				      NULL);
			g_free (sysfs_path_tmp);
			break;
		}
		g_free (sysfs_path_tmp);
	}

	/* return profiles */
	dbus_g_method_return (context, profiles);

	/* reset time */
	g_timer_reset (dbus->priv->timer);

	g_strfreev (profiles);
	g_ptr_array_unref (array);
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
	g_type_class_add_private (klass, sizeof (GcmDbusPrivate));
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

	/* get all devices */
	ret = gcm_client_coldplug (dbus->priv->client, &error);
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


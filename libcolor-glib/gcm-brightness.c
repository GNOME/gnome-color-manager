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
 * SECTION:gcm-brightness
 * @short_description: Object to allow setting the brightness using gnome-power-manager
 *
 * This object sets the laptop panel brightness using gnome-power-manager.
 */

#include "config.h"

#include <glib-object.h>
#include <gio/gio.h>

#include "gcm-brightness.h"

static void     gcm_brightness_finalize	(GObject     *object);

#define GCM_BRIGHTNESS_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GCM_TYPE_BRIGHTNESS, GcmBrightnessPrivate))

/**
 * GcmBrightnessPrivate:
 *
 * Private #GcmBrightness data
 **/
struct _GcmBrightnessPrivate
{
	guint				 percentage;
	GDBusConnection			*connection;
};

enum {
	PROP_0,
	PROP_PERCENTAGE,
	PROP_LAST
};

G_DEFINE_TYPE (GcmBrightness, gcm_brightness, G_TYPE_OBJECT)

#define	GPM_DBUS_SERVICE		"org.gnome.PowerManager"
#define	GPM_DBUS_INTERFACE_BACKLIGHT	"org.gnome.PowerManager.Backlight"
#define	GPM_DBUS_PATH_BACKLIGHT		"/org/gnome/PowerManager/Backlight"

/**
 * gcm_brightness_set_percentage:
 *
 * Since: 2.91.1
 **/
gboolean
gcm_brightness_set_percentage (GcmBrightness *brightness, guint percentage, GError **error)
{
	GcmBrightnessPrivate *priv = brightness->priv;
	gboolean ret = FALSE;
	GVariant *args = NULL;
	GVariant *response = NULL;

	g_return_val_if_fail (GCM_IS_BRIGHTNESS (brightness), FALSE);
	g_return_val_if_fail (percentage <= 100, FALSE);

	/* get a session bus connection */
	if (priv->connection == NULL) {
		priv->connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, error);
		if (priv->connection == NULL)
			goto out;
	}

	/* execute sync method */
	args = g_variant_new ("(u)", percentage),
	response = g_dbus_connection_call_sync (priv->connection,
						GPM_DBUS_SERVICE,
						GPM_DBUS_PATH_BACKLIGHT,
						GPM_DBUS_INTERFACE_BACKLIGHT,
						"SetBrightness",
						args,
						NULL,
						G_DBUS_CALL_FLAGS_NONE,
						-1, NULL, error);
	if (response == NULL)
		goto out;

	/* success */
	ret = TRUE;
out:
	if (args != NULL)
		g_variant_unref (args);
	if (response != NULL)
		g_variant_unref (response);
	return ret;
}

/**
 * gcm_brightness_get_percentage:
 *
 * Since: 2.91.1
 **/
gboolean
gcm_brightness_get_percentage (GcmBrightness *brightness, guint *percentage, GError **error)
{
	GcmBrightnessPrivate *priv = brightness->priv;
	gboolean ret = FALSE;
	GVariant *response = NULL;

	g_return_val_if_fail (GCM_IS_BRIGHTNESS (brightness), FALSE);

	/* get a session bus connection */
	if (priv->connection == NULL) {
		priv->connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, error);
		if (priv->connection == NULL)
			goto out;
	}

	/* execute sync method */
	response = g_dbus_connection_call_sync (priv->connection,
						GPM_DBUS_SERVICE,
						GPM_DBUS_PATH_BACKLIGHT,
						GPM_DBUS_INTERFACE_BACKLIGHT,
						"GetBrightness",
						NULL,
						G_VARIANT_TYPE ("(u)"),
						G_DBUS_CALL_FLAGS_NONE,
						-1, NULL, error);
	if (response == NULL)
		goto out;

	/* get the brightness */
	g_variant_get (response, "(u)", &priv->percentage);

	/* copy if set */
	if (percentage != NULL)
		*percentage = priv->percentage;

	/* success */
	ret = TRUE;
out:
	if (response != NULL)
		g_variant_unref (response);
	return ret;
}

/**
 * gcm_brightness_get_property:
 **/
static void
gcm_brightness_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GcmBrightness *brightness = GCM_BRIGHTNESS (object);
	GcmBrightnessPrivate *priv = brightness->priv;

	switch (prop_id) {
	case PROP_PERCENTAGE:
		g_value_set_uint (value, priv->percentage);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gcm_brightness_set_property:
 **/
static void
gcm_brightness_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gcm_brightness_class_init:
 **/
static void
gcm_brightness_class_init (GcmBrightnessClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gcm_brightness_finalize;
	object_class->get_property = gcm_brightness_get_property;
	object_class->set_property = gcm_brightness_set_property;

	/**
	 * GcmBrightness:percentage:
	 */
	pspec = g_param_spec_uint ("percentage", NULL, NULL,
				   0, 100, 0,
				   G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_PERCENTAGE, pspec);

	g_type_class_add_private (klass, sizeof (GcmBrightnessPrivate));
}

/**
 * gcm_brightness_init:
 **/
static void
gcm_brightness_init (GcmBrightness *brightness)
{
	brightness->priv = GCM_BRIGHTNESS_GET_PRIVATE (brightness);
	brightness->priv->percentage = 0;
}

/**
 * gcm_brightness_finalize:
 **/
static void
gcm_brightness_finalize (GObject *object)
{
	GcmBrightness *brightness = GCM_BRIGHTNESS (object);
	GcmBrightnessPrivate *priv = brightness->priv;

	if (priv->connection != NULL)
		g_object_unref (priv->connection);

	G_OBJECT_CLASS (gcm_brightness_parent_class)->finalize (object);
}

/**
 * gcm_brightness_new:
 *
 * Return value: a new GcmBrightness object.
 *
 * Since: 2.91.1
 **/
GcmBrightness *
gcm_brightness_new (void)
{
	GcmBrightness *brightness;
	brightness = g_object_new (GCM_TYPE_BRIGHTNESS, NULL);
	return GCM_BRIGHTNESS (brightness);
}


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
 * GNU General Public License for more brightness.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:gcm-brightness
 * @short_description: Object to allow setting the brightness using gnome-power-manager
 *
 * This object sets the laptop panel brightness using gnome-power-manager.
 */

#include "config.h"

#include <glib-object.h>
//#include <math.h>
//#include <string.h>
//#include <gio/gio.h>
//#include <stdlib.h>
#include <dbus/dbus-glib.h>

#include "gcm-brightness.h"

#include "egg-debug.h"

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
	DBusGProxy			*proxy;
	DBusGConnection			*connection;
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
 **/
gboolean
gcm_brightness_set_percentage (GcmBrightness *brightness, guint percentage, GError **error)
{
	GcmBrightnessPrivate *priv = brightness->priv;
	gboolean ret = FALSE;

	g_return_val_if_fail (GCM_IS_BRIGHTNESS (brightness), FALSE);
	g_return_val_if_fail (percentage <= 100, FALSE);

	/* are we connected */
	if (priv->proxy == NULL) {
		if (error != NULL)
			*error = g_error_new (1, 0, "not connected to gnome-power-manager");
		goto out;
	}

	/* set the brightness */
	ret = dbus_g_proxy_call (priv->proxy, "SetBrightness", error,
				 G_TYPE_UINT, percentage,
				 G_TYPE_INVALID,
				 G_TYPE_INVALID);
	if (!ret)
		goto out;
out:
	return ret;
}

/**
 * gcm_brightness_get_percentage:
 **/
gboolean
gcm_brightness_get_percentage (GcmBrightness *brightness, guint *percentage, GError **error)
{
	GcmBrightnessPrivate *priv = brightness->priv;
	gboolean ret = FALSE;

	g_return_val_if_fail (GCM_IS_BRIGHTNESS (brightness), FALSE);

	/* are we connected */
	if (priv->proxy == NULL) {
		if (error != NULL)
			*error = g_error_new (1, 0, "not connected to gnome-power-manager");
		goto out;
	}

	/* get the brightness */
	ret = dbus_g_proxy_call (priv->proxy, "GetBrightness", error,
				 G_TYPE_INVALID,
				 G_TYPE_UINT, &priv->percentage,
				 G_TYPE_INVALID);
	if (!ret)
		goto out;

	/* copy if set */
	if (percentage != NULL)
		*percentage = priv->percentage;
out:
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
	GError *error = NULL;

	brightness->priv = GCM_BRIGHTNESS_GET_PRIVATE (brightness);
	brightness->priv->percentage = 0;

	/* get a session connection */
	brightness->priv->connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	if (brightness->priv->connection == NULL) {
		egg_warning ("Could not connect to DBUS daemon: %s", error->message);
		g_error_free (error);
		return;
	}

	/* get a proxy to gnome-power-manager */
	brightness->priv->proxy = dbus_g_proxy_new_for_name_owner (brightness->priv->connection,
								   GPM_DBUS_SERVICE, GPM_DBUS_PATH_BACKLIGHT, GPM_DBUS_INTERFACE_BACKLIGHT, &error);
	if (brightness->priv->proxy == NULL) {
		egg_warning ("Cannot connect, maybe gnome-power-manager is not running: %s\n", error->message);
		g_error_free (error);
	}
}

/**
 * gcm_brightness_finalize:
 **/
static void
gcm_brightness_finalize (GObject *object)
{
	GcmBrightness *brightness = GCM_BRIGHTNESS (object);
	GcmBrightnessPrivate *priv = brightness->priv;

	if (priv->proxy != NULL)
		g_object_unref (priv->proxy);

	G_OBJECT_CLASS (gcm_brightness_parent_class)->finalize (object);
}

/**
 * gcm_brightness_new:
 *
 * Return value: a new GcmBrightness object.
 **/
GcmBrightness *
gcm_brightness_new (void)
{
	GcmBrightness *brightness;
	brightness = g_object_new (GCM_TYPE_BRIGHTNESS, NULL);
	return GCM_BRIGHTNESS (brightness);
}

/***************************************************************************
 ***                          MAKE CHECK TESTS                           ***
 ***************************************************************************/
#ifdef EGG_TEST
#include "egg-test.h"

void
gcm_brightness_test (EggTest *test)
{
	GcmBrightness *brightness;
	gboolean ret;
	GError *error = NULL;
	guint orig_percentage;
	guint percentage;

	if (!egg_test_start (test, "GcmBrightness"))
		return;

	/************************************************************/
	egg_test_title (test, "get a brightness object");
	brightness = gcm_brightness_new ();
	egg_test_assert (test, brightness != NULL);

	/************************************************************/
	egg_test_title (test, "get original brightness");
	ret = gcm_brightness_get_percentage (brightness, &orig_percentage, &error);
	if (ret)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "failed to get brightness: %s", error->message);

	/************************************************************/
	egg_test_title (test, "set the new brightness");
	ret = gcm_brightness_set_percentage (brightness, 10, &error);
	if (ret)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "failed to set brightness: %s", error->message);

	/************************************************************/
	egg_test_title (test, "get the new brightness");
	ret = gcm_brightness_get_percentage (brightness, &percentage, &error);
	if (!ret)
		egg_test_failed (test, "failed to get brightness: %s", error->message);
	else if (percentage < 5 || percentage > 15)
		egg_test_failed (test, "percentage was not set: %i", percentage);
	else
		egg_test_success (test, NULL);

	/************************************************************/
	egg_test_title (test, "set back original brightness");
	ret = gcm_brightness_set_percentage (brightness, orig_percentage, &error);
	if (ret)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "failed to set brightness: %s", error->message);

	g_object_unref (brightness);

	egg_test_end (test);
}
#endif


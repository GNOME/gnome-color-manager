/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
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
 * SECTION:gcm-screen
 * @short_description: For querying data about PackageKit
 *
 * A GObject to use for accessing PackageKit asynchronously.
 */

#include "config.h"

#include <string.h>
#include <glib-object.h>
#include <gio/gio.h>

#include "egg-debug.h"

#include "gcm-screen.h"

static void     gcm_screen_finalize	(GObject     *object);

#define GCM_SCREEN_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GCM_TYPE_SCREEN, GcmScreenPrivate))

#define GCM_SCREEN_DBUS_METHOD_TIMEOUT		1500 /* ms */

/**
 * GcmScreenPrivate:
 *
 * Private #GcmScreen data
 **/
struct _GcmScreenPrivate
{
	GnomeRRScreen			*rr_screen;
};

enum {
	SIGNAL_OUTPUTS_CHANGED,
	SIGNAL_LAST
};

enum {
	PROP_0,
	PROP_LAST
};

static guint signals[SIGNAL_LAST] = { 0 };
static gpointer gcm_screen_object = NULL;

G_DEFINE_TYPE (GcmScreen, gcm_screen, G_TYPE_OBJECT)

/**
 * gcm_screen_randr_event_cb:
 **/
static void
gcm_screen_randr_event_cb (GnomeRRScreen *rr_screen, GcmScreen *screen)
{
	egg_debug ("emit outputs-changed");
	g_signal_emit (screen, signals[SIGNAL_OUTPUTS_CHANGED], 0);
}

/**
 * gcm_screen_ensure_instance:
 **/
static gboolean
gcm_screen_ensure_instance (GcmScreen *screen, GError **error)
{
	gboolean ret = TRUE;
	GcmScreenPrivate *priv = screen->priv;

	/* already got */
	if (priv->rr_screen != NULL)
		goto out;

	/* get screen (this is slow) */
	priv->rr_screen = gnome_rr_screen_new (gdk_screen_get_default (), (GnomeRRScreenChanged) gcm_screen_randr_event_cb, screen, error);
	if (priv->rr_screen == NULL) {
		ret = FALSE;
		goto out;
	}
out:
	return ret;
}

/**
 * gcm_screen_get_output_by_name:
 **/
GnomeRROutput *
gcm_screen_get_output_by_name (GcmScreen *screen, const gchar *output_name, GError **error)
{
	gboolean ret;
	GnomeRROutput *output = NULL;
	GcmScreenPrivate *priv = screen->priv;

	g_return_val_if_fail (GCM_IS_SCREEN (screen), NULL);
	g_return_val_if_fail (output_name != NULL, NULL);

	/* get instance */
	ret = gcm_screen_ensure_instance (screen, error);
	if (!ret)
		goto out;

	/* get output */
	output = gnome_rr_screen_get_output_by_name (priv->rr_screen, output_name);
	if (output == NULL) {
		g_set_error (error, 1, 0, "no output for name: %s", output_name);
		goto out;
	}
out:
	return output;
}

/**
 * gcm_screen_get_outputs:
 **/
GnomeRROutput **
gcm_screen_get_outputs (GcmScreen *screen, GError **error)
{
	gboolean ret;
	GnomeRROutput **outputs = NULL;
	GcmScreenPrivate *priv = screen->priv;

	g_return_val_if_fail (GCM_IS_SCREEN (screen), NULL);

	/* get instance */
	ret = gcm_screen_ensure_instance (screen, error);
	if (!ret)
		goto out;

	/* get output */
	outputs = gnome_rr_screen_list_outputs (priv->rr_screen);
	if (outputs == NULL) {
		g_set_error (error, 1, 0, "no outputs for screen");
		goto out;
	}
out:
	return outputs;
}

/**
 * gcm_screen_class_init:
 **/
static void
gcm_screen_class_init (GcmScreenClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gcm_screen_finalize;

	/**
	 * GcmScreen::outputs-changed:
	 **/
	signals[SIGNAL_OUTPUTS_CHANGED] =
		g_signal_new ("outputs-changed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GcmScreenClass, outputs_changed),
			      NULL, NULL, g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	g_type_class_add_private (klass, sizeof (GcmScreenPrivate));
}

/**
 * gcm_screen_init:
 **/
static void
gcm_screen_init (GcmScreen *screen)
{
	screen->priv = GCM_SCREEN_GET_PRIVATE (screen);
}

/**
 * gcm_screen_finalize:
 **/
static void
gcm_screen_finalize (GObject *object)
{
	GcmScreen *screen = GCM_SCREEN (object);
	GcmScreenPrivate *priv = screen->priv;

	if (priv->rr_screen != NULL)
		gnome_rr_screen_destroy (priv->rr_screen);

	G_OBJECT_CLASS (gcm_screen_parent_class)->finalize (object);
}

/**
 * gcm_screen_new:
 *
 * Return value: a new GcmScreen object.
 **/
GcmScreen *
gcm_screen_new (void)
{
	if (gcm_screen_object != NULL) {
		g_object_ref (gcm_screen_object);
	} else {
		gcm_screen_object = g_object_new (GCM_TYPE_SCREEN, NULL);
		g_object_add_weak_pointer (gcm_screen_object, &gcm_screen_object);
	}
	return GCM_SCREEN (gcm_screen_object);
}


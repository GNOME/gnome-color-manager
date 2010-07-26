/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
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
 *
 */

#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gnome-settings-daemon/gnome-settings-plugin.h>
#include <libcolor-glib.h>

#include "egg-console-kit.h"
#include "gsd-color-manager.h"

#define GSD_COLOR_MANAGER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GSD_TYPE_COLOR_MANAGER, GsdColorManagerPrivate))

struct GsdColorManagerPrivate
{
	guint			 timeout;
	GcmSensorClient		*sensor_client;
	EggConsoleKit		*console_kit;
};

static void gsd_color_manager_class_init	(GsdColorManagerClass *klass);
static void gsd_color_manager_init		(GsdColorManager *color_manager);
static void gsd_color_manager_finalize		(GObject *object);

G_DEFINE_TYPE (GsdColorManager, gsd_color_manager, G_TYPE_OBJECT)

static gpointer manager_object = NULL;

gboolean
gsd_color_manager_start (GsdColorManager *manager,
			 GError **error)
{
	g_debug ("Starting color manager");
	return TRUE;
}

void
gsd_color_manager_stop (GsdColorManager *manager)
{
	g_debug ("Stopping color manager");
}

/**
 * gsd_color_manager_sensor_client_changed_cb:
 **/
static void
gsd_color_manager_sensor_client_changed_cb (GcmSensorClient *sensor_client, GsdColorManager *manager)
{
	gboolean ret;
	GError *error = NULL;

	/* is sensor connected */
	ret = gcm_sensor_client_get_present (sensor_client);
	if (!ret)
		return;

	/* just spawn executable for now */
	g_debug ("opening gnome-control-center");
	ret = g_spawn_command_line_async (BINDIR "/gnome-control-center color", &error);
	if (!ret) {
		g_warning ("failed to run sensor plug action: %s", error->message);
		g_error_free (error);
	}
}

/**
 * gsd_color_manager_active_changed_cb:
 **/
static void
gsd_color_manager_active_changed_cb (GcmSensorClient *sensor_client, gboolean is_active, GsdColorManager *manager)
{
	gboolean ret;
	GError *error = NULL;

	/* we don't care */
	if (!is_active)
		return;

	/* just spawn executable for now */
	g_debug ("running gcm-apply");
	ret = g_spawn_command_line_async (BINDIR "/gcm-apply", &error);
	if (!ret) {
		g_warning ("failed to run active changed action: %s", error->message);
		g_error_free (error);
	}
}

static void
gsd_color_manager_dispose (GObject *object)
{
	GsdColorManager *manager;

	manager = GSD_COLOR_MANAGER (object);

	gsd_color_manager_stop (manager);

	G_OBJECT_CLASS (gsd_color_manager_parent_class)->dispose (object);
}

static void
gsd_color_manager_class_init (GsdColorManagerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->dispose = gsd_color_manager_dispose;
	object_class->finalize = gsd_color_manager_finalize;

	g_type_class_add_private (klass, sizeof (GsdColorManagerPrivate));
}

static void
gsd_color_manager_init (GsdColorManager *manager)
{
	manager->priv = GSD_COLOR_MANAGER_GET_PRIVATE (manager);
	manager->priv->sensor_client = gcm_sensor_client_new ();
	g_signal_connect (manager->priv->sensor_client, "changed",
			  G_CALLBACK (gsd_color_manager_sensor_client_changed_cb), manager);
	manager->priv->console_kit = egg_console_kit_new ();
	g_signal_connect (manager->priv->console_kit, "active-changed",
			  G_CALLBACK (gsd_color_manager_active_changed_cb), manager);
}

static void
gsd_color_manager_finalize (GObject *object)
{
	GsdColorManager *color_manager;

	g_return_if_fail (object != NULL);
	g_return_if_fail (GSD_IS_COLOR_MANAGER (object));

	color_manager = GSD_COLOR_MANAGER (object);

	g_object_unref (color_manager->priv->sensor_client);

	G_OBJECT_CLASS (gsd_color_manager_parent_class)->finalize (object);
}

GsdColorManager *
gsd_color_manager_new (void)
{
	if (manager_object) {
		g_object_ref (manager_object);
	} else {
		manager_object = g_object_new (GSD_TYPE_COLOR_MANAGER, NULL);
		g_object_add_weak_pointer (manager_object, (gpointer *) &manager_object);
	}

	return GSD_COLOR_MANAGER (manager_object);
}

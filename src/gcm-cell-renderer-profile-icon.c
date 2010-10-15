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

#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "egg-debug.h"

#include "gcm-cell-renderer-profile-icon.h"

enum {
	PROP_0,
	PROP_PROFILE,
	PROP_LAST
};

G_DEFINE_TYPE (GcmCellRendererProfileIcon, gcm_cell_renderer_profile_icon, GTK_TYPE_CELL_RENDERER_PIXBUF)

static gpointer parent_class = NULL;

static void
gcm_cell_renderer_profile_icon_get_property (GObject *object, guint param_id,
					     GValue *value, GParamSpec *pspec)
{
	GcmCellRendererProfileIcon *renderer = GCM_CELL_RENDERER_PROFILE_ICON (object);

	switch (param_id) {
	case PROP_PROFILE:
		g_value_set_object (value, renderer->profile);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
gcm_cell_renderer_set_icon_name (GcmCellRendererProfileIcon *renderer)
{
	const gchar *icon_name = NULL;

	if (gcm_profile_get_kind (renderer->profile) == GCM_PROFILE_KIND_DISPLAY_DEVICE &&
	    !gcm_profile_get_has_vcgt (renderer->profile))
		icon_name = "dialog-information";

	g_object_set (renderer, "icon-name", icon_name, NULL);
}

static void
gcm_cell_renderer_profile_icon_set_property (GObject *object, guint param_id,
					const GValue *value, GParamSpec *pspec)
{
	GcmCellRendererProfileIcon *renderer = GCM_CELL_RENDERER_PROFILE_ICON (object);

	switch (param_id) {
	case PROP_PROFILE:
		if (renderer->profile != NULL)
			g_object_unref (renderer->profile);
		renderer->profile = g_value_dup_object (value);
		gcm_cell_renderer_set_icon_name (renderer);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

/**
 * gcm_cell_renderer_finalize:
 * @object: The object to finalize
 **/
static void
gcm_cell_renderer_finalize (GObject *object)
{
	GcmCellRendererProfileIcon *renderer;
	renderer = GCM_CELL_RENDERER_PROFILE_ICON (object);
	g_free (renderer->icon_name);
	if (renderer->profile != NULL)
		g_object_unref (renderer->profile);
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gcm_cell_renderer_profile_icon_class_init (GcmCellRendererProfileIconClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);
	object_class->finalize = gcm_cell_renderer_finalize;

	parent_class = g_type_class_peek_parent (class);

	object_class->get_property = gcm_cell_renderer_profile_icon_get_property;
	object_class->set_property = gcm_cell_renderer_profile_icon_set_property;

	g_object_class_install_property (object_class, PROP_PROFILE,
					 g_param_spec_object ("profile", NULL,
					 NULL, GCM_TYPE_PROFILE, G_PARAM_READWRITE));
}

/**
 * gcm_cell_renderer_profile_icon_init:
 **/
static void
gcm_cell_renderer_profile_icon_init (GcmCellRendererProfileIcon *renderer)
{
	renderer->profile = NULL;
	renderer->icon_name = NULL;
}

/**
 * gcm_cell_renderer_profile_icon_new:
 **/
GtkCellRenderer *
gcm_cell_renderer_profile_icon_new (void)
{
	return g_object_new (GCM_TYPE_CELL_RENDERER_PROFILE_ICON, NULL);
}


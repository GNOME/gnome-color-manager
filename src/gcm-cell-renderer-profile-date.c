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

#include "gcm-cell-renderer-profile-date.h"
#include "gcm-utils.h"

enum {
	PROP_0,
	PROP_PROFILE,
	PROP_IS_DEFAULT,
	PROP_LAST
};

G_DEFINE_TYPE (GcmCellRendererProfileDate, gcm_cell_renderer_profile_date, GTK_TYPE_CELL_RENDERER_TEXT)

static gpointer parent_class = NULL;

static void
gcm_cell_renderer_profile_date_get_property (GObject *object, guint param_id,
				        GValue *value, GParamSpec *pspec)
{
	GcmCellRendererProfileDate *renderer = GCM_CELL_RENDERER_PROFILE_DATE (object);

	switch (param_id) {
	case PROP_PROFILE:
		g_value_set_object (value, renderer->profile);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static GString *
gcm_cell_renderer_get_profile_date (CdProfile *profile)
{
	gint now;
	gint64 age;
	gint64 created;
	GString *string;

	if (profile == NULL) {
		/* TRANSLATORS: this is when there is no profile for the device */
		string = g_string_new (_("No profile"));
		goto out;
	}

	/* get profile age */
	now = g_get_real_time () / G_USEC_PER_SEC;
	created = cd_profile_get_created (profile);
	if (created == 0) {
		string = g_string_new ("");
		goto out;
	}
	age = now - created;

	/* days */
	string = g_string_new ("");
	age /= 60 * 60 * 24;

	/* approximate years */
	if (age > 365) {
		age /= 365;
		g_string_append_printf (string, ngettext (
					"%i year",
					"%i years",
					age), (guint) age);
		goto out;
	}

	/* approximate months */
	if (age > 30) {
		age /= 30;
		g_string_append_printf (string, ngettext (
					"%i month",
					"%i months",
					age), (guint) age);
		goto out;
	}

	/* approximate weeks */
	if (age > 7) {
		age /= 7;
		g_string_append_printf (string, ngettext (
					"%i week",
					"%i weeks",
					age), (guint) age);
		goto out;
	}

	/* fallback */
	g_string_append_printf (string, _("Less than 1 week"));
out:
	if (string->len > 0) {
		g_string_prepend (string, "<span foreground='gray'>");
		g_string_append (string, "</span>");
	}
	return string;
}

static void
gcm_cell_renderer_set_markup (GcmCellRendererProfileDate *renderer)
{
	GString *string;

	/* do we have a profile to load? */
	string = gcm_cell_renderer_get_profile_date (renderer->profile);

	/* assign */
	g_free (renderer->markup);
	renderer->markup = g_string_free (string, FALSE);
	g_object_set (renderer, "markup", renderer->markup, NULL);
}

static void
gcm_cell_renderer_profile_date_set_property (GObject *object, guint param_id,
					     const GValue *value, GParamSpec *pspec)
{
	GcmCellRendererProfileDate *renderer = GCM_CELL_RENDERER_PROFILE_DATE (object);

	switch (param_id) {
	case PROP_PROFILE:
		if (renderer->profile != NULL)
			g_object_unref (renderer->profile);
		renderer->profile = g_value_dup_object (value);
		gcm_cell_renderer_set_markup (renderer);
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
	GcmCellRendererProfileDate *renderer;
	renderer = GCM_CELL_RENDERER_PROFILE_DATE (object);
	g_free (renderer->markup);
	if (renderer->profile != NULL)
		g_object_unref (renderer->profile);
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gcm_cell_renderer_profile_date_class_init (GcmCellRendererProfileDateClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);
	object_class->finalize = gcm_cell_renderer_finalize;

	parent_class = g_type_class_peek_parent (class);

	object_class->get_property = gcm_cell_renderer_profile_date_get_property;
	object_class->set_property = gcm_cell_renderer_profile_date_set_property;

	g_object_class_install_property (object_class, PROP_PROFILE,
					 g_param_spec_object ("profile", "PROFILE",
					 "PROFILE", CD_TYPE_PROFILE, G_PARAM_READWRITE));
}

/**
 * gcm_cell_renderer_profile_date_init:
 **/
static void
gcm_cell_renderer_profile_date_init (GcmCellRendererProfileDate *renderer)
{
	renderer->profile = NULL;
	renderer->markup = NULL;
}

/**
 * gcm_cell_renderer_profile_date_new:
 **/
GtkCellRenderer *
gcm_cell_renderer_profile_date_new (void)
{
	return g_object_new (GCM_TYPE_CELL_RENDERER_PROFILE_DATE, NULL);
}


/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "gcm-cell-renderer-profile-text.h"
#include "gcm-utils.h"

enum {
	PROP_0,
	PROP_PROFILE,
	PROP_IS_DEFAULT,
	PROP_LAST
};

G_DEFINE_TYPE (GcmCellRendererProfileText, gcm_cell_renderer_profile_text, GTK_TYPE_CELL_RENDERER_TEXT)

static gpointer parent_class = NULL;

static void
gcm_cell_renderer_profile_text_get_property (GObject *object, guint param_id,
				        GValue *value, GParamSpec *pspec)
{
	GcmCellRendererProfileText *renderer = GCM_CELL_RENDERER_PROFILE_TEXT (object);

	switch (param_id) {
	case PROP_PROFILE:
		g_value_set_object (value, renderer->profile);
		break;
	case PROP_IS_DEFAULT:
		g_value_set_boolean (value, renderer->is_default);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static GString *
gcm_cell_renderer_get_profile_text (CdProfile *profile)
{
	CdColorspace colorspace;
	const gchar *id;
	GString *string;
	g_autofree gchar *markup = NULL;

	if (profile == NULL) {
		/* TRANSLATORS: this is when there is no profile for the device */
		return g_string_new (_("No profile"));
	}

	/* add profile description */
	id = cd_profile_get_title (profile);
	if (id != NULL && id[0] != '\0') {
		markup = g_markup_escape_text (id, -1);
		string = g_string_new (markup);
		goto out;
	}

	/* some meta profiles do not have ICC profiles */
	colorspace = cd_profile_get_colorspace (profile);
	if (colorspace != CD_COLORSPACE_UNKNOWN) {
		string = g_string_new (NULL);
		g_string_append_printf (string,
					_("Default %s"),
					cd_colorspace_to_localised_string (colorspace));
		goto out;
	}

	/* fall back to ID, ick */
	string = g_string_new (cd_profile_get_id (profile));
out:

	/* any source prefix? */
	id = cd_profile_get_metadata_item (profile,
					   CD_PROFILE_METADATA_DATA_SOURCE);
	if (g_strcmp0 (id, CD_PROFILE_METADATA_DATA_SOURCE_EDID) == 0) {
		/* TRANSLATORS: this is a profile prefix to signify the
		 * profile has been auto-generated for this hardware */
		g_string_prepend (string, _("Default: "));
	}
	if (g_strcmp0 (id, CD_PROFILE_METADATA_DATA_SOURCE_STANDARD) == 0) {
		/* TRANSLATORS: this is a profile prefix to signify the
		 * profile his a standard space like AdobeRGB */
		g_string_prepend (string, _("Colorspace: "));
	}
	if (g_strcmp0 (id, CD_PROFILE_METADATA_DATA_SOURCE_TEST) == 0) {
		/* TRANSLATORS: this is a profile prefix to signify the
		 * profile is a test profile */
		g_string_prepend (string, _("Test profile: "));
	}
	return string;
}

static void
gcm_cell_renderer_set_markup (GcmCellRendererProfileText *renderer)
{
	GString *string;

	/* do we have a profile to load? */
	string = gcm_cell_renderer_get_profile_text (renderer->profile);

	/* this is the default profile */
	if (renderer->is_default) {
		g_string_prepend (string, "<b>");
		g_string_append (string, "</b>");
	}

	/* assign */
	g_free (renderer->markup);
	renderer->markup = g_string_free (string, FALSE);
	g_object_set (renderer, "markup", renderer->markup, NULL);
}

static void
gcm_cell_renderer_profile_text_set_property (GObject *object, guint param_id,
					const GValue *value, GParamSpec *pspec)
{
	GcmCellRendererProfileText *renderer = GCM_CELL_RENDERER_PROFILE_TEXT (object);

	switch (param_id) {
	case PROP_PROFILE:
		if (renderer->profile != NULL)
			g_object_unref (renderer->profile);
		renderer->profile = g_value_dup_object (value);
		gcm_cell_renderer_set_markup (renderer);
		break;
	case PROP_IS_DEFAULT:
		renderer->is_default = g_value_get_boolean (value);
		gcm_cell_renderer_set_markup (renderer);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
gcm_cell_renderer_finalize (GObject *object)
{
	GcmCellRendererProfileText *renderer;
	renderer = GCM_CELL_RENDERER_PROFILE_TEXT (object);
	g_free (renderer->markup);
	if (renderer->profile != NULL)
		g_object_unref (renderer->profile);
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gcm_cell_renderer_profile_text_class_init (GcmCellRendererProfileTextClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);
	object_class->finalize = gcm_cell_renderer_finalize;

	parent_class = g_type_class_peek_parent (class);

	object_class->get_property = gcm_cell_renderer_profile_text_get_property;
	object_class->set_property = gcm_cell_renderer_profile_text_set_property;

	g_object_class_install_property (object_class, PROP_PROFILE,
					 g_param_spec_object ("profile", "PROFILE",
					 "PROFILE", CD_TYPE_PROFILE, G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_IS_DEFAULT,
					 g_param_spec_boolean ("is-default", "IS_DEFAULT",
					 "IS_DEFAULT", FALSE, G_PARAM_READWRITE));
}

static void
gcm_cell_renderer_profile_text_init (GcmCellRendererProfileText *renderer)
{
	renderer->is_default = FALSE;
	renderer->profile = NULL;
	renderer->markup = NULL;
}

GtkCellRenderer *
gcm_cell_renderer_profile_text_new (void)
{
	return g_object_new (GCM_TYPE_CELL_RENDERER_PROFILE_TEXT, NULL);
}


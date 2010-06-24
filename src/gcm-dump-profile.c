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

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <locale.h>

#include "egg-debug.h"

#include "gcm-enum.h"
#include "gcm-profile.h"

/**
 * gcm_dump_profile_filename:
 **/
static gboolean
gcm_dump_profile_filename (const gchar *filename)
{
	gboolean ret;
	GError *error = NULL;
	GcmProfile *profile;
	guint profile_kind;
	guint colorspace;
	guint size;
	gboolean has_vcgt;
	const gchar *description;
	const gchar *copyright;
	const gchar *manufacturer;
	const gchar *model;
	const gchar *datetime;
	GFile *file = NULL;

	/* parse profile */
	profile = gcm_profile_new ();
	file = g_file_new_for_path (filename);
	ret = gcm_profile_parse (profile, file, &error);
	if (!ret) {
		egg_warning ("failed to parse: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* print what we know */
	profile_kind = gcm_profile_get_kind (profile);
	g_print ("Kind:\t%s\n", gcm_profile_kind_to_string (profile_kind));
	colorspace = gcm_profile_get_colorspace (profile);
	g_print ("Colorspace:\t%s\n", gcm_colorspace_to_string (colorspace));
	size = gcm_profile_get_size (profile);
	g_print ("Size:\t%i bytes\n", size);
	has_vcgt = gcm_profile_get_has_vcgt (profile);
	g_print ("Has VCGT:\t%s\n", has_vcgt ? "Yes" : "No");
	description = gcm_profile_get_description (profile);
	if (description != NULL)
		g_print ("Description:\t%s\n", description);
	copyright = gcm_profile_get_copyright (profile);
	if (copyright != NULL)
		g_print ("Copyright:\t%s\n", copyright);
	manufacturer = gcm_profile_get_manufacturer (profile);
	if (manufacturer != NULL)
		g_print ("Manufacturer:\t%s\n", manufacturer);
	model = gcm_profile_get_model (profile);
	if (model != NULL)
		g_print ("Model:\t%s\n", model);
	datetime = gcm_profile_get_datetime (profile);
	if (datetime != NULL)
		g_print ("Created:\t%s\n", datetime);
out:
	if (file != NULL)
		g_object_unref (file);
	g_object_unref (profile);
	return ret;
}

/**
 * main:
 **/
int
main (int argc, char **argv)
{
	guint i;
	guint retval = 0;
	GOptionContext *context;
	gchar **files = NULL;

	const GOptionEntry options[] = {
		{ G_OPTION_REMAINING, '\0', 0, G_OPTION_ARG_FILENAME_ARRAY, &files,
		  /* TRANSLATORS: command line option: a list of files to install */
		  _("Profiles to view"), NULL },
		{ NULL}
	};

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	gtk_init (&argc, &argv);

	/* TRANSLATORS: this just dumps the profile to the screen */
	context = g_option_context_new (_("ICC profile dump program"));
	g_option_context_add_main_entries (context, options, NULL);
	g_option_context_add_group (context, egg_debug_get_option_group ());
	g_option_context_add_group (context, gtk_get_option_group (TRUE));
	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);

	/* nothing specified */
	if (files == NULL)
		goto out;

	/* show each profile */
	for (i=0; files[i] != NULL; i++)
		gcm_dump_profile_filename (files[i]);
out:
	g_strfreev (files);
	return retval;
}


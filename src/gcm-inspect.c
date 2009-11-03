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
 * GNU General Public License for more profile.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "egg-debug.h"

#include "gcm-utils.h"
#include "gcm-profile.h"

/**
 * main:
 **/
int
main (int argc, char **argv)
{
	gboolean ret;
	gboolean verbose = FALSE;
	guint retval = 0;
	GError *error = NULL;
	GOptionContext *context;
	guint8 *data = NULL;
	gsize length;
	GcmProfile *profile = NULL;
	gchar *description = NULL;
	gchar *copyright = NULL;

	const GOptionEntry options[] = {
		{ "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose,
			"Show extra debugging information", NULL },
		{ NULL}
	};

	gtk_init (&argc, &argv);

	context = g_option_context_new ("gnome-color-manager inspect program");
	g_option_context_add_main_entries (context, options, NULL);
	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);

	egg_debug_init (verbose);

	/* get profile from XServer */
	ret = gcm_utils_get_x11_icc_profile_data (0, &data, &length, &error);
	if (!ret) {
		egg_warning ("failed to get XServer profile data: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* parse the data */
	profile = gcm_profile_new ();
	ret = gcm_profile_parse_data (profile, (const gchar *) data, length, &error);
	if (!ret) {
		egg_warning ("failed to parse data: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* print some interesting facts about the profile */
	g_object_get (profile,
		      "description", &description,
		      "copyright", &copyright,
		      NULL);

	/* TRANSLATORS: this is the ICC profile description stored in an atom in the XServer */
	g_print ("%s %s\n", _("Profile description:"), description);

	/* TRANSLATORS: this is the ICC profile copyright */
	g_print ("%s %s\n", _("Profile copyright:"), copyright);
out:
	g_free (data);
	g_free (copyright);
	g_free (description);
	if (profile != NULL)
		g_object_unref (profile);
	return retval;
}


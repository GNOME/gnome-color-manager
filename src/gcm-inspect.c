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
#include "gcm-xserver.h"

/**
 * gcm_inspect_print_data_info:
 **/
static gboolean
gcm_inspect_print_data_info (const gchar *title, const guint8 *data, gsize length)
{
	gchar *description = NULL;
	gchar *copyright = NULL;
	GcmProfile *profile = NULL;
	GError *error = NULL;
	gboolean ret;

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

	/* print title */
	g_print ("%s\n", title);

	/* TRANSLATORS: this is the ICC profile description stored in an atom in the XServer */
	g_print (" - %s %s\n", _("Description:"), description);

	/* TRANSLATORS: this is the ICC profile copyright */
	g_print (" - %s %s\n", _("Copyright:"), copyright);
out:
	g_free (copyright);
	g_free (description);
	if (profile != NULL)
		g_object_unref (profile);
	return ret;
}

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
	guint8 *data_tmp;
	gsize length;
	GcmXserver *xserver = NULL;
	GnomeRROutput **outputs;
	guint i;
	GnomeRRScreen *rr_screen = NULL;
	const gchar *output_name;
	gchar *title;

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

	/* setup object to access X */
	xserver = gcm_xserver_new ();

	/* get profile from XServer */
	ret = gcm_xserver_get_root_window_profile_data (xserver, &data, &length, &error);
	if (!ret) {
		egg_warning ("failed to get XServer profile data: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* TRANSLATORS: the root window of all the screens */
	gcm_inspect_print_data_info (_("Root window profile (deprecated):"), data, length);

	/* get screen */
	rr_screen = gnome_rr_screen_new (gdk_screen_get_default (), NULL, NULL, &error);
	if (rr_screen == NULL) {
		egg_warning ("failed to get rr screen: %s", error->message);
		goto out;
	}

	/* coldplug devices */
	outputs = gnome_rr_screen_list_outputs (rr_screen);
	for (i=0; outputs[i] != NULL; i++) {

		/* get output name */
		output_name = gnome_rr_output_get_name (outputs[i]);
		title = g_strdup_printf (_("Output profile '%s':"), output_name);

		/* get profile from XServer */
		ret = gcm_xserver_get_output_profile_data (xserver, output_name, &data_tmp, &length, &error);
		if (!ret) {
			egg_warning ("failed to get output profile data: %s", error->message);
			/* TRANSLATORS: this is when the profile has not been set */
			g_print ("%s %s\n", title, _("not set"));
			g_error_free (error);
			/* non-fatal */
			error = NULL;
		} else {
			/* TRANSLATORS: the output, i.e. the flat panel */
			gcm_inspect_print_data_info (title, data_tmp, length);
			g_free (data_tmp);
		}
		g_free (title);
	}

out:
	g_free (data);
	if (rr_screen != NULL)
		gnome_rr_screen_destroy (rr_screen);
	if (xserver != NULL)
		g_object_unref (xserver);
	return retval;
}


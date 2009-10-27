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
#include <math.h>

#include "egg-debug.h"

#include "gcm-utils.h"

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
	GnomeRROutput **outputs;
	guint i;
	GnomeRRScreen *rr_screen;
	gboolean connected;

	const GOptionEntry options[] = {
		{ "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose,
			"Show extra debugging information", NULL },
		{ NULL}
	};

	gtk_init (&argc, &argv);

	context = g_option_context_new ("gnome-color-manager apply program");
	g_option_context_add_main_entries (context, options, NULL);
	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);

	egg_debug_init (verbose);

	/* get screen */
        rr_screen = gnome_rr_screen_new (gdk_screen_get_default (), NULL, NULL, &error);
        if (rr_screen == NULL) {
		egg_warning ("failed to get rr screen: %s", error->message);
		goto out;
        }

	/* set for each output */
	outputs = gnome_rr_screen_list_outputs (rr_screen);
	for (i=0; outputs[i] != NULL; i++) {
		/* if nothing connected then ignore */
		connected = gnome_rr_output_is_connected (outputs[i]);
		if (!connected)
			continue;
		ret = gcm_utils_set_output_gamma (outputs[i], GCM_PROFILE_LOCATION, &error);
		if (!ret) {
			retval = 1;
			egg_warning ("failed to set gamma: %s", error->message);
			g_error_free (error);
			break;
		}
	}
out:
	if (rr_screen != NULL)
		gnome_rr_screen_destroy (rr_screen);
	return retval;
}


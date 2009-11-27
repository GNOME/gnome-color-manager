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

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libgnomeui/gnome-rr.h>
#include <locale.h>

#include "egg-debug.h"

#include "gcm-utils.h"

/**
 * main:
 **/
int
main (int argc, char **argv)
{
	const guint8 *data;
	gboolean ret;
	gchar *output_name;
	gchar *filename;
	guint i;
	guint retval = 0;
	GError *error = NULL;
	GnomeRROutput **outputs;
	GnomeRRScreen *rr_screen = NULL;
	GOptionContext *context;

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	gtk_init (&argc, &argv);

	context = g_option_context_new ("gnome-color-manager dump edid program");
	g_option_context_add_group (context, egg_debug_get_option_group ());
	g_option_context_add_group (context, gtk_get_option_group (TRUE));
	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);

	/* get screen */
	rr_screen = gnome_rr_screen_new (gdk_screen_get_default (), NULL, NULL, &error);
	if (rr_screen == NULL) {
		egg_warning ("failed to get rr screen: %s", error->message);
		retval = 1;
		goto out;
	}

	/* coldplug devices */
	outputs = gnome_rr_screen_list_outputs (rr_screen);
	for (i=0; outputs[i] != NULL; i++) {

		/* only try to get edid if connected */
		ret = gnome_rr_output_is_connected (outputs[i]);
		if (!ret)
			continue;

		/* get data */
		data = gnome_rr_output_get_edid_data (outputs[i]);
		if (data == NULL)
			continue;

		/* get output name */
		output_name = g_strdup (gnome_rr_output_get_name (outputs[i]));
		gcm_utils_alphanum_lcase (output_name);

		/* get suitable filename */
		filename = g_strdup_printf ("./%s.bin", output_name);

		/* save to disk */
		ret = g_file_set_contents (filename, (const gchar *) data, 0x80, &error);
		if (ret) {
			g_print ("Saved %i bytes to %s\n", 128, filename);
		} else {
			g_print ("Failed to save EDID to %s: %s\n", filename, error->message);
			/* non-fatal */
			g_clear_error (&error);
		}
		g_free (filename);
	}
out:
	if (rr_screen != NULL)
		gnome_rr_screen_destroy (rr_screen);
	return retval;
}


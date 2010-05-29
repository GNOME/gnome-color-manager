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
#include <libgnomeui/gnome-rr.h>
#include <locale.h>

#include "egg-debug.h"

#include "gcm-utils.h"
#include "gcm-screen.h"
#include "gcm-edid.h"

/**
 * gcm_dump_edid_filename:
 **/
static gboolean
gcm_dump_edid_filename (const gchar *filename)
{
	const gchar *monitor_name;
	const gchar *vendor_name;
	const gchar *serial_number;
	const gchar *eisa_id;
	const gchar *pnp_id;
	gchar *data = NULL;
	guint width;
	guint height;
	gfloat gamma;
	gboolean ret;
	GError *error = NULL;
	GcmEdid *edid = NULL;

	/* load */
	ret = g_file_get_contents (filename, &data, NULL, &error);
	if (!ret) {
		/* TRANSLATORS: this is when the EDID file cannot be read */
		g_print ("%s %s\n", _("Cannot load file contents:"), error->message);
		goto out;
	}

	/* parse */
	edid = gcm_edid_new ();
	ret = gcm_edid_parse (edid, (const guint8 *) data, &error);
	if (!ret) {
		/* TRANSLATORS: this is when the EDID cannot be parsed */
		g_print ("%s %s\n", _("Cannot parse EDID contents:"), error->message);
		goto out;
	}

	/* print data */
	monitor_name = gcm_edid_get_monitor_name (edid);
	if (monitor_name != NULL) {
		/* TRANSLATORS: this is debugging output for the supplied EDID file */
		g_print ("  %s %s\n", _("Monitor name:"), monitor_name);
	}
	vendor_name = gcm_edid_get_vendor_name (edid);
	if (vendor_name != NULL) {
		/* TRANSLATORS: this is debugging output for the supplied EDID file */
		g_print ("  %s %s\n", _("Vendor name:"), vendor_name);
	}
	serial_number = gcm_edid_get_serial_number (edid);
	if (serial_number != NULL) {
		/* TRANSLATORS: this is debugging output for the supplied EDID file */
		g_print ("  %s %s\n", _("Serial number:"), serial_number);
	}
	eisa_id = gcm_edid_get_eisa_id (edid);
	if (eisa_id != NULL) {
		/* TRANSLATORS: this is debugging output for the supplied EDID file */
		g_print ("  %s %s\n", _("EISA ID:"), eisa_id);
	}
	pnp_id = gcm_edid_get_pnp_id (edid);
	if (pnp_id != NULL) {
		/* TRANSLATORS: this is debugging output for the supplied EDID file */
		g_print ("  %s %s\n", _("PNP identifier:"), pnp_id);
	}
	width = gcm_edid_get_width (edid);
	height = gcm_edid_get_height (edid);
	if (width != 0) {
		/* TRANSLATORS: this is debugging output for the supplied EDID file */
		g_print ("  %s %ix%i\n", _("Size:"), width, height);
	}
	gamma = gcm_edid_get_gamma (edid);
	if (gamma > 0.0f) {
		/* TRANSLATORS: this is debugging output for the supplied EDID file */
		g_print ("  %s %f\n", _("Gamma:"), gamma);
	}
out:
	if (edid != NULL)
		g_object_unref (edid);
	g_free (data);
	return ret;
}

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
	gchar **files = NULL;
	guint i;
	guint retval = 0;
	GError *error = NULL;
	GnomeRROutput **outputs;
	GcmScreen *screen = NULL;
	GOptionContext *context;

	const GOptionEntry options[] = {
		{ G_OPTION_REMAINING, '\0', 0, G_OPTION_ARG_FILENAME_ARRAY, &files,
		  /* TRANSLATORS: command line option: a list of files to parse */
		  _("EDID dumps to parse"), NULL },
		{ NULL}
	};

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	gtk_init (&argc, &argv);

	context = g_option_context_new ("gnome-color-manager dump edid program");
	g_option_context_add_main_entries (context, options, NULL);
	g_option_context_add_group (context, egg_debug_get_option_group ());
	g_option_context_add_group (context, gtk_get_option_group (TRUE));
	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);

	/* we've specified files, just parse them */
	if (files != NULL) {
		for (i=0; files[i] != NULL; i++) {
			/* TRANSLATORS: this is the filename we are displaying */
			g_print ("%s %s\n", _("EDID dump:"), files[i]);
			gcm_dump_edid_filename (files[i]);
		}
		goto out;
	}

	/* coldplug devices */
	screen = gcm_screen_new ();
	outputs = gcm_screen_get_outputs (screen, &error);
	if (screen == NULL) {
		egg_warning ("failed to get outputs: %s", error->message);
		retval = 1;
		goto out;
	}
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
			/* TRANSLATORS: we saved the EDID to a file - second parameter is a filename */
			g_print (_("Saved %i bytes to %s"), 128, filename);
			g_print ("\n");
			gcm_dump_edid_filename (filename);
		} else {
			/* TRANSLATORS: we saved the EDID to a file - parameter is a filename */
			g_print (_("Failed to save EDID to %s"), filename);
			g_print (": %s\n", error->message);
			/* non-fatal */
			g_clear_error (&error);
		}
		g_free (filename);
	}
out:
	g_strfreev (files);
	if (screen != NULL)
		g_object_unref (screen);
	return retval;
}


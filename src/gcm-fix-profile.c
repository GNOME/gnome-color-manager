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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include <glib/gi18n.h>
#include <locale.h>
#include <lcms.h>

/**
 * gcm_fix_profile_filename:
 **/
static void
gcm_fix_profile_filename (const gchar *filename, const gchar *description)
{
	cmsHPROFILE lcms_profile;
	lcms_profile = cmsOpenProfileFromFile (filename, "rw");
	if (lcms_profile == NULL)
		return;
	if (description != NULL)
		_cmsAddTextTag (lcms_profile, icSigProfileDescriptionTag, description);
	_cmsSaveProfile (lcms_profile, filename);
	cmsCloseProfile (lcms_profile);
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
	gchar *description = NULL;

	const GOptionEntry options[] = {
		{ "desc", '\0', 0, G_OPTION_ARG_STRING, &description,
		  /* TRANSLATORS: command line option */
		  _("The description for the profile"), NULL },
		{ G_OPTION_REMAINING, '\0', 0, G_OPTION_ARG_FILENAME_ARRAY, &files,
		  /* TRANSLATORS: command line option: a list of icc files to fix */
		  _("Profiles to fix"), NULL },
		{ NULL}
	};

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	/* TRANSLATORS: this fixes broken profiles */
	context = g_option_context_new (_("ICC profile fix program"));
	g_option_context_add_main_entries (context, options, NULL);
	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);

	/* nothing specified */
	if (files == NULL)
		goto out;

	/* show each profile */
	for (i=0; files[i] != NULL; i++)
		gcm_fix_profile_filename (files[i], description);
out:
	g_free (description);
	g_strfreev (files);
	return retval;
}


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
#include <locale.h>
#include <lcms.h>

static gint lcms_error_code = 0;

/*
 * gcm_fix_profile_filename:
 */
static gboolean
gcm_fix_profile_filename (const gchar *filename, const gchar *description, const gchar *copyright, const gchar *model, const gchar *manufacturer)
{
	gboolean ret = TRUE;
	cmsHPROFILE lcms_profile = NULL;
	gchar *data = NULL;
	gsize len;
	GError *error = NULL;

	ret = g_file_get_contents (filename, &data, &len, &error);
	if (!ret) {
		g_warning ("failed to open profile: %s", error->message);
		g_error_free (error);
		goto out;
	}

	lcms_profile = cmsOpenProfileFromMem (data, len);
	if (lcms_profile == NULL || lcms_error_code != 0) {
		g_warning ("failed to open profile");
		ret = FALSE;
		goto out;
	}
	if (description != NULL) {
		ret = _cmsAddTextTag (lcms_profile, icSigProfileDescriptionTag, description);
		if (!ret || lcms_error_code != 0) {
			g_warning ("failed to write description");
			goto out;
		}
	}
	if (copyright != NULL) {
		ret = _cmsAddTextTag (lcms_profile, icSigCopyrightTag, copyright);
		if (!ret || lcms_error_code != 0) {
			g_warning ("failed to write copyright");
			goto out;
		}
	}
	if (model != NULL) {
		ret = _cmsAddTextTag (lcms_profile, icSigDeviceModelDescTag, model);
		if (!ret || lcms_error_code != 0) {
			g_warning ("failed to write model");
			goto out;
		}
	}
	if (manufacturer != NULL) {
		ret = _cmsAddTextTag (lcms_profile, icSigDeviceMfgDescTag, manufacturer);
		if (!ret || lcms_error_code != 0) {
			g_warning ("failed to write manufacturer");
			goto out;
		}
	}
	_cmsSaveProfile (lcms_profile, filename);
out:
	if (lcms_profile != NULL)
		cmsCloseProfile (lcms_profile);
	g_free (data);
	return ret;
}

/*
 * gcm_fix_profile_lcms_error_cb:
 */
static int
gcm_fix_profile_lcms_error_cb (int ErrorCode, const char *ErrorText)
{
	g_warning ("LCMS error %i: %s", ErrorCode, ErrorText);

	/* copy this sytemwide */
	lcms_error_code = ErrorCode;

	return LCMS_ERRC_WARNING;
}

/*
 * main:
 */
int
main (int argc, char **argv)
{
	guint i;
	guint retval = 0;
	gboolean ret;
	GOptionContext *context;
	gchar **files = NULL;
	gchar *description = NULL;
	gchar *copyright = NULL;
	gchar *model = NULL;
	gchar *manufacturer = NULL;

	const GOptionEntry options[] = {
		{ "description", 'd', 0, G_OPTION_ARG_STRING, &description,
		  /* TRANSLATORS: command line option */
		  _("The description of the profile"), NULL },
		{ "copyright", 'c', 0, G_OPTION_ARG_STRING, &copyright,
		  /* TRANSLATORS: command line option */
		  _("The copyright of the profile"), NULL },
		{ "model", 'm', 0, G_OPTION_ARG_STRING, &model,
		  /* TRANSLATORS: command line option */
		  _("The model of the profile"), NULL },
		{ "manufacturer", 'n', 0, G_OPTION_ARG_STRING, &manufacturer,
		  /* TRANSLATORS: command line option */
		  _("The manufacturer of the profile"), NULL },
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

	/* setup LCMS */
	cmsSetErrorHandler (gcm_fix_profile_lcms_error_cb);
	cmsErrorAction (LCMS_ERROR_SHOW);
	cmsSetLanguage ("en", "US");

	/* fix each profile */
	for (i=0; files[i] != NULL; i++) {
		ret = gcm_fix_profile_filename (files[i], description, copyright, model, manufacturer);
		if (!ret) {
			retval = 1;
			goto out;
		}
	}
out:
	g_free (description);
	g_free (copyright);
	g_free (model);
	g_free (manufacturer);
	g_strfreev (files);
	return retval;
}


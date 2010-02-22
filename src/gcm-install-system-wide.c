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
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include <sys/types.h>
#include <unistd.h>
#include <glib-object.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <locale.h>

#define GCM_INSTALL_SYSTEM_WIDE_EXIT_CODE_SUCCESS			0
#define GCM_INSTALL_SYSTEM_WIDE_EXIT_CODE_FAILED			1
#define GCM_INSTALL_SYSTEM_WIDE_EXIT_CODE_ARGUMENTS_INVALID		3
#define GCM_INSTALL_SYSTEM_WIDE_EXIT_CODE_CONTENT_TYPE_INVALID		4
#define GCM_INSTALL_SYSTEM_WIDE_EXIT_CODE_FAILED_TO_COPY		5

/**
 * main:
 **/
gint
main (gint argc, gchar *argv[])
{
	gchar **filenames = NULL;
	gchar *id = NULL;
	GOptionContext *context;
	gint uid;
	gint euid;
	guint retval = 0;
	gchar *dest = NULL;
	GFile *file = NULL;
	GFile *file_dest = NULL;
	GFileInfo *info = NULL;
	const gchar *type;
	GError *error = NULL;
	gboolean ret = FALSE;

	const GOptionEntry options[] = {
		{ "id", '\0', 0, G_OPTION_ARG_STRING, &id,
		   /* command line argument, the ID of the device */
		  _("Device ID, e.g. 'xrandr_ibm_france_ltn154p2_l05'"), NULL },
		{ G_OPTION_REMAINING, '\0', 0, G_OPTION_ARG_FILENAME_ARRAY, &filenames,
		  /* TRANSLATORS: command line option: a list of files to install */
		  _("ICC profile to install"), NULL },
		{ NULL}
	};

	/* setup translations */
	setlocale (LC_ALL, "");
	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	/* setup type system */
	g_type_init ();

	context = g_option_context_new (NULL);
	/* TRANSLATORS: tool that is used when copying profiles system-wide */
	g_option_context_set_summary (context, _("GNOME Color Manager ICC profile system-wide installer"));
	g_option_context_add_main_entries (context, options, NULL);
	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);

	/* no input */
	if (filenames == NULL || g_strv_length (filenames) != 1) {
		/* TRANSLATORS: user did not specify a valid filename */
		g_print ("%s\n", _("You need to specify exactly one ICC profile filename"));
		retval = GCM_INSTALL_SYSTEM_WIDE_EXIT_CODE_ARGUMENTS_INVALID;
		goto out;
	}

	/* no id */
	if (id == NULL) {
		/* TRANSLATORS: user did not specify a valid device ID */
		g_print ("%s\n", _("You need to specify exactly one device ID"));
		retval = GCM_INSTALL_SYSTEM_WIDE_EXIT_CODE_ARGUMENTS_INVALID;
		goto out;
	}

	/* get calling process */
	uid = getuid ();
	euid = geteuid ();
	if (uid != 0 || euid != 0) {
		/* TRANSLATORS: only able to install profiles as root */
		g_print ("%s\n", _("This program can only be used by the root user"));
		retval = GCM_INSTALL_SYSTEM_WIDE_EXIT_CODE_ARGUMENTS_INVALID;
		goto out;
	}

	/* get content type for file */
	file = g_file_new_for_path (filenames[0]);
	info = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE, G_FILE_QUERY_INFO_NONE, NULL, &error);
	if (info == NULL) {
		/* TRANSLATORS: error details */
		g_print ("%s: %s\n", _("Failed to get content type"), error->message);
		g_error_free (error);
		retval = GCM_INSTALL_SYSTEM_WIDE_EXIT_CODE_CONTENT_TYPE_INVALID;
		goto out;
	}

	/* check is correct type */
	type = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE);
	if (g_strcmp0 (type, "application/vnd.iccprofile") != 0) {
		/* TRANSLATORS: the content type is the detected type of file */
		g_print ("%s: %s\n", _("Content type was incorrect"), type);
		retval = GCM_INSTALL_SYSTEM_WIDE_EXIT_CODE_CONTENT_TYPE_INVALID;
		goto out;
	}

	/* get new location */
	dest = g_strdup_printf ("%s/%s.icc", GCM_SYSTEM_PROFILES_DIR, id);
	file_dest = g_file_new_for_path (dest);

	/* do the copy */
	ret = g_file_copy (file, file_dest, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, &error);
	if (!ret) {
		/* TRANSLATORS: error details */
		g_print ("%s: %s\n", _("Failed to copy"), error->message);
		g_error_free (error);
		retval = GCM_INSTALL_SYSTEM_WIDE_EXIT_CODE_FAILED_TO_COPY;
		goto out;
	}

	/* success */
	retval = GCM_INSTALL_SYSTEM_WIDE_EXIT_CODE_SUCCESS;
out:
	if (info != NULL)
		g_object_unref (info);
	if (file != NULL)
		g_object_unref (file);
	if (file_dest != NULL)
		g_object_unref (file_dest);
	g_strfreev (filenames);
	g_free (id);
	return retval;
}


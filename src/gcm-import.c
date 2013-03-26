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
#include <gdk/gdkx.h>
#include <math.h>
#include <locale.h>
#include <colord.h>

#include "gcm-utils.h"
#include "gcm-debug.h"

/**
 * gcm_import_show_details:
 **/
static gboolean
gcm_import_show_details (GtkWindow *window,
			 gboolean is_profile_id,
			 const gchar *data)
{
	gboolean ret;
	GError *error = NULL;
	GPtrArray *argv;
	guint xid;

	/* spawn new viewer async and modal to this dialog */
	argv = g_ptr_array_new_with_free_func (g_free);
	xid = gdk_x11_window_get_xid (gtk_widget_get_window (GTK_WIDGET(window)));
	g_ptr_array_add (argv, g_build_filename (BINDIR, "gcm-viewer", NULL));
	g_ptr_array_add (argv, g_strdup_printf ("--parent-window=%u", xid));
	if (is_profile_id)
		g_ptr_array_add (argv, g_strdup_printf ("--profile=%s", data));
	else
		g_ptr_array_add (argv, g_strdup_printf ("--file=%s", data));
	g_ptr_array_add (argv, NULL);
	ret = g_spawn_async (NULL,
			     (gchar **) argv->pdata,
			     NULL,
			     0,
			     NULL, NULL,
			     NULL,
			     &error);

	if (!ret) {
		g_warning ("failed to spawn viewer: %s", error->message);
		g_error_free (error);
	}
	g_ptr_array_unref (argv);
	return ret;
}

/**
 * main:
 **/
int
main (int argc, char **argv)
{
	CdClient *client = NULL;
	CdProfile *profile_tmp = NULL;
	const gchar *copyright;
	const gchar *description;
	const gchar *title;
	const gchar *lang;
	gboolean ret;
	gchar **files = NULL;
	CdIcc *icc = NULL;
	GError *error = NULL;
	GFile *destination = NULL;
	GFile *file = NULL;
	GOptionContext *context;
	GString *string = NULL;
	GtkResponseType response;
	GtkWidget *image = NULL;
	GtkWidget *dialog;
	guint retval = 1;

	const GOptionEntry options[] = {
		{ G_OPTION_REMAINING, '\0', 0, G_OPTION_ARG_FILENAME_ARRAY, &files,
		  /* TRANSLATORS: command line option: a list of catalogs to install */
		  _("ICC profile to install"), NULL },
		{ NULL}
	};

	setlocale (LC_ALL, "");

	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	gtk_init (&argc, &argv);

	context = g_option_context_new ("gnome-color-manager import program");
	g_option_context_add_main_entries (context, options, NULL);
	g_option_context_add_group (context, gcm_debug_get_option_group ());
	g_option_context_add_group (context, gtk_get_option_group (TRUE));
	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);

	/* nothing sent */
	if (files == NULL) {
		/* TRANSLATORS: nothing was specified on the command line */
		dialog = gtk_message_dialog_new (NULL,
						 0,
						 GTK_MESSAGE_ERROR,
						 GTK_BUTTONS_CLOSE,
						 _("No filename specified"));
		gtk_window_set_icon_name (GTK_WINDOW (dialog),
					  GCM_STOCK_ICON);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		goto out;
	}

	/* load profile */
	icc = cd_icc_new ();
	file = g_file_new_for_path (files[0]);
	ret = cd_icc_load_file (icc, file,
				CD_ICC_LOAD_FLAGS_FALLBACK_MD5,
				NULL, &error);
	if (!ret) {
		/* TRANSLATORS: could not read file */
		dialog = gtk_message_dialog_new (NULL,
						 0,
						 GTK_MESSAGE_ERROR,
						 GTK_BUTTONS_CLOSE,
						 _("Failed to open ICC profile"));
		gtk_window_set_icon_name (GTK_WINDOW (dialog),
					  GCM_STOCK_ICON);
		/* TRANSLATORS: parsing error */
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
							  _("Failed to parse file: %s"),
							  error->message);
		gtk_dialog_run (GTK_DIALOG (dialog));
		g_error_free (error);
		gtk_widget_destroy (dialog);
		goto out;
	}

	/* get data */
	lang = g_getenv ("LANG");
	description = cd_icc_get_description (icc, lang, NULL);
	copyright = cd_icc_get_copyright (icc, lang, NULL);

	/* use the same icon as the color control panel */
	image = gtk_image_new_from_icon_name ("preferences-color",
					      GTK_ICON_SIZE_DIALOG);
	gtk_misc_set_alignment (GTK_MISC (image), 0.5, 0.0);
	gtk_widget_show (image);

	/* create message */
	string = g_string_new ("");
	/* TRANSLATORS: message text */
	g_string_append_printf (string, _("Profile description: %s"),
				description != NULL ? description : files[0]);

	/* add copyright */
	if (copyright != NULL) {
		if (g_str_has_prefix (copyright, "Copyright "))
			copyright += 10;
		if (g_str_has_prefix (copyright, "Copyright, "))
			copyright += 11;
		/* TRANSLATORS: message text */
		g_string_append_printf (string, "\n%s %s", _("Profile copyright:"), copyright);
	}

	/* check file does't already exist as system-wide */
	client = cd_client_new ();
	ret = cd_client_connect_sync (client,
				      NULL,
				      &error);
	if (!ret) {
		g_warning ("failed to connect to colord: %s",
			   error->message);
		g_error_free (error);
		goto out;
	}

	profile_tmp = cd_client_find_profile_by_property_sync (client,
							      CD_PROFILE_METADATA_FILE_CHECKSUM,
							      cd_icc_get_checksum (icc),
							      NULL,
							      NULL);
	if (profile_tmp != NULL) {
		ret = cd_profile_connect_sync (profile_tmp,
					       NULL,
					       &error);
		if (!ret) {
			g_warning ("failed to connect to profile %s: %s",
				   cd_profile_get_object_path (profile_tmp),
				   error->message);
			g_error_free (error);
			goto out;
		}
		/* TRANSLATORS: color profile already been installed */
		dialog = gtk_message_dialog_new (NULL,
						 0,
						 GTK_MESSAGE_INFO,
						 GTK_BUTTONS_CLOSE,
						 _("Color profile is already imported"));
		gtk_window_set_icon_name (GTK_WINDOW (dialog), GCM_STOCK_ICON);
		gtk_message_dialog_set_image (GTK_MESSAGE_DIALOG (dialog), image);
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", string->str);
		gtk_dialog_add_button (GTK_DIALOG (dialog), _("Show Details"), GTK_RESPONSE_HELP);
		response = gtk_dialog_run (GTK_DIALOG (dialog));
		if (response == GTK_RESPONSE_HELP) {
			gcm_import_show_details (GTK_WINDOW (dialog),
						 TRUE,
						 cd_profile_get_id (profile_tmp));
			goto try_harder;
		}
		gtk_widget_destroy (dialog);
		goto out;
	}

	/* get correct title */
	switch (cd_icc_get_kind (icc)) {
	case CD_PROFILE_KIND_DISPLAY_DEVICE:
		/* TRANSLATORS: the profile type */
		title = _("Import display color profile?");
		break;
	case CD_PROFILE_KIND_OUTPUT_DEVICE:
		/* TRANSLATORS: the profile type */
		title = _("Import device color profile?");
		break;
	case CD_PROFILE_KIND_NAMED_COLOR:
		/* TRANSLATORS: the profile type */
		title = _("Import named color profile?");
		break;
	default:
		/* TRANSLATORS: the profile type */
		title = _("Import color profile?");
		break;
	}

	/* ask confirmation */
	dialog = gtk_message_dialog_new (NULL,
					 0,
					 GTK_MESSAGE_INFO,
					 GTK_BUTTONS_CANCEL,
					 "%s",
					 title);
	gtk_message_dialog_set_image (GTK_MESSAGE_DIALOG (dialog), image);
	gtk_window_set_icon_name (GTK_WINDOW (dialog), GCM_STOCK_ICON);
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", string->str);
	/* TRANSLATORS: button text */
	gtk_dialog_add_button (GTK_DIALOG (dialog), _("Import"), GTK_RESPONSE_OK);
	gtk_dialog_add_button (GTK_DIALOG (dialog), _("Show Details"), GTK_RESPONSE_HELP);
try_harder:
	response = gtk_dialog_run (GTK_DIALOG (dialog));
	if (response == GTK_RESPONSE_HELP) {
		gcm_import_show_details (GTK_WINDOW (dialog), FALSE, files[0]);
		goto try_harder;
	}
	gtk_widget_destroy (dialog);

	/* not the correct response */
	if (response != GTK_RESPONSE_OK)
		goto out;

	/* copy icc file to users profile path */
	profile_tmp = cd_client_import_profile_sync (client,
						     file,
						     NULL,
						     &error);
	if (profile_tmp == NULL) {
		/* TRANSLATORS: could not read file */
		dialog = gtk_message_dialog_new (NULL,
						 0,
						 GTK_MESSAGE_ERROR,
						 GTK_BUTTONS_CLOSE,
						 _("Failed to import file"));
		gtk_window_set_icon_name (GTK_WINDOW (dialog), GCM_STOCK_ICON);
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", error->message);
		gtk_dialog_run (GTK_DIALOG (dialog));
		g_error_free (error);
		gtk_widget_destroy (dialog);
		goto out;
	}
out:
	if (file != NULL)
		g_object_unref (file);
	if (string != NULL)
		g_string_free (string, TRUE);
	if (icc != NULL)
		g_object_unref (icc);
	if (client != NULL)
		g_object_unref (client);
	if (profile_tmp != NULL)
		g_object_unref (profile_tmp);
	if (destination != NULL)
		g_object_unref (destination);
	g_strfreev (files);
	return retval;
}


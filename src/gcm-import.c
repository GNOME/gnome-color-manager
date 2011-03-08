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
#include <math.h>
#include <locale.h>
#include <colord.h>

#include "gcm-profile.h"
#include "gcm-utils.h"
#include "gcm-color.h"
#include "gcm-cie-widget.h"
#include "gcm-debug.h"

/**
 * gcm_import_add_cie_widget:
 **/
static void
gcm_import_add_cie_widget (GtkDialog *dialog, GtkWidget *cie_widget)
{
	GtkWidget *aspect;
	GtkWidget *vbox;

	vbox = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	aspect = gtk_aspect_frame_new (NULL, 0.0f, 0.0f, 1.0f, FALSE);
	gtk_frame_set_shadow_type (GTK_FRAME(aspect), GTK_SHADOW_NONE);
	gtk_container_add (GTK_CONTAINER(aspect), cie_widget);
	gtk_box_pack_end (GTK_BOX(vbox), aspect, TRUE, TRUE, 12);
	gtk_widget_show (cie_widget);
	gtk_widget_show (aspect);
}

/**
 * main:
 **/
int
main (int argc, char **argv)
{
	CdClient *client = NULL;
	CdColorspace colorspace;
	CdProfile *profile_tmp = NULL;
	const gchar *copyright;
	const gchar *description;
	gboolean ret;
	gchar **files = NULL;
	GcmProfile *profile = NULL;
	GError *error = NULL;
	GFile *destination = NULL;
	GFile *file = NULL;
	GOptionContext *context;
	GString *string = NULL;
	GtkResponseType response;
	GtkWidget *cie_widget = NULL;
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
		dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, _("No filename specified"));
		gtk_window_set_icon_name (GTK_WINDOW (dialog), GCM_STOCK_ICON);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		goto out;
	}

	/* load profile */
	profile = gcm_profile_new ();
	file = g_file_new_for_path (files[0]);
	ret = gcm_profile_parse (profile, file, &error);
	if (!ret) {
		/* TRANSLATORS: could not read file */
		dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, _("Failed to open ICC profile"));
		gtk_window_set_icon_name (GTK_WINDOW (dialog), GCM_STOCK_ICON);
		/* TRANSLATORS: parsing error */
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), _("Failed to parse file: %s"), error->message);
		gtk_dialog_run (GTK_DIALOG (dialog));
		g_error_free (error);
		gtk_widget_destroy (dialog);
		goto out;
	}

	/* get data */
	description = gcm_profile_get_description (profile);
	copyright = gcm_profile_get_copyright (profile);
	colorspace = gcm_profile_get_colorspace (profile);

	/* use CIE widget */
	cie_widget = gcm_cie_widget_new ();
	g_object_set (cie_widget,
		      "use-grid", FALSE,
		      "use-whitepoint", FALSE,
		      NULL);
	gtk_widget_set_size_request (cie_widget, 200, 200);
	gcm_cie_widget_set_from_profile (cie_widget, profile);

	/* check file does't already exist as a file */
	destination = gcm_utils_get_profile_destination (file);
	ret = g_file_query_exists (destination, NULL);
	if (ret) {
		/* TRANSLATORS: color profile already been installed */
		dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE, _("ICC profile already installed"));
		gtk_window_set_icon_name (GTK_WINDOW (dialog), GCM_STOCK_ICON);
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s\n%s", description, copyright);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		goto out;
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
	profile_tmp = cd_client_find_profile_sync (client,
						   gcm_profile_get_checksum (profile),
						   NULL,
						   NULL);
	if (profile_tmp != NULL) {
		/* TRANSLATORS: color profile already been installed */
		dialog = gtk_message_dialog_new (NULL,
						 GTK_DIALOG_MODAL,
						 GTK_MESSAGE_INFO,
						 GTK_BUTTONS_CLOSE,
						 _("ICC profile already installed system-wide"));
		gtk_window_set_icon_name (GTK_WINDOW (dialog), GCM_STOCK_ICON);
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
							  "%s\n%s",
							  description,
							  copyright);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		goto out;
	}

	/* create message */
	string = g_string_new ("");
	if (description != NULL) {
		/* TRANSLATORS: message text */
		g_string_append_printf (string, _("Import ICC color profile %s?"), description);
	} else {
		/* TRANSLATORS: message text */
		g_string_append (string, _("Import ICC color profile?"));
	}

	/* add copyright */
	if (copyright != NULL)
		g_string_append_printf (string, "\n%s", copyright);

	/* ask confirmation */
	dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_CLOSE, "%s", _("Import ICC profile"));
	gtk_window_set_icon_name (GTK_WINDOW (dialog), GCM_STOCK_ICON);
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", string->str);
	/* TRANSLATORS: button text */
	gtk_dialog_add_button (GTK_DIALOG (dialog), _("Install"), GTK_RESPONSE_OK);

	/* add cie widget */
	gcm_import_add_cie_widget (GTK_DIALOG(dialog), cie_widget);

	/* only show the cie widget if we have RGB data */
	if (colorspace != CD_COLORSPACE_RGB)
		gtk_widget_hide (cie_widget);

	gtk_window_set_resizable (GTK_WINDOW (dialog), TRUE);
	response = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	/* not the correct response */
	if (response != GTK_RESPONSE_OK)
		goto out;

	/* copy icc file to users profile path */
	ret = gcm_utils_mkdir_and_copy (file, destination, &error);
	if (!ret) {
		/* TRANSLATORS: could not read file */
		dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, _("Failed to copy file"));
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
	if (profile != NULL)
		g_object_unref (profile);
	if (client != NULL)
		g_object_unref (client);
	if (profile_tmp != NULL)
		g_object_unref (profile_tmp);
	if (destination != NULL)
		g_object_unref (destination);
	g_strfreev (files);
	return retval;
}


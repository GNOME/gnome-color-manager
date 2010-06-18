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

#include "gcm-utils.h"

#include "egg-debug.h"

#define PK_DBUS_SERVICE					"org.freedesktop.PackageKit"
#define PK_DBUS_PATH					"/org/freedesktop/PackageKit"
#define PK_DBUS_INTERFACE_QUERY				"org.freedesktop.PackageKit.Query"
#define PK_DBUS_INTERFACE_MODIFY			"org.freedesktop.PackageKit.Modify"

/**
 * gcm_utils_linkify:
 **/
gchar *
gcm_utils_linkify (const gchar *text)
{
	guint i;
	guint j = 0;
	gboolean ret;
	GString *string;

	/* find and replace links */
	string = g_string_new ("");
	for (i=0;; i++) {

		/* find the start of a link */
		ret = g_str_has_prefix (&text[i], "http://");
		if (ret) {
			g_string_append_len (string, text+j, i-j);
			for (j=i;; j++) {
				/* find the end of the link, or the end of the string */
				if (text[j] == '\0' ||
				    text[j] == ' ') {
					g_string_append (string, "<a href=\"");
					g_string_append_len (string, text+i, j-i);
					g_string_append (string, "\">");
					g_string_append_len (string, text+i, j-i);
					g_string_append (string, "</a>");
					break;
				}
			}
		}

		/* end of the string, dump what's left */
		if (text[i] == '\0') {
			g_string_append_len (string, text+j, i-j);
			break;
		}
	}
	return g_string_free (string, FALSE);
}

/**
 * gcm_utils_is_icc_profile:
 **/
gboolean
gcm_utils_is_icc_profile (GFile *file)
{
	GFileInfo *info;
	const gchar *type;
	GError *error = NULL;
	gboolean ret = FALSE;
	gchar *filename = NULL;

	/* get content type for file */
	filename = g_file_get_uri (file);
	if (filename == NULL)
		filename = g_file_get_path (file);
	info = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE, G_FILE_QUERY_INFO_NONE, NULL, &error);
	if (info != NULL) {
		type = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE);
		if (g_strcmp0 (type, "application/vnd.iccprofile") == 0) {
			ret = TRUE;
			goto out;
		}
	} else {
		egg_warning ("failed to get content type of %s: %s", filename, error->message);
		g_error_free (error);
	}

	/* fall back if we have not got a new enought s-m-i */
	if (g_str_has_suffix (filename, ".icc")) {
		ret = TRUE;
		goto out;
	}
	if (g_str_has_suffix (filename, ".icm")) {
		ret = TRUE;
		goto out;
	}
	if (g_str_has_suffix (filename, ".ICC")) {
		ret = TRUE;
		goto out;
	}
	if (g_str_has_suffix (filename, ".ICM")) {
		ret = TRUE;
		goto out;
	}
out:
	if (info != NULL)
		g_object_unref (info);
	g_free (filename);
	return ret;
}

/**
 * gcm_utils_install_package:
 **/
gboolean
gcm_utils_install_package (const gchar *package_name, GtkWindow *window)
{
	GDBusConnection *connection;
	GVariant *args = NULL;
	GVariant *response = NULL;
	GVariantBuilder *builder = NULL;
	GError *error = NULL;
	gboolean ret = FALSE;
	guint32 xid = 0;
	gchar **packages = NULL;

	g_return_val_if_fail (package_name != NULL, FALSE);

#ifndef HAVE_PACKAGEKIT
	egg_warning ("cannot install %s: this package was not compiled with --enable-packagekit", package_name);
	goto out;
#endif

	/* get xid of this window */
	if (window != NULL)
		xid = gdk_x11_drawable_get_xid (gtk_widget_get_window (GTK_WIDGET(window)));

	/* we're expecting an array of packages */
	packages = g_strsplit (package_name, "|", 1);

	/* get a session bus connection */
	connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
	if (connection == NULL) {
		/* TRANSLATORS: no DBus session bus */
		g_print ("%s %s\n", _("Failed to connect to session bus:"), error->message);
		g_error_free (error);
		goto out;
	}

	/* create arguments */
	builder = g_variant_builder_new (G_VARIANT_TYPE ("(uass)"));
	g_variant_builder_add_value (builder, g_variant_new_uint32 (xid));
	g_variant_builder_add_value (builder, g_variant_new_strv ((const gchar * const *)packages, -1));
	g_variant_builder_add_value (builder, g_variant_new_string ("hide-confirm-search,hide-finished"));
	args = g_variant_builder_end (builder);

	/* execute sync method */
	response = g_dbus_connection_call_sync (connection,
						PK_DBUS_SERVICE,
						PK_DBUS_PATH,
						PK_DBUS_INTERFACE_MODIFY,
						"InstallPackageNames",
						args,
						G_VARIANT_TYPE ("(b)"),
						G_DBUS_CALL_FLAGS_NONE,
						G_MAXINT, NULL, &error);
	if (response == NULL) {
		/* TRANSLATORS: the DBus method failed */
		egg_warning ("%s %s\n", _("The request failed:"), error->message);
		g_error_free (error);
		goto out;
	}

	/* success */
	ret = TRUE;
out:
	if (builder != NULL)
		g_variant_builder_unref (builder);
	if (args != NULL)
		g_variant_unref (args);
	if (response != NULL)
		g_variant_unref (response);
	g_strfreev (packages);
	return ret;
}

/**
 * gcm_utils_is_package_installed:
 **/
gboolean
gcm_utils_is_package_installed (const gchar *package_name)
{
	GDBusConnection *connection;
	GVariant *args = NULL;
	GVariant *response = NULL;
	GError *error = NULL;
	gboolean installed = TRUE;

	g_return_val_if_fail (package_name != NULL, FALSE);

#ifndef HAVE_PACKAGEKIT
	egg_warning ("cannot query %s: this package was not compiled with --enable-packagekit", package_name);
	return TRUE;
#endif

	/* get a session bus connection */
	connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
	if (connection == NULL) {
		/* TRANSLATORS: no DBus session bus */
		g_print ("%s %s\n", _("Failed to connect to session bus:"), error->message);
		g_error_free (error);
		goto out;
	}

	/* execute sync method */
	args = g_variant_new ("(ss)", package_name, "timeout=5");
	response = g_dbus_connection_call_sync (connection,
						PK_DBUS_SERVICE,
						PK_DBUS_PATH,
						PK_DBUS_INTERFACE_QUERY,
						"IsInstalled",
						args,
						G_VARIANT_TYPE ("(b)"),
						G_DBUS_CALL_FLAGS_NONE,
						G_MAXINT, NULL, &error);
	if (response == NULL) {
		/* TRANSLATORS: the DBus method failed */
		egg_warning ("%s %s\n", _("The request failed:"), error->message);
		g_error_free (error);
		goto out;
	}

	/* get value */
	g_variant_get (response, "(b)", &installed);
out:
	if (args != NULL)
		g_variant_unref (args);
	if (response != NULL)
		g_variant_unref (response);
	return installed;
}

/**
 * gcm_utils_output_is_lcd_internal:
 * @output_name: the output name
 *
 * Return value: %TRUE is the output is an internal LCD panel
 **/
gboolean
gcm_utils_output_is_lcd_internal (const gchar *output_name)
{
	g_return_val_if_fail (output_name != NULL, FALSE);
	if (g_strstr_len (output_name, -1, "LVDS") != NULL)
		return TRUE;
	if (g_strstr_len (output_name, -1, "lvds") != NULL)
		return TRUE;
	return FALSE;
}

/**
 * gcm_utils_output_is_lcd:
 * @output_name: the output name
 *
 * Return value: %TRUE is the output is an internal or external LCD panel
 **/
gboolean
gcm_utils_output_is_lcd (const gchar *output_name)
{
	g_return_val_if_fail (output_name != NULL, FALSE);
	if (gcm_utils_output_is_lcd_internal (output_name))
		return TRUE;
	if (g_strstr_len (output_name, -1, "DVI") != NULL)
		return TRUE;
	if (g_strstr_len (output_name, -1, "dvi") != NULL)
		return TRUE;
	return FALSE;
}

/**
 * gcm_utils_ensure_printable:
 **/
void
gcm_utils_ensure_printable (gchar *text)
{
	guint i;
	guint idx = 0;

	g_return_if_fail (text != NULL);

	for (i=0; text[i] != '\0'; i++) {
		if (g_ascii_isalnum (text[i]) ||
		    g_ascii_ispunct (text[i]) ||
		    text[i] == ' ')
			text[idx++] = text[i];
	}
	text[idx] = '\0';

	/* broken profiles have _ instead of spaces */
	g_strdelimit (text, "_", ' ');
}

/**
 * gcm_utils_mkdir_with_parents:
 **/
gboolean
gcm_utils_mkdir_with_parents (const gchar *filename, GError **error)
{
	gboolean ret;
	GFile *file = NULL;

	/* ensure desination exists */
	ret = g_file_test (filename, G_FILE_TEST_EXISTS);
	if (!ret) {
		file = g_file_new_for_path (filename);
		ret = g_file_make_directory_with_parents (file, NULL, error);
		if (!ret)
			goto out;
	}
out:
	if (file != NULL)
		g_object_unref (file);
	return ret;
}

/**
 * gcm_utils_mkdir_for_filename:
 **/
gboolean
gcm_utils_mkdir_for_filename (const gchar *filename, GError **error)
{
	gboolean ret = FALSE;
	GFile *file;
	GFile *parent_dir = NULL;

	/* get a file from the URI / path */
	file = g_file_new_for_path (filename);
	if (file == NULL)
		file = g_file_new_for_uri (filename);
	if (file == NULL) {
		g_set_error (error, 1, 0, "could not resolve file for %s", filename);
		goto out;
	}

	/* get parent */
	parent_dir = g_file_get_parent (file);
	if (parent_dir == NULL) {
		g_set_error (error, 1, 0, "could not get parent dir %s", filename);
		goto out;
	}

	/* ensure desination exists */
	ret = g_file_query_exists (parent_dir, NULL);
	if (!ret) {
		ret = g_file_make_directory_with_parents (parent_dir, NULL, error);
		if (!ret)
			goto out;
	}
out:
	if (file != NULL)
		g_object_unref (file);
	if (parent_dir != NULL)
		g_object_unref (parent_dir);
	return ret;
}

/**
 * gcm_utils_mkdir_and_copy:
 **/
gboolean
gcm_utils_mkdir_and_copy (GFile *source, GFile *destination, GError **error)
{
	gboolean ret;
	GFile *parent;

	g_return_val_if_fail (source != NULL, FALSE);
	g_return_val_if_fail (destination != NULL, FALSE);

	/* get parent */
	parent = g_file_get_parent (destination);

	/* create directory */
	if (!g_file_query_exists (parent, NULL)) {
		ret = g_file_make_directory_with_parents (parent, NULL, error);
		if (!ret)
			goto out;
	}

	/* do the copy */
	ret = g_file_copy (source, destination, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, error);
	if (!ret)
		goto out;
out:
	g_object_unref (parent);
	return ret;
}

/**
 * gcm_utils_get_profile_destination:
 **/
GFile *
gcm_utils_get_profile_destination (GFile *file)
{
	gchar *basename;
	gchar *destination;
	GFile *dest;

	g_return_val_if_fail (file != NULL, NULL);

	/* get destination filename for this source file */
	basename = g_file_get_basename (file);
	destination = g_build_filename (g_get_user_data_dir (), "icc", basename, NULL);
	dest = g_file_new_for_path (destination);

	g_free (basename);
	g_free (destination);
	return dest;
}

/**
 * gcm_utils_ptr_array_to_strv:
 * @array: the GPtrArray of strings
 *
 * Form a composite string array of strings.
 * The data in the GPtrArray is copied.
 *
 * Return value: the string array, or %NULL if invalid
 **/
gchar **
gcm_utils_ptr_array_to_strv (GPtrArray *array)
{
	gchar **value;
	const gchar *value_temp;
	guint i;

	g_return_val_if_fail (array != NULL, NULL);

	/* copy the array to a strv */
	value = g_new0 (gchar *, array->len + 1);
	for (i=0; i<array->len; i++) {
		value_temp = (const gchar *) g_ptr_array_index (array, i);
		value[i] = g_strdup (value_temp);
	}

	return value;
}

/**
 * gcm_gnome_help:
 * @link_id: Subsection of help file, or %NULL.
 **/
gboolean
gcm_gnome_help (const gchar *link_id)
{
	GError *error = NULL;
	gchar *uri;
	gboolean ret = TRUE;

	if (link_id)
		uri = g_strconcat ("ghelp:gnome-color-manager?", link_id, NULL);
	else
		uri = g_strdup ("ghelp:gnome-color-manager");
	egg_debug ("opening uri %s", uri);

	gtk_show_uri (NULL, uri, GDK_CURRENT_TIME, &error);

	if (error != NULL) {
		GtkWidget *d;
		d = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					    GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "%s", error->message);
		gtk_dialog_run (GTK_DIALOG(d));
		gtk_widget_destroy (d);
		g_error_free (error);
		ret = FALSE;
	}

	g_free (uri);
	return ret;
}

/**
 * gcm_utils_alphanum_lcase:
 **/
void
gcm_utils_alphanum_lcase (gchar *data)
{
	guint i;

	g_return_if_fail (data != NULL);

	/* replace unsafe chars, and make lowercase */
	for (i=0; data[i] != '\0'; i++) {
		if (!g_ascii_isalnum (data[i]))
			data[i] = '_';
		data[i] = g_ascii_tolower (data[i]);
	}
}

/**
 * gcm_utils_ensure_sensible_filename:
 **/
void
gcm_utils_ensure_sensible_filename (gchar *data)
{
	guint i;

	g_return_if_fail (data != NULL);

	/* replace unsafe chars, and make lowercase */
	for (i=0; data[i] != '\0'; i++) {
		if (data[i] != ' ' &&
		    data[i] != '-' &&
		    data[i] != '(' &&
		    data[i] != ')' &&
		    data[i] != '[' &&
		    data[i] != ']' &&
		    data[i] != ',' &&
		    !g_ascii_isalnum (data[i]))
			data[i] = '_';
	}
}

/**
 * gcm_utils_get_default_config_location:
 **/
gchar *
gcm_utils_get_default_config_location (void)
{
	gchar *filename;

	if (g_getenv ("GCM_TEST") != NULL) {
		filename = g_strdup ("/tmp/device-profiles.conf");
		goto out;
	}

	/* create default path */
	filename = g_build_filename (g_get_user_config_dir (), "color", "device-profiles.conf", NULL);
out:
	return filename;
}

/**
 * gcm_utils_device_kind_to_profile_kind:
 **/
GcmProfileKind
gcm_utils_device_kind_to_profile_kind (GcmDeviceKind kind)
{
	GcmProfileKind profile_kind;
	switch (kind) {
	case GCM_DEVICE_KIND_DISPLAY:
		profile_kind = GCM_PROFILE_KIND_DISPLAY_DEVICE;
		break;
	case GCM_DEVICE_KIND_CAMERA:
	case GCM_DEVICE_KIND_SCANNER:
		profile_kind = GCM_PROFILE_KIND_INPUT_DEVICE;
		break;
	case GCM_DEVICE_KIND_PRINTER:
		profile_kind = GCM_PROFILE_KIND_OUTPUT_DEVICE;
		break;
	default:
		egg_warning ("unknown kind: %i", kind);
		profile_kind = GCM_PROFILE_KIND_UNKNOWN;
	}
	return profile_kind;
}

/**
 * gcm_utils_format_date_time:
 **/
gchar *
gcm_utils_format_date_time (const struct tm *created)
{
	gchar buffer[256];

	/* TRANSLATORS: this is the profile creation date strftime format */
	strftime (buffer, sizeof(buffer), _("%B %e %Y, %I:%M:%S %p"), created);

	return g_strdup (g_strchug (buffer));
}

/**
 * gcm_intent_to_localized_text:
 **/
const gchar *
gcm_intent_to_localized_text (GcmIntent intent)
{
	if (intent == GCM_INTENT_PERCEPTUAL) {
		/* TRANSLATORS: rendering intent: you probably want to google this */
		return _("Perceptual");
	}
	if (intent == GCM_INTENT_RELATIVE_COLORMETRIC) {
		/* TRANSLATORS: rendering intent: you probably want to google this */
		return _("Relative");
	}
	if (intent == GCM_INTENT_SATURATION) {
		/* TRANSLATORS: rendering intent: you probably want to google this */
		return _("Saturation");
	}
	if (intent == GCM_INTENT_ABSOLUTE_COLORMETRIC) {
		/* TRANSLATORS: rendering intent: you probably want to google this */
		return _("Absolute");
	}
	return "unknown";
}

/**
 * gcm_intent_to_localized_description:
 **/
const gchar *
gcm_intent_to_localized_description (GcmIntent intent)
{
	if (intent == GCM_INTENT_PERCEPTUAL) {
		/* TRANSLATORS: rendering intent: you probably want to google this */
		return _("High quality photography");
	}
	if (intent == GCM_INTENT_RELATIVE_COLORMETRIC) {
		/* TRANSLATORS: rendering intent: you probably want to google this */
		return _("Precise color matching");
	}
	if (intent == GCM_INTENT_SATURATION) {
		/* TRANSLATORS: rendering intent: you probably want to google this */
		return _("Graphs and presentations");
	}
	if (intent == GCM_INTENT_ABSOLUTE_COLORMETRIC) {
		/* TRANSLATORS: rendering intent: you probably want to google this */
		return _("Proofing devices");
	}
	return "unknown";
}


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
#include <colord.h>
#include <math.h>

#include "gcm-utils.h"

#define PK_DBUS_SERVICE					"org.freedesktop.PackageKit"
#define PK_DBUS_PATH					"/org/freedesktop/PackageKit"
#define PK_DBUS_INTERFACE_QUERY				"org.freedesktop.PackageKit.Query"
#define PK_DBUS_INTERFACE_MODIFY			"org.freedesktop.PackageKit.Modify"

/**
 * gcm_utils_linkify:
 **/
gchar *
gcm_utils_linkify (const gchar *hostile_text)
{
	guint i;
	guint j = 0;
	gboolean ret;
	GString *string;
	gchar *text;

	/* Properly escape this as some profiles 'helpfully' put markup in like:
	 * "Copyright (C) 2005-2010 Kai-Uwe Behrmann <www.behrmann.name>" */
	text = g_markup_escape_text (hostile_text, -1);

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
	g_free (text);
	return g_string_free (string, FALSE);
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
	g_warning ("cannot install %s: this package was not compiled with --enable-packagekit", package_name);
	goto out;
#endif

	/* get xid of this window */
	if (window != NULL)
		xid = gdk_x11_window_get_xid (gtk_widget_get_window (GTK_WIDGET(window)));

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
						NULL,
						G_DBUS_CALL_FLAGS_NONE,
						G_MAXINT, NULL, &error);
	if (response == NULL) {
		/* TRANSLATORS: the DBus method failed */
		g_warning ("%s %s\n", _("The request failed:"), error->message);
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
		uri = g_strconcat ("help:gnome-color-manager?", link_id, NULL);
	else
		uri = g_strdup ("help:gnome-color-manager");
	g_debug ("opening uri %s", uri);

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
 * cd_colorspace_to_localised_string:
 **/
const gchar *
cd_colorspace_to_localised_string (CdColorspace colorspace)
{
	if (colorspace == CD_COLORSPACE_RGB) {
		/* TRANSLATORS: this is the colorspace, e.g. red, green, blue */
		return _("RGB");
	}
	if (colorspace == CD_COLORSPACE_CMYK) {
		/* TRANSLATORS: this is the colorspace, e.g. cyan, magenta, yellow, black */
		return _("CMYK");
	}
	if (colorspace == CD_COLORSPACE_GRAY) {
		/* TRANSLATORS: this is the colorspace type */
		return _("gray");
	}
	return NULL;
}

/**
 * gcm_utils_get_pixel_format:
 **/
static CdPixelFormat
gcm_utils_get_pixel_format (GdkPixbuf *pixbuf)
{
	guint bits;
	CdPixelFormat format = CD_PIXEL_FORMAT_UNKNOWN;

	/* no alpha channel */
	bits = gdk_pixbuf_get_bits_per_sample (pixbuf);
	if (!gdk_pixbuf_get_has_alpha (pixbuf) && bits == 8) {
		format = CD_PIXEL_FORMAT_RGB24;
		goto out;
	}

	/* alpha channel */
	if (bits == 8) {
		format = CD_PIXEL_FORMAT_RGBA32;
		goto out;
	}
out:
	return format;
}

/**
 * gcm_utils_image_convert:
 **/
gboolean
gcm_utils_image_convert (GtkImage *image,
			 CdIcc *input,
			 CdIcc *abstract,
			 CdIcc *output,
			 GError **error)
{
	CdPixelFormat pixel_format;
	CdTransform *transform = NULL;
	GdkPixbuf *pixbuf;
	GdkPixbuf *original_pixbuf;
	gboolean ret = TRUE;
	guchar *data;
	guint bpp;

	/* get pixbuf */
	pixbuf = gtk_image_get_pixbuf (image);
	if (pixbuf == NULL)
		goto out;

	/* work out the pixel format */
	pixel_format = gcm_utils_get_pixel_format (pixbuf);
	if (pixel_format == CD_PIXEL_FORMAT_UNKNOWN) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0, "format not supported");
		goto out;
	}

	/* get a copy of the original image, *not* a ref */
	original_pixbuf = g_object_get_data (G_OBJECT (pixbuf), "GcmImageOld");
	if (original_pixbuf == NULL) {
		data = g_memdup (gdk_pixbuf_get_pixels (pixbuf),
				 gdk_pixbuf_get_bits_per_sample (pixbuf) *
				 gdk_pixbuf_get_rowstride (pixbuf) *
				 gdk_pixbuf_get_height (pixbuf) / 8);
		original_pixbuf = gdk_pixbuf_new_from_data (data,
			  gdk_pixbuf_get_colorspace (pixbuf),
			  gdk_pixbuf_get_has_alpha (pixbuf),
			  gdk_pixbuf_get_bits_per_sample (pixbuf),
			  gdk_pixbuf_get_width (pixbuf),
			  gdk_pixbuf_get_height (pixbuf),
			  gdk_pixbuf_get_rowstride (pixbuf),
			  (GdkPixbufDestroyNotify) g_free, NULL);
		g_object_set_data_full (G_OBJECT (pixbuf), "GcmImageOld",
					original_pixbuf,
					(GDestroyNotify) g_object_unref);
	}

	/* convert in-place */
	transform = cd_transform_new ();
	cd_transform_set_input_icc (transform, input);
	cd_transform_set_abstract_icc (transform, abstract);
	cd_transform_set_output_icc (transform, output);
	cd_transform_set_rendering_intent (transform, CD_RENDERING_INTENT_PERCEPTUAL);
	cd_transform_set_input_pixel_format (transform, pixel_format);
	cd_transform_set_output_pixel_format (transform, pixel_format);
	bpp = gdk_pixbuf_get_rowstride (pixbuf) / gdk_pixbuf_get_width (pixbuf);
	ret = cd_transform_process (transform,
				    gdk_pixbuf_get_pixels (original_pixbuf),
				    gdk_pixbuf_get_pixels (pixbuf),
				    gdk_pixbuf_get_width (pixbuf),
				    gdk_pixbuf_get_height (pixbuf),
				    gdk_pixbuf_get_rowstride (pixbuf) / bpp,
				    NULL,
				    error);
	if (!ret)
		goto out;

	/* refresh */
	g_object_ref (pixbuf);
	gtk_image_set_from_pixbuf (image, pixbuf);
	g_object_unref (pixbuf);
out:
	if (transform != NULL)
		g_object_unref (transform);
	return ret;
}

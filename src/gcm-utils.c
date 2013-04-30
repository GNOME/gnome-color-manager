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
#include <lcms2.h>
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
 * cd_icc_has_colorspace_description:
 * @profile: A valid #CdProfile
 *
 * Finds out if the profile contains a colorspace description.
 *
 * Return value: %TRUE if the description mentions the profile colorspace explicity,
 * e.g. "Adobe RGB" for %CD_COLORSPACE_RGB.
 **/
gboolean
cd_icc_has_colorspace_description (CdProfile *profile)
{
	CdColorspace colorspace;
	const gchar *description;

	g_return_val_if_fail (CD_IS_PROFILE (profile), FALSE);

	/* for each profile type */
	colorspace = cd_profile_get_colorspace (profile);
	description = cd_profile_get_title (profile);
	if (colorspace == CD_COLORSPACE_RGB)
		return (g_strstr_len (description, -1, "RGB") != NULL);
	if (colorspace == CD_COLORSPACE_CMYK)
		return (g_strstr_len (description, -1, "CMYK") != NULL);

	/* nothing */
	return FALSE;
}

/**
 * cd_icc_generate_curve:
 * @icc: A valid #CdIcc
 * @size: the size of the curve to generate
 *
 * Generates a curve of a specified size.
 *
 * Return value: A #GPtrArray object, or %NULL. Free with g_object_unref()
 **/
GPtrArray *
cd_icc_generate_curve (CdIcc *icc, guint size)
{
	gdouble *values_in = NULL;
	gdouble *values_out = NULL;
	guint i;
	CdColorRGB *data;
	GPtrArray *array = NULL;
	gfloat divamount;
	gfloat divadd;
	guint component_width;
	cmsHPROFILE srgb_profile = NULL;
	cmsHTRANSFORM transform = NULL;
	guint type;
	CdColorspace colorspace;
	gdouble tmp;
	cmsHPROFILE lcms_profile;

	/* run through the icc */
	colorspace = cd_icc_get_colorspace (icc);
	if (colorspace == CD_COLORSPACE_RGB) {

		/* RGB */
		component_width = 3;
		type = TYPE_RGB_DBL;

		/* create input array */
		values_in = g_new0 (gdouble, size * 3 * component_width);
		divamount = 1.0f / (gfloat) (size - 1);
		for (i = 0; i < size; i++) {
			divadd = divamount * (gfloat) i;

			/* red component */
			values_in[(i * 3 * component_width)+0] = divadd;
			values_in[(i * 3 * component_width)+1] = 0.0f;
			values_in[(i * 3 * component_width)+2] = 0.0f;

			/* green component */
			values_in[(i * 3 * component_width)+3] = 0.0f;
			values_in[(i * 3 * component_width)+4] = divadd;
			values_in[(i * 3 * component_width)+5] = 0.0f;

			/* blue component */
			values_in[(i * 3 * component_width)+6] = 0.0f;
			values_in[(i * 3 * component_width)+7] = 0.0f;
			values_in[(i * 3 * component_width)+8] = divadd;
		}
	}

	/* do each transform */
	if (values_in != NULL) {
		/* create output array */
		values_out = g_new0 (gdouble, size * 3 * component_width);

		/* create a transform from icc to sRGB */
		srgb_profile = cmsCreate_sRGBProfile ();
		lcms_profile = cd_icc_get_handle (icc);
		transform = cmsCreateTransform (lcms_profile, type, srgb_profile, TYPE_RGB_DBL, INTENT_PERCEPTUAL, 0);
		if (transform == NULL)
			goto out;

		/* do transform */
		cmsDoTransform (transform, values_in, values_out, size * 3);

		/* create output array */
		array = g_ptr_array_new_with_free_func (g_free);

		for (i = 0; i < size; i++) {
			data = g_new0 (CdColorRGB, 1);

			/* default values */
			data->R = 0.0f;
			data->G = 0.0f;
			data->B = 0.0f;

			/* only save curve data if it is positive */
			tmp = values_out[(i * 3 * component_width)+0];
			if (tmp > 0.0f)
				data->R = tmp;
			tmp = values_out[(i * 3 * component_width)+4];
			if (tmp > 0.0f)
				data->G = tmp;
			tmp = values_out[(i * 3 * component_width)+8];
			if (tmp > 0.0f)
				data->B = tmp;
			g_ptr_array_add (array, data);
		}
	}

out:
	g_free (values_in);
	g_free (values_out);
	if (transform != NULL)
		cmsDeleteTransform (transform);
	if (srgb_profile != NULL)
		cmsCloseProfile (srgb_profile);
	return array;
}

#define HYP(a,b)		(sqrt((a)*(a) + (b)*(b)))

/**
 * cd_icc_create_lab_cube:
 *
 * The original code was taken from icc_examin,
 *  Copyright 2004-2009 Kai-Uwe Behrmann <ku.b@gmx.de>
 **/
static gdouble *
cd_icc_create_lab_cube (guint res)
{
	gdouble *lab = NULL;
	gdouble max = 0.99;
	gdouble min = 0.01;
	gint area;
	gint channels_n = 3;
	gint pos;
	gsize size;
	guint x, y;

	size = 4 * res * (res+1) + 2 * (res-1) * (res-1);
	lab = g_new0 (gdouble, size * channels_n);
	if (lab == NULL)
		goto out;

	g_debug ("created 2*%ix%i array", (guint)size, (guint)channels_n);

	/* side squares */
	for (y = 0; y <= res; ++y) {
		for (x = 0; x < 4 * res; ++x) {
			area = 0;
			pos = (y * 4 * res + x) * channels_n;

			lab[pos + 0] = pow(0.9999 - (gdouble)y / (gdouble)res, 2.0) + 0.0001;
			if (area * res <= x && x < ++area * res) {
				lab[pos + 1] = min + (x - (area - 1) * res) / (gdouble)res * (max-min);
				lab[pos + 2] = min;
			} else if (area * res <= x && x < ++area * res) {
				lab[pos + 1] = max;
				lab[pos + 2] = min + (x - (area - 1) * res) / (gdouble)res * (max-min);
			} else if (area * res <= x && x < ++area * res) {
				lab[pos + 1] = max - (x - (area - 1) * res) / (gdouble)res * (max-min);
				lab[pos + 2] = max;
			} else if (area * res <= x && x < ++area * res) {
				lab[pos + 1] = min;
				lab[pos + 2] = max - (x - (area - 1) * res) / (double)res * (max-min);
			}
		}
	}

	/* bottom and top square */
	for (y = 0; y < (res - 1); ++y) {
		for (x = 0; x < 2 * (res - 1); ++x) {
			gint x_pos;
			gint y_pos;
			gdouble val;

			pos = (4 * res * (res + 1) + y * 2 * (res - 1) + x) * channels_n;
			area = 1;
			x_pos = x + 1;
			y_pos = y + 1;
			val = (gdouble)y_pos/(gdouble)res * (max-min);

			if (/*0 <= x &&*/ x < res - 1) {
				lab[pos + 0] = 1.0;
				lab[pos + 1] = min + (x_pos - (area - 1) * (res - 1)) / (gdouble)res * (max-min);
				lab[pos + 2] = min + val;
			} else if (res - 1 <= x && x < 2 * res - 2) {
				++area;
				lab[pos + 1] = min + (x_pos - (area - 1) * (res - 1)) / (gdouble)res * (max-min);
				lab[pos + 2] = min + val;
				lab[pos + 0] = HYP (lab[pos + 1] - 0.5, lab[pos + 2] - 0.5)/100.; /* 0.0 */
			}
		}
	}
out:
	return lab;
}

/**
 * cd_icc_create_hull_for_data:
 *
 * The original code was taken from icc_examin,
 *  Copyright 2004-2009 Kai-Uwe Behrmann <ku.b@gmx.de>
 **/
static GcmHull *
cd_icc_create_hull_for_data (guint res, gdouble *lab, gdouble *rgb)
{
	CdColorRGB color;
	CdColorXYZ xyz;
	GcmHull *hull = NULL;
	gint channels_n = 3;
	gint off;
	gsize i;
	gsize size;
	guint face[3];
	guint x, y;

	size = 4 * res * (res+1) + 2 * (res-1) * (res-1);

	hull = gcm_hull_new ();

	/* collect colour points */
	for (i = 0; i < size; ++i) {
		xyz.X = lab[i*channels_n+0];
		xyz.Y = lab[i*channels_n+1];
		xyz.Z = lab[i*channels_n+2];
		color.R = rgb[i*channels_n+0];
		color.G = rgb[i*channels_n+1];
		color.B = rgb[i*channels_n+2];
		gcm_hull_add_vertex (hull, &xyz, &color);
	}

	for (y = 0; y < res; ++y) {
		for (x = 0; x < 4 * res; ++x) {
			gint x_ = x;
			if (x == 4 * res - 1)
				x_ = -1;
			face[0] = y * 4*res+x;
			face[1] = y * 4*res+x_+1;
			face[2] = (y+1)*4*res+x;
			gcm_hull_add_face (hull, face, 3);

			face[0] = y * 4*res+x_+1;
			face[1] = (y+1)*4*res+x_+1;
			face[2] = (y+1)*4*res+x;
			gcm_hull_add_face (hull, face, 3);
		}
	}

	off = 4 * res * (res + 1);

	/* 1 0 0 (L res b) */
	face[0] = 4*res-1;
	face[1] = off;
	face[2] = 0;
	gcm_hull_add_face (hull, face, 3);

	face[0] = off;
	face[2] = 0;
	face[1] = 1;
	gcm_hull_add_face (hull, face, 3);

	/* 0 0 0 */
	face[1] = off-1;
	face[0] = off+res-1;
	face[2] = off-4*res;
	gcm_hull_add_face (hull, face, 3);

	face[1] = off+res-1;
	face[2] = off-4*res;
	face[0] = off - 4*res+1;
	gcm_hull_add_face (hull, face, 3);

	/* 0 0 1 */
	face[2] = off-res;
	face[1] = off-res-1;
	face[0] = off+2*(res-1)*(res-1)-res+1;
	gcm_hull_add_face (hull, face, 3);

	face[0] = off-res;
	face[1] = off-res+1;
	face[2] = off+2*(res-1)*(res-1)-res+1;
	gcm_hull_add_face (hull, face, 3);

	/* 0 1 1 */
	face[0] = off-2*res+1;
	face[2] = off-2*res;
	face[1] = off+2*(res-1)*(res-1)-1;
	gcm_hull_add_face (hull, face, 3);

	face[1] = off-2*res;
	face[2] = off+2*(res-1)*(res-1)-1;
	face[0] = off-2*res-1;
	gcm_hull_add_face (hull, face, 3);

	/* 1 1 1 */
	face[0] = 2*res-1;
	face[2] = 2*res;
	face[1] = off+2*(res-1)*(res-1)-res;
	gcm_hull_add_face (hull, face, 3);

	face[1] = 2*res;
	face[2] = off+2*(res-1)*(res-1)-res;
	face[0] = 2*res+1;
	gcm_hull_add_face (hull, face, 3);

	/* 1 0 1 */
	face[2] = 3*res;
	face[0] = 3*res-1;
	face[1] = off+2*(res-1)*(res-1)-2*res+2;
	gcm_hull_add_face (hull, face, 3);

	face[2] = 3*res;
	face[1] = 3*res+1;
	face[0] = off+2*(res-1)*(res-1)-2*res+2;
	gcm_hull_add_face (hull, face, 3);

	/* 1 1 0 */
	face[0] = off+res-2;
	face[1] = res + 1;
	face[2] = res - 1;
	gcm_hull_add_face (hull, face, 3);

	face[0] = res + 1;
	face[2] = res - 1;
	face[1] = res;
	gcm_hull_add_face (hull, face, 3);

	/* 0 1 0 */
	face[0] = off+2*(res-1)-1;
	face[1] = off-3*res-1;
	face[2] = off-3*res;
	gcm_hull_add_face (hull, face, 3);

	face[1] = off+2*(res-1)-1;
	face[0] = off-3*res+1;
	face[2] = off-3*res+0;
	gcm_hull_add_face (hull, face, 3);

	for (y = 0; y < res; ++y) {
		if (0 < y && y < res - 1) {
			/* 0 0 . */
			face[2] = off-y;
			face[0] = off+(y+1)*2*(res-1)-res+1;
			face[1] = off-y-1;
			gcm_hull_add_face (hull, face, 3);

			face[0] = off+(y+0)*2*(res-1)-res+1;
			face[2] = off-y;
			face[1] = off+(y+1)*2*(res-1)-res+1;
			gcm_hull_add_face (hull, face, 3);

			/* 0 1 . */
			face[1] = off+(y+1)*2*(res-1)-1;
			face[0] = off-3*res+y+1;
			face[2] = off+(y)*2*(res-1)-1;
			gcm_hull_add_face (hull, face, 3);

			face[1] = off-3*res+y+1;
			face[2] = off+(y)*2*(res-1)-1;
			face[0] = off-3*res+y;
			gcm_hull_add_face (hull, face, 3);

			/* 1 0 . */
			face[0] = off+2*(res-1)*(res-1)-(y+1)*2*(res-1);
			face[1] = 3*res+y+1;
			face[2] = off+2*(res-1)*(res-1)-y*2*(res-1);
			gcm_hull_add_face (hull, face, 3);

			face[0] = 3*res+y+1;
			face[2] = off+2*(res-1)*(res-1)-y*2*(res-1);
			face[1] = 3*res+y;
			gcm_hull_add_face (hull, face, 3);

			/* 1 1 . */
			face[0] = off+2*(res-1)*(res-1)-(y+1)*2*(res-1)+res-2;
			face[1] = off+2*(res-1)*(res-1)-(y+0)*2*(res-1)+res-2;
			face[2] = 2*res-y;
			gcm_hull_add_face (hull, face, 3);

			face[0] = 2*res-y-1;
			face[1] = off+2*(res-1)*(res-1)-(y+1)*2*(res-1)+res-2;
			face[2] = 2*res-y;
			gcm_hull_add_face (hull, face, 3);
		}

		for (x = 0; x < 2 * res; ++x) {
			gint x_ = x + off;

			/* lower border */
			if ( y == 0 ) {
				if (x == 0) {
				} else if (x == res - 1) {
				} else if (x < res - 1) {
					/* 1 . 0 */
					face[0] = off + x - 1;
					face[1] = off + x;
					face[2] = x;
					gcm_hull_add_face (hull, face, 3);

					face[0] = off + x;
					face[2] = x;
					face[1] = x + 1;
					gcm_hull_add_face (hull, face, 3);

					/* 0 . 1 */
					face[0] = off-res-x;
					face[2] = off-res-x-1;
					face[1] = off+2*(res-1)*(res-1)-res+x;
					gcm_hull_add_face (hull, face, 3);

					face[2] = off-res-x-1;
					face[0] = off+2*(res-1)*(res-1)-res+x;
					face[1] = off+2*(res-1)*(res-1)-res+x+1;
					gcm_hull_add_face (hull, face, 3);

					/* 1 . 1 */
					face[0] = 3*res - x;
					face[1] = 3*res - x-1;
					face[2] = off+2*(res-1)*(res-1)-2*(res-1)+x-1;
					gcm_hull_add_face (hull, face, 3);

					face[0] = 3*res - x-1;
					face[2] = off+2*(res-1)*(res-1)-2*(res-1)+x-1;
					face[1] = off+2*(res-1)*(res-1)-2*(res-1)+x;
					gcm_hull_add_face (hull, face, 3);

				} else if (x > res + 1) {
					/* 0 . 0 */
					face[0] = off+x-3;
					face[2] = off+x-3+1;
					face[1] = 4*res*(res+1)-4*res + x-res-1;
					gcm_hull_add_face (hull, face, 3);

					face[1] = off+x-3+1;
					face[2] = 4*res*(res+1)-4*res + x-res-1;
					face[0] = 4*res*(res+1)-4*res + x-res;
					gcm_hull_add_face (hull, face, 3);
				}

			/* upper border */
			} else if ( y == res - 1 ) {
				if (x == 0) {
				}
			} else if (/*0 <= x &&*/ x < res - 1 - 1) {

				/* upper middle field (*L=0.0) */
				face[0] = (y-1) * 2*(res-1)+x_;
				face[2] = (y-1)*2*(res-1)+x_+1;
				face[1] = (y+0)*2*(res-1)+x_;
				gcm_hull_add_face (hull, face, 3);

				face[2] = (y-1)*2*(res-1)+x_+1;
				face[0] = (y+0)*2*(res-1)+x_;
				face[1] = (y+0)*2*(res-1)+x_+1;
				gcm_hull_add_face (hull, face, 3);

			} else if (res - 1 <= x && x < 2 * res - 2 - 1) {

				/* lower middle field (*L=1.0) */
				face[0] = (y-1) * 2*(res-1)+x_;
				face[1] = (y-1)*2*(res-1)+x_+1;
				face[2] = (y+0)*2*(res-1)+x_;
				gcm_hull_add_face (hull, face, 3);

				face[0] = (y-1)*2*(res-1)+x_+1;
				face[2] = (y+0)*2*(res-1)+x_;
				face[1] = (y+0)*2*(res-1)+x_+1;
				gcm_hull_add_face (hull, face, 3);
			}
		}
	}

	return hull;
}

/**
 * cd_icc_generate_gamut_hull:
 * @icc: a #CdIcc
 * @res: The resolution. 10 is quick, 20 is more precise. 12 is a good default.
 *
 * A cube from six squares with the range of the Lab cube will be
 * transformed to a icc colour space and then converted to a
 * mesh.
 *
 * The original code was taken from icc_examin,
 *  Copyright 2004-2009 Kai-Uwe Behrmann <ku.b@gmx.de>
 **/
GcmHull *
cd_icc_generate_gamut_hull (CdIcc *icc, guint res)
{
	cmsHPROFILE lab_profile = NULL;
	cmsHPROFILE srgb_profile = NULL;
	cmsHTRANSFORM lab_transform = NULL;
	cmsHTRANSFORM srgb_transform = NULL;
	cmsHPROFILE lcms_profile;
	GcmHull *hull = NULL;
	gdouble *lab = NULL;
	gdouble *rgb = NULL;
	gint channels_n = 3;
	gsize size = 4 * res * (res+1) + 2 * (res-1) * (res-1);

	/* create data array */
	lab = cd_icc_create_lab_cube (res);
	rgb = g_new0 (gdouble, size * channels_n);
	if (rgb == NULL)
		goto out;

	/* run the cube through the Lab icc */
	lab_profile = cmsCreateLab4Profile (cmsD50_xyY ());
	lcms_profile = cd_icc_get_handle (icc);
	lab_transform = cmsCreateTransform (lcms_profile, TYPE_RGB_DBL,
					    lab_profile, TYPE_Lab_DBL,
					    INTENT_ABSOLUTE_COLORIMETRIC, 0);
	if (lab_transform == NULL) {
		g_warning ("failed to create Lab transform");
		goto out;
	}
	cmsDoTransform (lab_transform, lab, lab, size);

	/* run the cube through the sRGB icc */
	srgb_profile = cmsCreate_sRGBProfile ();
	srgb_transform = cmsCreateTransform (lab_profile, TYPE_Lab_DBL,
					     srgb_profile, TYPE_RGB_DBL,
					     INTENT_ABSOLUTE_COLORIMETRIC, 0);
	if (srgb_transform == NULL) {
		g_warning ("failed to create sRGB transform");
		goto out;
	}
	cmsDoTransform (srgb_transform, lab, rgb, size);

	/* create gamut hull */
	hull = cd_icc_create_hull_for_data (res, lab, rgb);
out:
	g_free (rgb);
	g_free (lab);
	if (lab_profile != NULL)
		cmsCloseProfile (lab_profile);
	if (srgb_profile != NULL)
		cmsCloseProfile (srgb_profile);
	if (lab_transform != NULL)
		cmsDeleteTransform (lab_transform);
	if (srgb_transform != NULL)
		cmsDeleteTransform (srgb_transform);
	return hull;
}

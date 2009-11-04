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
 * GNU General Public License for more utils.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <X11/Xatom.h>

#include "gcm-utils.h"
#include "gcm-edid.h"
#include "egg-debug.h"

/**
 * gcm_utils_output_is_lcd_internal:
 * @output_name: the output name
 *
 * Return value: %TRUE is the output is an internal LCD panel
 **/
gboolean
gcm_utils_output_is_lcd_internal (const gchar *output_name)
{
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
	if (gcm_utils_output_is_lcd_internal (output_name))
		return TRUE;
	if (g_strstr_len (output_name, -1, "DVI") != NULL)
		return TRUE;
	if (g_strstr_len (output_name, -1, "dvi") != NULL)
		return TRUE;
	return FALSE;
}

/**
 * gcm_utils_get_output_name:
 *
 * Return value: the output name, free with g_free().
 **/
gchar *
gcm_utils_get_output_name (GnomeRROutput *output)
{
	const guint8 *data;
	guint i, j;
	const gchar *output_name;
	gchar *name = NULL;
	GcmEdid *edid = NULL;
	gboolean ret;

	/* if nothing connected then fall back to the connector name */
	ret = gnome_rr_output_is_connected (output);
	if (!ret)
		goto out;

	/* parse the EDID to get a crtc-specific name, not an output specific name */
	data = gnome_rr_output_get_edid_data (output);
	if (data == NULL)
		goto out;

	edid = gcm_edid_new ();
	ret = gcm_edid_parse (edid, data, NULL);
	if (!ret) {
		egg_warning ("failed to parse edid");
		goto out;
	}

	/* find the best option */
	g_object_get (edid, "monitor-name", &name, NULL);
	if (name == NULL)
		g_object_get (edid, "ascii-string", &name, NULL);
	if (name == NULL)
		g_object_get (edid, "serial-number", &name, NULL);

out:
	/* fallback to the output name */
	if (name == NULL) {
		output_name = gnome_rr_output_get_name (output);
		ret = gcm_utils_output_is_lcd_internal (output_name);
		if (ret)
			output_name = _("Internal LCD");
		name = g_strdup (output_name);
	}

	if (edid != NULL)
		g_object_unref (edid);
	return name;
}

/**
 * gcm_utils_get_gamma_size:
 *
 * Return value: the gamma size, or 0 if error;
 **/
guint
gcm_utils_get_gamma_size (GnomeRRCrtc *crtc, GError **error)
{
	guint id;
	guint size;

	/* get id that X recognises */
	id = gnome_rr_crtc_get_id (crtc);

	/* get the value, and catch errors */
	gdk_error_trap_push ();
	size = XRRGetCrtcGammaSize (GDK_DISPLAY(), id);
	if (gdk_error_trap_pop ()) {
		size = 0;
		if (error != NULL)
			*error = g_error_new (1, 0, "failed to get gamma size");
		goto out;
	}
out:
	return size;
}

/**
 * gcm_utils_set_crtc_gamma:
 *
 * Return value: %TRUE for success;
 **/
gboolean
gcm_utils_set_crtc_gamma (GnomeRRCrtc *crtc, GcmClut *clut, GError **error)
{
	guint id;
	gboolean ret = TRUE;
	GPtrArray *array = NULL;
	XRRCrtcGamma *gamma = NULL;
	guint i;
	GcmClutData *data;

	/* get data */
	array = gcm_clut_get_array (clut);
	if (array == NULL) {
		ret = FALSE;
		if (error != NULL)
			*error = g_error_new (1, 0, "failed to get CLUT data");
		goto out;
	}

	/* no length? */
	if (array->len == 0) {
		ret = FALSE;
		if (error != NULL)
			*error = g_error_new (1, 0, "no data in the CLUT array");
		goto out;
	}

	/* convert to a type X understands */
	gamma = XRRAllocGamma (array->len);
	for (i=0; i<array->len; i++) {
		data = g_ptr_array_index (array, i);
		gamma->red[i] = data->red;
		gamma->green[i] = data->green;
		gamma->blue[i] = data->blue;
	}

	/* get id that X recognises */
	id = gnome_rr_crtc_get_id (crtc);

	/* get the value, and catch errors */
	gdk_error_trap_push ();
	XRRSetCrtcGamma (GDK_DISPLAY(), id, gamma);
	gdk_flush ();
	if (gdk_error_trap_pop ()) {
		ret = FALSE;
		if (error != NULL)
			*error = g_error_new (1, 0, "failed to set crtc gamma %p (%i) on %i", gamma, array->len, id);
		goto out;
	}
out:
	if (gamma != NULL)
		XRRFreeGamma (gamma);
	if (array != NULL)
		g_ptr_array_unref (array);
	return ret;
}

/**
 * gcm_utils_get_clut_for_output:
 **/
GcmClut *
gcm_utils_get_clut_for_output (GnomeRROutput *output, GError **error)
{
	gchar *name = NULL;
	gboolean connected;
	GnomeRRCrtc *crtc;
	guint size;
	gboolean ret;
	GcmClut *clut = NULL;
	GError *error_local = NULL;

	/* don't set the gamma if there is no device */
	connected = gnome_rr_output_is_connected (output);
	if (!connected)
		goto out;

	/* get data */
	name = gcm_utils_get_output_name (output);
	egg_debug ("[%i] output %p (name=%s)", connected, output, name);

	/* get crtc */
	crtc = gnome_rr_output_get_crtc (output);

	/* get gamma size */
	size = gcm_utils_get_gamma_size (crtc, error);
	if (size == 0)
		goto out;

	/* create a lookup table */
	clut = gcm_clut_new ();

	/* set correct size */
	g_object_set (clut,
		      "size", size,
		      "id", name,
		      NULL);

	/* lookup from config file */
	ret = gcm_clut_load_from_config (clut, &error_local);
	if (!ret) {
		/* this is not fatal */
		egg_debug ("failed to get values for %s: %s", name, error_local->message);
		g_error_free (error_local);
	}
out:
	g_free (name);
	return clut;
}

/**
 * gcm_utils_set_output_gamma:
 *
 * Return value: %TRUE for success;
 **/
gboolean
gcm_utils_set_output_gamma (GnomeRROutput *output, GError **error)
{
	gboolean ret = FALSE;
	GcmClut *clut = NULL;
	GnomeRRCrtc *crtc;
	gint x, y;
	gchar *filename;

	/* get CLUT */
	clut = gcm_utils_get_clut_for_output (output, error);
	if (clut == NULL)
		goto out;

	/* get filename of the profile, or NULL */
	g_object_get (clut,
		      "profile", &filename,
		      NULL);

	/* get crtc */
	crtc = gnome_rr_output_get_crtc (output);

	/* actually set the gamma */
	ret = gcm_utils_set_crtc_gamma (crtc, clut, error);
	if (!ret)
		goto out;

	/* is the monitor our primary monitor */
	gnome_rr_output_get_position (output, &x, &y);
	if (x == 0 && y == 0 && filename != NULL) {
		egg_debug ("setting main ICC profile atom from %s", filename);
		ret = gcm_utils_set_x11_icc_profile (0, filename, error);
		if (!ret)
			goto out;
	}
out:
	g_free (filename);
	if (clut != NULL)
		g_object_unref (clut);
	return ret;
}

/**
 * gcm_utils_get_profile_filenames_for_directory:
 **/
static gboolean
gcm_utils_get_profile_filenames_for_directory (GPtrArray *array, const gchar *directory)
{
	GDir *dir;
	GError *error = NULL;
	gboolean ret = TRUE;
	const gchar *name;

	/* get contents */
	dir = g_dir_open (directory, 0, &error);
	if (dir == NULL) {
		egg_warning ("failed to open: %s", error->message);
		g_error_free (error);
		ret = FALSE;
		goto out;
	}

	/* process entire list */
	do {
		name = g_dir_read_name (dir);
		if (name == NULL)
			break;
		if (g_str_has_suffix (name, ".icc") ||
		    g_str_has_suffix (name, ".icm") ||
		    g_str_has_suffix (name, ".ICC") ||
		    g_str_has_suffix (name, ".ICM")) {
			egg_debug ("adding %s", name);
			g_ptr_array_add (array, g_build_filename (directory, name, NULL));
		}
	} while (TRUE);
out:
	if (dir != NULL)
		g_dir_close (dir);
	return ret;
}

/**
 * gcm_utils_get_profile_filenames:
 *
 * Return value, an array of strings, free with g_ptr_array_unref()
 **/
GPtrArray *
gcm_utils_get_profile_filenames (void)
{
	GPtrArray *array;
	gchar *user;

	/* create output array */
	array = g_ptr_array_new_with_free_func (g_free);

	/* get systemwide profiles */
	gcm_utils_get_profile_filenames_for_directory (array, "/usr/share/color/icc");

	/* get gnome-color-manager test profiles */
	gcm_utils_get_profile_filenames_for_directory (array, GCM_DATA "/profiles");

	/* get per-user profiles */
	user = g_build_filename (g_get_home_dir (), "/.color/icc", NULL);
	gcm_utils_get_profile_filenames_for_directory (array, user);
	g_free (user);

	return array;
}

/**
 * gcm_utils_mkdir_and_copy:
 **/
gboolean
gcm_utils_mkdir_and_copy (const gchar *source, const gchar *destination, GError **error)
{
	gboolean ret;
	gchar *path;
	GFile *sourcefile;
	GFile *destfile;
	GFile *destpath;

	/* setup paths */
	sourcefile = g_file_new_for_path (source);
	path = g_path_get_dirname (destination);
	destpath = g_file_new_for_path (path);
	destfile = g_file_new_for_path (destination);

	/* ensure desination exists */
	ret = g_file_test (path, G_FILE_TEST_EXISTS);
	if (!ret) {
		ret = g_file_make_directory_with_parents  (destpath, NULL, error);
		if (!ret)
			goto out;
	}

	/* do the copy */
	egg_debug ("copying from %s to %s", source, path);
	ret = g_file_copy (sourcefile, destfile, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, error);
	if (!ret)
		goto out;
out:
	g_free (path);
	g_object_unref (sourcefile);
	g_object_unref (destpath);
	g_object_unref (destfile);
	return ret;
}

/**
 * gcm_utils_get_profile_destination:
 **/
gchar *
gcm_utils_get_profile_destination (const gchar *filename)
{
	gchar *basename;
	gchar *destination;

	/* get destination filename for this source file */
	basename = g_path_get_basename (filename);
	destination = g_build_filename (g_get_home_dir (), GCM_PROFILE_PATH, basename, NULL);

	g_free (basename);
	return destination;
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
 * gpk_gnome_help:
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
 * gcm_utils_get_x11_icc_profile_data:
 *
 * @id: the ID that is used according to ICC Profiles In X Specification
 * @data: the data that is returned from the XServer. Free with g_free()
 * @length: the size of the returned data, or %NULL if you don't care
 * @error: a %GError that is set in the result of an error, or %NULL
 * Return value: %TRUE for success.
 *
 * TODO: the ICC Profiles In X Specification is very vague about how the id
 * map to the RROutput name or ID. Seek clarification.
 *
 * Gets the ICC profile data from the XServer.
 **/
gboolean
gcm_utils_get_x11_icc_profile_data (guint id, guint8 **data, gsize *length, GError **error)
{
	gboolean ret = FALSE;
	gchar *atom_name;
	gchar *data_tmp = NULL;
	gint format;
	gint rc;
	gulong bytes_after;
	gulong nitems;
	Atom atom = None;
	Atom type;
	Display	*display;
	GdkDisplay *display_gdk;
	GdkWindow *window_gdk;
	Window window;

	g_return_val_if_fail (data != NULL, FALSE);

	/* get defaults for single screen */
	display_gdk = gdk_display_get_default ();
	window_gdk = gdk_get_default_root_window ();
	display = GDK_DISPLAY_XDISPLAY (display_gdk);
	window = GDK_WINDOW_XID (window_gdk);

	/* get the atom name */
	if (id == 0)
		atom_name = g_strdup ("_ICC_PROFILE");
	else
		atom_name = g_strdup_printf ("_ICC_PROFILE_%i", id);

	/* get the value */
	gdk_error_trap_push ();
	atom = gdk_x11_get_xatom_by_name_for_display (display_gdk, atom_name);
	rc = XGetWindowProperty (display, window, atom, 0, G_MAXLONG, False, XA_CARDINAL,
				 &type, &format, &nitems, &bytes_after, (void*) &data_tmp);
	gdk_error_trap_pop ();

	/* did the call fail */
	if (rc != Success) {
		if (error != NULL)
			*error = g_error_new (1, 0, "failed to get %s atom with rc %i", atom_name, rc);
		goto out;
	}

	/* was nothing found */
	if (nitems == 0) {
		if (error != NULL)
			*error = g_error_new (1, 0, "%s atom has not been set", atom_name);
		goto out;
	}

	/* allocate the data using Glib, rather than asking the user to use XFree */
	*data = g_new0 (guint8, nitems);
	memcpy (*data, data_tmp, nitems);

	/* copy the length */
	if (length != NULL)
		*length = nitems;

	/* success */
	ret = TRUE;
out:
	g_free (atom_name);
	if (data_tmp != NULL)
		XFree (data_tmp);
	return ret;
}

/**
 * gcm_utils_set_x11_icc_profile:
 * @id: the ID that is used according to ICC Profiles In X Specification
 * @filename: the filename of the ICC profile
 * @error: a %GError that is set in the result of an error, or %NULL
 * Return value: %TRUE for success.
 *
 * Sets the ICC profile data to the XServer.
 *
 * TODO: the ICC Profiles In X Specification is very vague about how the id
 * map to the RROutput name or ID. Seek clarification.
 **/
gboolean
gcm_utils_set_x11_icc_profile (guint id, const gchar *filename, GError **error)
{
	gboolean ret;
	gchar *data = NULL;
	gsize length;

	g_return_val_if_fail (filename != NULL, FALSE);

	/* get contents of file */
	ret = g_file_get_contents (filename, &data, &length, error);
	if (!ret)
		goto out;

	/* send to the XServer */
	ret = gcm_utils_set_x11_icc_profile_data (id, (const guint8 *) data, length, error);
	if (!ret)
		goto out;
out:
	g_free (data);
	return ret;
}

/**
 * gcm_utils_set_x11_icc_profile_data:
 * @id: the ID that is used according to ICC Profiles In X Specification
 * @data: the data that is to be set to the XServer
 * @length: the size of the data
 * @error: a %GError that is set in the result of an error, or %NULL
 * Return value: %TRUE for success.
 *
 * Sets the ICC profile data to the XServer.
 *
 * TODO: the ICC Profiles In X Specification is very vague about how the id
 * map to the RROutput name or ID. Seek clarification.
 **/
gboolean
gcm_utils_set_x11_icc_profile_data (guint id, const guint8 *data, gsize length, GError **error)
{
	gboolean ret = FALSE;
	gchar *atom_name;
	gint rc;
	Atom atom = None;
	Display	*display;
	GdkDisplay *display_gdk;
	GdkWindow *window_gdk;
	Window window;

	g_return_val_if_fail (data != NULL, FALSE);
	g_return_val_if_fail (length != 0, FALSE);

	/* get defaults for single screen */
	display_gdk = gdk_display_get_default ();
	window_gdk = gdk_get_default_root_window ();
	display = GDK_DISPLAY_XDISPLAY (display_gdk);
	window = GDK_WINDOW_XID (window_gdk);

	/* get the atom name */
	if (id == 0)
		atom_name = g_strdup ("_ICC_PROFILE");
	else
		atom_name = g_strdup_printf ("_ICC_PROFILE_%i", id);

	/* get the value */
	gdk_error_trap_push ();
	atom = gdk_x11_get_xatom_by_name_for_display (display_gdk, atom_name);
	rc = XChangeProperty (display, window, atom, XA_CARDINAL, 8, PropModeReplace, (unsigned char*) data, length);
	gdk_error_trap_pop ();

	/* for some reason this fails with BadRequest, but actually sets the value */
	if (rc == BadRequest)
		rc = Success;

	/* did the call fail */
	if (rc != Success) {
		if (error != NULL)
			*error = g_error_new (1, 0, "failed to set %s atom with rc %i", atom_name, rc);
		goto out;
	}

	/* success */
	ret = TRUE;
out:
	g_free (atom_name);
	return ret;
}


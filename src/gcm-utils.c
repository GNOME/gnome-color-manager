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
#include <libgnomeui/gnome-rr.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/xf86vmode.h>
#include <gconf/gconf-client.h>

#include "gcm-utils.h"
#include "gcm-clut.h"
#include "gcm-xserver.h"

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
 * gcm_utils_get_gamma_size_fallback:
 **/
static guint
gcm_utils_get_gamma_size_fallback (void)
{
	guint size;
	Bool rc;

	/* this is per-screen, not per output which is less than ideal */
	gdk_error_trap_push ();
	egg_warning ("using PER-SCREEN gamma tables as driver is not XRANDR 1.3 compliant");
	rc = XF86VidModeGetGammaRampSize (GDK_DISPLAY(), gdk_x11_get_default_screen (), (int*) &size);
	gdk_error_trap_pop ();
	if (!rc)
		size = 0;

	return size;
}

/**
 * gcm_utils_get_gamma_size:
 *
 * Return value: the gamma size, or 0 if error;
 **/
static guint
gcm_utils_get_gamma_size (GnomeRRCrtc *crtc, GError **error)
{
	guint id;
	guint size;

	/* get id that X recognises */
	id = gnome_rr_crtc_get_id (crtc);

	/* get the value, and catch errors */
	gdk_error_trap_push ();
	size = XRRGetCrtcGammaSize (GDK_DISPLAY(), id);
	if (gdk_error_trap_pop ())
		size = 0;

	/* some drivers support Xrandr 1.2, not 1.3 */
	if (size == 0)
		size = gcm_utils_get_gamma_size_fallback ();

	/* no size, or X popped an error */
	if (size == 0) {
		if (error != NULL)
			*error = g_error_new (1, 0, "failed to get gamma size");
		goto out;
	}
out:
	return size;
}

/**
 * gcm_utils_set_gamma_fallback:
 **/
static gboolean
gcm_utils_set_gamma_fallback (XRRCrtcGamma *gamma, guint size)
{
	Bool rc;

	/* this is per-screen, not per output which is less than ideal */
	gdk_error_trap_push ();
	egg_warning ("using PER-SCREEN gamma tables as driver is not XRANDR 1.3 compliant");
	rc = XF86VidModeSetGammaRamp (GDK_DISPLAY(), gdk_x11_get_default_screen (), size, gamma->red, gamma->green, gamma->blue);
	gdk_error_trap_pop ();

	return rc;
}

/**
 * gcm_utils_set_gamma_for_crtc:
 *
 * Return value: %TRUE for success;
 **/
static gboolean
gcm_utils_set_gamma_for_crtc (GnomeRRCrtc *crtc, GcmClut *clut, GError **error)
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
		/* some drivers support Xrandr 1.2, not 1.3 */
		ret = gcm_utils_set_gamma_fallback (gamma, array->len);
		if (!ret) {
			if (error != NULL)
				*error = g_error_new (1, 0, "failed to set crtc gamma %p (%i) on %i", gamma, array->len, id);
			goto out;
		}
	}
out:
	if (gamma != NULL)
		XRRFreeGamma (gamma);
	if (array != NULL)
		g_ptr_array_unref (array);
	return ret;
}

/**
 * gcm_utils_set_gamma_for_device:
 *
 * Return value: %TRUE for success;
 **/
gboolean
gcm_utils_set_gamma_for_device (GcmDevice *device, GError **error)
{
	gboolean ret = FALSE;
	GcmClut *clut = NULL;
	GcmXserver *xserver = NULL;
	GnomeRRCrtc *crtc;
	GnomeRROutput *output;
	gint x, y;
	gchar *profile = NULL;
	gfloat gamma;
	gfloat brightness;
	gfloat contrast;
	gchar *output_name;
	gchar *id = NULL;
	guint size;
	gboolean use_global;
	gboolean use_atom;
	gboolean leftmost_screen = FALSE;
	GcmDeviceType type;
	GnomeRRScreen *rr_screen = NULL;
	GConfClient *gconf_client = NULL;

	/* use gconf to decide to set LUT or set ATOMs */
	gconf_client = gconf_client_get_default ();

	/* get details about the device */
	g_object_get (device,
		      "id", &id,
		      "type", &type,
		      "profile-filename", &profile,
		      "gamma", &gamma,
		      "brightness", &brightness,
		      "contrast", &contrast,
		      "native-device-xrandr", &output_name,
		      NULL);

	/* do no set the gamma for non-display types */
	if (type != GCM_DEVICE_TYPE_DISPLAY) {
		if (error != NULL)
			*error = g_error_new (1, 0, "not a display: %s", id);
		goto out;
	}

	/* should be set for display types */
	if (output_name == NULL) {
		if (error != NULL)
			*error = g_error_new (1, 0, "no output name for display: %s", id);
		goto out;
	}

	/* check we have an output */
	rr_screen = gnome_rr_screen_new (gdk_screen_get_default (), NULL, NULL, error);
	if (rr_screen == NULL)
		goto out;
	output = gnome_rr_screen_get_output_by_name (rr_screen, output_name);
	if (output == NULL) {
		if (error != NULL)
			*error = g_error_new (1, 0, "no output for device: %s [%s]", id, output_name);
		goto out;
	}

	/* get crtc size */
	crtc = gnome_rr_output_get_crtc (output);
	if (crtc == NULL) {
		if (error != NULL)
			*error = g_error_new (1, 0, "failed to get crtc for device: %s", id);
		goto out;
	}

	/* get gamma table size */
	size = gcm_utils_get_gamma_size (crtc, error);
	if (size == 0)
		goto out;

	/* create CLUT */
	clut = gcm_clut_new ();

	/* only set the CLUT if we're not seting the atom */
	use_global = gconf_client_get_bool (gconf_client, GCM_SETTINGS_GLOBAL_DISPLAY_CORRECTION, NULL);
	if (use_global) {
		g_object_set (clut,
			      "profile", profile,
			      "gamma", gamma,
			      "brightness", brightness,
			      "contrast", contrast,
			      "size", size,
			      NULL);

		/* load this new profile */
		ret = gcm_clut_load_from_profile (clut, error);
		if (!ret)
			goto out;
	} else {
		/* we're using a dummy clut */
		g_object_set (clut,
			      "size", size,
			      NULL);
	}

	/* actually set the gamma */
	ret = gcm_utils_set_gamma_for_crtc (crtc, clut, error);
	if (!ret)
		goto out;

	/* set per output and per-screen atoms */
	xserver = gcm_xserver_new ();

	/* is the monitor our primary monitor */
	gnome_rr_output_get_position (output, &x, &y);
	leftmost_screen = (x == 0 && y == 0);

	/* either remove the atoms or set them */
	use_atom = gconf_client_get_bool (gconf_client, GCM_SETTINGS_SET_ICC_PROFILE_ATOM, NULL);
	if (!use_atom || profile == NULL) {

		/* remove the output atom if there's nothing to show */
		ret = gcm_xserver_remove_output_profile (xserver, output_name, error);
		if (!ret)
			goto out;

		/* primary screen */
		if (leftmost_screen) {
			ret = gcm_xserver_remove_root_window_profile (xserver, error);
			if (!ret)
				goto out;
		}
	} else {
		/* set the per-output and per screen profile atoms */
		ret = gcm_xserver_set_output_profile (xserver, output_name, profile, error);
		if (!ret)
			goto out;

		/* primary screen */
		if (leftmost_screen) {
			ret = gcm_xserver_set_root_window_profile (xserver, profile, error);
			if (!ret)
				goto out;
		}
	}
out:
	g_free (id);
	g_free (profile);
	g_free (output_name);
	if (gconf_client != NULL)
		g_object_unref (gconf_client);
	if (rr_screen != NULL)
		gnome_rr_screen_destroy (rr_screen);
	if (clut != NULL)
		g_object_unref (clut);
	if (xserver != NULL)
		g_object_unref (xserver);
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
	gchar *full_path;

	/* get contents */
	dir = g_dir_open (directory, 0, &error);
	if (dir == NULL) {
		egg_debug ("failed to open: %s", error->message);
		g_error_free (error);
		ret = FALSE;
		goto out;
	}

	/* process entire list */
	do {
		name = g_dir_read_name (dir);
		if (name == NULL)
			break;

		/* make the compete path */
		full_path = g_build_filename (directory, name, NULL);
		if (g_file_test (full_path, G_FILE_TEST_IS_DIR)) {
			egg_debug ("recursing to %s", full_path);
			gcm_utils_get_profile_filenames_for_directory (array, full_path);
		} else if (g_str_has_suffix (name, ".icc") ||
			   g_str_has_suffix (name, ".icm") ||
			   g_str_has_suffix (name, ".ICC") ||
			   g_str_has_suffix (name, ".ICM")) {
			/* add to array */
			g_ptr_array_add (array, g_strdup (full_path));
		} else {
			/* invalid file */
			egg_debug ("not recognised as ICC profile: %s", full_path);
		}
		g_free (full_path);
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
	gcm_utils_get_profile_filenames_for_directory (array, "/usr/local/share/color/icc");

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
 * gcm_utils_get_default_config_location:
 **/
gchar *
gcm_utils_get_default_config_location (void)
{
	gchar *filename;

	/* create default path */
	filename = g_build_filename (g_get_user_config_dir (), "gnome-color-manager", "config.dat", NULL);

	return filename;
}


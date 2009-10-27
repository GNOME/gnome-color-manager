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

#include "gcm-utils.h"
#include "egg-debug.h"

/**
 * gcm_utils_get_edid_name:
 *
 * Return value: the output name, free with g_free().
 **/
static gchar *
gcm_utils_get_edid_name (const guint8 *edid)
{
	guint i;
	gchar *monitor = NULL;
	gchar *serial = NULL;
	gchar *string = NULL;
	gchar *retval = NULL;

	/* parse EDID data */
	for (i=54; i <= 108; i+=18) {
		/* ignore pixel clock data */
		if (edid[i] != 0)
			continue;
		if (edid[i+2] != 0)
			continue;

		/* any useful blocks? */
		if (edid[i+3] == 0xfc)
			monitor = g_strdup (&edid[i+5]);
		else if (edid[i+3] == 0xff)
			serial = g_strdup (&edid[i+5]);
		else if (edid[i+3] == 0xfe)
			string = g_strdup (&edid[i+5]);
	}

	/* find the best option */
	if (monitor != NULL)
		retval = g_strdup (monitor);
	else if (serial != NULL)
		retval = g_strdup (serial);
	else if (string != NULL)
		retval = g_strdup (string);
	g_free (monitor);
	g_free (serial);
	g_free (string);

	/* replace invalid chars */
	if (retval != NULL) {
		g_strdelimit (retval, "-", '_');
		g_strdelimit (retval, "\n", '\0');
	}
	return retval;
}

/**
 * gcm_utils_get_output_name:
 *
 * Return value: the output name, free with g_free().
 **/
gchar *
gcm_utils_get_output_name (GnomeRROutput *output)
{
	const guint8 *edid;
	guint i, j;
	const gchar *output_name;
	gchar *name;

	/* TODO: need to parse the EDID to get a crtc-specific name, not an output specific name */
	edid = gnome_rr_output_get_edid_data (output);
	name = gcm_utils_get_edid_name (edid);

	/* fallback to the output name */
	if (name == NULL) {
		output_name = gnome_rr_output_get_name (output);
		if (g_strstr_len (output_name, -1, "LVDS") != NULL)
			output_name = _("Internal LCD");
		name = g_strdup (output_name);
	}

	/* for now, use the output name */
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

	/* get CLUT */
	clut = gcm_utils_get_clut_for_output (output, error);
	if (clut == NULL)
		goto out;

	/* get crtc */
	crtc = gnome_rr_output_get_crtc (output);

	/* actually set the gamma */
	ret = gcm_utils_set_crtc_gamma (crtc, clut, error);
	if (!ret)
		goto out;
out:
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

	/* get per-user profiles */
	user = g_build_filename (g_get_home_dir (), "/.color/icc", NULL);
	gcm_utils_get_profile_filenames_for_directory (array, user);
	g_free (user);

	return array;
}


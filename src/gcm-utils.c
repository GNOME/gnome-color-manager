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

#include <glib-object.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include "gcm-utils.h"
#include "egg-debug.h"

/**
 * gcm_utils_get_output_name:
 *
 * Return value: the output name, free with g_free().
 **/
gchar *
gcm_utils_get_output_name (GnomeRROutput *output)
{
//	const guint8 *edid;
//	guint i;
	gchar *name;

	/* TODO: need to parse the EDID to get a crtc-specific name, not an output specific name */
//	edid = gnome_rr_output_get_edid_data (output);
//	for (i=0; i<127; i++)
//		egg_debug ("edid: %i: %x [%c]", i, edid[i], edid[i]);

	/* for now, use the output name */
	name = g_strdup (gnome_rr_output_get_name (output));
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
			*error = g_error_new (1, 0, "failed to get gamma size");
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
 * gcm_utils_set_output_gamma:
 *
 * Return value: %TRUE for success;
 **/
gboolean
gcm_utils_set_output_gamma (GnomeRROutput *output, const gchar *config, GError **error)
{
	gboolean ret = FALSE;
	gchar *name = NULL;
	gboolean connected;
	GnomeRRCrtc *crtc;
	guint size;
	guint id;
	GcmClut *clut = NULL;

	/* don't set the gamma if there is no device */
	connected = gnome_rr_output_is_connected (output);
	if (!connected)
		goto out;

	/* get data */
	name = gcm_utils_get_output_name (output);
	egg_debug ("[%i] output %p (name=%s)", connected, output, name);

	/* get crtc */
	crtc = gnome_rr_output_get_crtc (output);
	id = gnome_rr_crtc_get_id (crtc);

	/* get gamma size */
	size = gcm_utils_get_gamma_size (crtc, error);
	if (size == 0)
		goto out;
		
	/* create a lookup table */
	clut = gcm_clut_new ();
	g_object_set (clut,
		      "size", size,
		      NULL);

	/* lookup from config file */
	ret = gcm_clut_load_from_config (clut, config, name, error);
	if (!ret)
		goto out;

	/* actually set the gamma */
	ret = gcm_utils_set_crtc_gamma (crtc, clut, error);
	if (!ret)
		goto out;
out:
	if (clut != NULL)
		g_object_unref (clut);
	g_free (name);
	return ret;
}


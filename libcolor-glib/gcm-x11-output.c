/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2007-2008 Soren Sandmann <sandmann@redhat.com>
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

/**
 * SECTION:gcm-x11-output
 * @short_description: Object to interact with the XServer
 *
 * This object talks to the currently running X Server.
 */

#include "config.h"

#include <glib-object.h>
#include <X11/extensions/Xrandr.h>
#include <X11/Xatom.h>
#include <string.h>
#include <gdk/gdk.h>

#include "gcm-x11-output.h"

#include "egg-debug.h"

static void     gcm_x11_output_finalize	(GObject     *object);

#define GCM_X11_OUTPUT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GCM_TYPE_X11_OUTPUT, GcmX11OutputPrivate))

/**
 * GcmX11OutputPrivate:
 *
 * Private #GcmX11Output data
 **/
struct _GcmX11OutputPrivate
{
	gchar				*display_name;
	Display				*display;
	gchar				*name;
	guint				 id;
	guint				 crtc_id;
	gboolean			 primary;
	guint				 gamma_size;
	gboolean			 connected;
	guint				 x;
	guint				 y;
	guint				 width;
	guint				 height;
};

enum {
	PROP_0,
	PROP_DISPLAY_NAME,
	PROP_LAST
};

G_DEFINE_TYPE (GcmX11Output, gcm_x11_output, G_TYPE_OBJECT)

/**
 * gcm_x11_output_set_name:
 **/
void
gcm_x11_output_set_display (GcmX11Output *output, gpointer display)
{
	g_return_if_fail (GCM_IS_X11_OUTPUT (output));
	output->priv->display = display;
}

/**
 * gcm_x11_output_set_name:
 **/
void
gcm_x11_output_set_name (GcmX11Output *output, const gchar *name)
{
	g_return_if_fail (GCM_IS_X11_OUTPUT (output));
	output->priv->name = g_strdup (name);
}

/**
 * gcm_x11_output_get_name:
 **/
const gchar *
gcm_x11_output_get_name (GcmX11Output *output)
{
	g_return_val_if_fail (GCM_IS_X11_OUTPUT (output), NULL);
	return output->priv->name;
}

/**
 * gcm_x11_output_set_id:
 **/
void
gcm_x11_output_set_id (GcmX11Output *output, guint id)
{
	g_return_if_fail (GCM_IS_X11_OUTPUT (output));
	output->priv->id = id;
}

/**
 * gcm_x11_output_get_id:
 **/
guint
gcm_x11_output_get_id (GcmX11Output *output)
{
	g_return_val_if_fail (GCM_IS_X11_OUTPUT (output), 0);
	return output->priv->id;
}

/**
 * gcm_x11_output_set_crtc_id:
 **/
void
gcm_x11_output_set_crtc_id (GcmX11Output *output, guint crtc_id)
{
	g_return_if_fail (GCM_IS_X11_OUTPUT (output));
	output->priv->crtc_id = crtc_id;
}

/**
 * gcm_x11_output_get_crtc_id:
 **/
guint
gcm_x11_output_get_crtc_id (GcmX11Output *output)
{
	g_return_val_if_fail (GCM_IS_X11_OUTPUT (output), 0);
	return output->priv->crtc_id;
}

/**
 * gcm_x11_output_set_gamma_size:
 **/
void
gcm_x11_output_set_gamma_size (GcmX11Output *output, guint gamma_size)
{
	g_return_if_fail (GCM_IS_X11_OUTPUT (output));
	output->priv->gamma_size = gamma_size;
}

/**
 * gcm_x11_output_get_gamma_size:
 **/
guint
gcm_x11_output_get_gamma_size (GcmX11Output *output)
{
	g_return_val_if_fail (GCM_IS_X11_OUTPUT (output), 0);
	return output->priv->gamma_size;
}

/**
 * gcm_x11_output_set_primary:
 **/
void
gcm_x11_output_set_primary (GcmX11Output *output, gboolean primary)
{
	g_return_if_fail (GCM_IS_X11_OUTPUT (output));
	output->priv->primary = primary;
}

/**
 * gcm_x11_output_get_primary:
 **/
gboolean
gcm_x11_output_get_primary (GcmX11Output *output)
{
	g_return_val_if_fail (GCM_IS_X11_OUTPUT (output), FALSE);
	return output->priv->primary;
}

/**
 * gcm_x11_output_set_connected:
 **/
void
gcm_x11_output_set_connected (GcmX11Output *output, gboolean connected)
{
	g_return_if_fail (GCM_IS_X11_OUTPUT (output));
	output->priv->connected = connected;
}

/**
 * gcm_x11_output_get_connected:
 **/
gboolean
gcm_x11_output_get_connected (GcmX11Output *output)
{
	g_return_val_if_fail (GCM_IS_X11_OUTPUT (output), FALSE);
	return output->priv->connected;
}

/**
 * gcm_x11_output_set_position:
 **/
void
gcm_x11_output_set_position (GcmX11Output *output, guint x, guint y)
{
	g_return_if_fail (GCM_IS_X11_OUTPUT (output));
	output->priv->x = x;
	output->priv->y = y;
}

/**
 * gcm_x11_output_get_position:
 **/
void
gcm_x11_output_get_position (GcmX11Output *output, guint *x, guint *y)
{
	g_return_if_fail (GCM_IS_X11_OUTPUT (output));
	if (x != NULL)
		*x = output->priv->x;
	if (y != NULL)
		*y = output->priv->y;
}

/**
 * gcm_x11_output_set_size:
 **/
void
gcm_x11_output_set_size (GcmX11Output *output, guint width, guint height)
{
	g_return_if_fail (GCM_IS_X11_OUTPUT (output));
	output->priv->width = width;
	output->priv->height = height;
}

/**
 * gcm_x11_output_get_size:
 **/
void
gcm_x11_output_get_size (GcmX11Output *output, guint *width, guint *height)
{
	g_return_if_fail (GCM_IS_X11_OUTPUT (output));
	if (width != NULL)
		*width = output->priv->width;
	if (height != NULL)
		*height = output->priv->height;
}

/**
 * gcm_x11_output_get_property_atom:
 **/
static guint8 *
gcm_x11_output_get_property_atom (GcmX11Output *output, Atom atom, gint *len)
{
	guchar *prop;
	gint actual_format;
	unsigned long nitems, bytes_after;
	Atom actual_type;
	guint8 *result = NULL;

	/* get a property on the output */
	gdk_error_trap_push ();
	XRRGetOutputProperty (output->priv->display, output->priv->id, atom,
			      0, 100, False, False,
			      AnyPropertyType,
			      &actual_type, &actual_format,
			      &nitems, &bytes_after, &prop);
	gdk_flush ();
	if (gdk_error_trap_pop ())
		goto out;
	if (actual_type == XA_INTEGER && actual_format == 8) {
		result = g_memdup (prop, nitems);
		if (len)
			*len = nitems;
	}
	XFree (prop);
out:
	return result;
}

/**
 * gcm_x11_output_get_edid_data:
 **/
gboolean
gcm_x11_output_get_edid_data (GcmX11Output *output, guint8 **data, gsize *length, GError **error)
{
	Atom edid_atom;
	guint8 *result;
	gint len;
	gboolean ret = FALSE;

	g_return_val_if_fail (GCM_IS_X11_OUTPUT (output), FALSE);
	g_return_val_if_fail (output->priv->display != NULL, FALSE);

	/* get the new name */
	edid_atom = XInternAtom (output->priv->display, "EDID", FALSE);
	result = gcm_x11_output_get_property_atom (output, edid_atom, &len);

	/* try harder */
	if (result == NULL) {
		edid_atom = XInternAtom (output->priv->display, "EDID_DATA", FALSE);
		result = gcm_x11_output_get_property_atom (output, edid_atom, &len);
	}

	/* failed */
	if (result == NULL) {
		g_set_error_literal (error, 1, 0, "no edid data");
		goto out;
	}

	/* success */
	if (data != NULL)
		*data = result;
	ret = TRUE;
out:
	return ret;
}

/**
 * gcm_x11_output_set_gamma:
 * @output: a valid %GcmX11Output instance
 * @error: a %GError, or %NULL
 *
 * Sets the gamma ramps for the given output.
 *
 * Return value: %TRUE for success.
 **/
gboolean
gcm_x11_output_set_gamma (GcmX11Output *output, guint size, guint16 *red, guint16 *green, guint16 *blue, GError **error)
{
	guint copy_size;
	XRRCrtcGamma *gamma;

	g_return_val_if_fail (GCM_IS_X11_OUTPUT (output), FALSE);
	g_return_val_if_fail (output->priv->display != NULL, FALSE);
	g_return_val_if_fail (red != NULL, FALSE);
	g_return_val_if_fail (green != NULL, FALSE);
	g_return_val_if_fail (blue != NULL, FALSE);

	return TRUE;

	if (size != output->priv->gamma_size)
		return FALSE;

	gamma = XRRAllocGamma (output->priv->gamma_size);
	copy_size = output->priv->gamma_size * sizeof (guint16);
	memcpy (gamma->red, red, copy_size);
	memcpy (gamma->green, green, copy_size);
	memcpy (gamma->blue, blue, copy_size);

	gdk_error_trap_push ();
	XRRSetCrtcGamma (output->priv->display, output->priv->crtc_id, gamma);
	XRRFreeGamma (gamma);
	gdk_flush ();
	if (gdk_error_trap_pop ()) {
		g_set_error_literal (error,
				     GCM_X11_OUTPUT_ERROR, GCM_X11_OUTPUT_ERROR_INTERNAL,
				     "Failed to set gamma");
		return FALSE;
	}
	return TRUE;
}

/**
 * gcm_x11_output_get_gamma:
 **/
gboolean
gcm_x11_output_get_gamma (GcmX11Output *output, guint *size, guint16 **red, guint16 **green, guint16 **blue, GError **error)
{
	guint copy_size;
	guint16 *r, *g, *b;
	XRRCrtcGamma *gamma;

	g_return_val_if_fail (GCM_IS_X11_OUTPUT (output), FALSE);
	g_return_val_if_fail (output->priv->display != NULL, FALSE);

	gdk_error_trap_push ();
	gamma = XRRGetCrtcGamma (output->priv->display, output->priv->crtc_id);
	gdk_flush ();
	if (gdk_error_trap_pop () || gamma == NULL) {
		g_set_error_literal (error,
				     GCM_X11_OUTPUT_ERROR, GCM_X11_OUTPUT_ERROR_INTERNAL,
				     "Failed to get gamma");
		return FALSE;
	}

	copy_size = output->priv->gamma_size * sizeof (guint16);
	if (red != NULL) {
		r = g_new0 (guint16, output->priv->gamma_size);
		memcpy (r, gamma->red, copy_size);
		*red = r;
	}
	if (green != NULL) {
		g = g_new0 (guint16, output->priv->gamma_size);
		memcpy (g, gamma->green, copy_size);
		*green = g;
	}
	if (blue != NULL) {
		b = g_new0 (guint16, output->priv->gamma_size);
		memcpy (b, gamma->blue, copy_size);
		*blue = b;
	}
	if (size)
		*size = output->priv->gamma_size;
	XRRFreeGamma (gamma);
	return TRUE;
}


/**
 * gcm_x11_output_get_profile_data:
 *
 * @x11: a valid %GcmX11Output instance
 * @data: the data that is returned from the XServer. Free with g_free()
 * @length: the size of the returned data, or %NULL if you don't care
 * @error: a %GError that is set in the result of an error, or %NULL
 *
 * Gets the ICC profile data from the specified output.
 *
 * Return value: %TRUE for success.
 *
 * Since: 0.0.1
 **/
gboolean
gcm_x11_output_get_profile_data (GcmX11Output *output, guint8 **data, gsize *length, GError **error)
{
	gboolean ret = FALSE;
	gchar *data_tmp = NULL;
	gint format;
	gint rc = -1;
	gulong bytes_after;
	gulong nitems = 0;
	Atom atom;
	Atom type;
	GcmX11OutputPrivate *priv = output->priv;

	g_return_val_if_fail (GCM_IS_X11_OUTPUT (output), FALSE);
	g_return_val_if_fail (data != NULL, FALSE);

	/* get the value */
	gdk_error_trap_push ();
	atom = XInternAtom (priv->display, "_ICC_PROFILE", FALSE);
	rc = XRRGetOutputProperty (priv->display, priv->id,
				   atom, 0, ~0, False, False,
				   AnyPropertyType, &type, &format, &nitems, &bytes_after,
				   (unsigned char **) &data_tmp);
	egg_debug ("got %i bytes", (guint) nitems);
	gdk_error_trap_pop ();

	/* did the call fail */
	if (rc != Success) {
		g_set_error (error, 1, 0, "failed to get icc profile atom with rc %i", rc);
		goto out;
	}

	/* was nothing found */
	if (nitems == 0) {
		g_set_error (error, 1, 0, "icc profile atom has not been set");
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
	if (data_tmp != NULL)
		XFree (data_tmp);
	return ret;
}

/**
 * gcm_x11_output_set_profile:
 * @x11: a valid %GcmX11Output instance
 * @filename: the filename of the ICC profile
 * @error: a %GError that is set in the result of an error, or %NULL
 *
 * Sets the ICC profile data to the specified output.
 *
 * Return value: %TRUE for success.
 *
 * Since: 0.0.1
 **/
gboolean
gcm_x11_output_set_profile (GcmX11Output *output, const gchar *filename, GError **error)
{
	gboolean ret;
	gchar *data = NULL;
	gsize length;

	g_return_val_if_fail (GCM_IS_X11_OUTPUT (output), FALSE);
	g_return_val_if_fail (filename != NULL, FALSE);

	egg_debug ("setting output ICC profile atom from %s", filename);

	/* get contents of file */
	ret = g_file_get_contents (filename, &data, &length, error);
	if (!ret)
		goto out;

	/* send to the XServer */
	ret = gcm_x11_output_set_profile_data (output, (const guint8 *) data, length, error);
	if (!ret)
		goto out;
out:
	g_free (data);
	return ret;
}

/**
 * gcm_x11_output_set_profile_data:
 * @x11: a valid %GcmX11Output instance
 * @data: the data that is to be set to the XServer
 * @length: the size of the data
 * @error: a %GError that is set in the result of an error, or %NULL
 *
 * Sets the ICC profile data to the specified output.
 *
 * Return value: %TRUE for success.
 *
 * Since: 0.0.1
 **/
gboolean
gcm_x11_output_set_profile_data (GcmX11Output *output, const guint8 *data, gsize length, GError **error)
{
	gboolean ret = FALSE;
	gint rc;
	Atom atom = None;
	GcmX11OutputPrivate *priv = output->priv;

	g_return_val_if_fail (GCM_IS_X11_OUTPUT (output), FALSE);
	g_return_val_if_fail (data != NULL, FALSE);
	g_return_val_if_fail (length != 0, FALSE);

	/* get the value */
	gdk_error_trap_push ();
	atom = XInternAtom (priv->display, "_ICC_PROFILE", FALSE);
	XRRChangeOutputProperty (priv->display, priv->id,
				 atom, XA_CARDINAL, 8,
				 PropModeReplace,
				 (unsigned char*) data, (gint)length);
	rc = gdk_error_trap_pop ();

	/* did the call fail */
	if (rc != Success) {
		g_set_error (error, 1, 0, "failed to set output icc profile atom with rc %i", rc);
		goto out;
	}

	/* success */
	ret = TRUE;
out:
	return ret;
}

/**
 * gcm_x11_output_remove_profile:
 * @x11: a valid %GcmX11Output instance
 * @error: a %GError that is set in the result of an error, or %NULL
 *
 * Sets the ICC profile data to the specified output.
 *
 * Return value: %TRUE for success.
 *
 * Since: 0.0.1
 **/
gboolean
gcm_x11_output_remove_profile (GcmX11Output *output, GError **error)
{
	gboolean ret = FALSE;
	gint rc;
	Atom atom;
	GcmX11OutputPrivate *priv = output->priv;

	g_return_val_if_fail (GCM_IS_X11_OUTPUT (output), FALSE);

	/* get the value */
	gdk_error_trap_push ();
	atom = XInternAtom (priv->display, "_ICC_PROFILE", FALSE);
	XRRDeleteOutputProperty (priv->display, priv->id, atom);
	rc = gdk_error_trap_pop ();

	/* did the call fail */
	if (rc != Success) {
		g_set_error (error, 1, 0, "failed to remove output icc profile atom with rc %i", rc);
		goto out;
	}

	/* success */
	ret = TRUE;
out:
	return ret;
}

/**
 * gcm_x11_output_get_property:
 **/
static void
gcm_x11_output_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GcmX11Output *output = GCM_X11_OUTPUT (object);
	GcmX11OutputPrivate *priv = output->priv;

	switch (prop_id) {
	case PROP_DISPLAY_NAME:
		g_value_set_string (value, priv->display_name);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gcm_x11_output_set_property:
 **/
static void
gcm_x11_output_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	GcmX11Output *output = GCM_X11_OUTPUT (object);
	GcmX11OutputPrivate *priv = output->priv;

	switch (prop_id) {
	case PROP_DISPLAY_NAME:
		g_free (priv->display_name);
		priv->display_name = g_strdup (g_value_get_string (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gcm_x11_output_class_init:
 **/
static void
gcm_x11_output_class_init (GcmX11OutputClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gcm_x11_output_finalize;
	object_class->get_property = gcm_x11_output_get_property;
	object_class->set_property = gcm_x11_output_set_property;

	/**
	 * GcmX11Output:display-name:
	 */
	pspec = g_param_spec_string ("display-name", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_DISPLAY_NAME, pspec);

	g_type_class_add_private (klass, sizeof (GcmX11OutputPrivate));
}

/**
 * gcm_x11_output_init:
 **/
static void
gcm_x11_output_init (GcmX11Output *output)
{
	output->priv = GCM_X11_OUTPUT_GET_PRIVATE (output);
	output->priv->display_name = NULL;
}

/**
 * gcm_x11_output_finalize:
 **/
static void
gcm_x11_output_finalize (GObject *object)
{
	GcmX11Output *output = GCM_X11_OUTPUT (object);
	GcmX11OutputPrivate *priv = output->priv;

	g_free (priv->display_name);

	G_OBJECT_CLASS (gcm_x11_output_parent_class)->finalize (object);
}

/**
 * gcm_x11_output_new:
 *
 * Return value: a new GcmX11Output object.
 *
 * Since: 0.0.1
 **/
GcmX11Output *
gcm_x11_output_new (void)
{
	GcmX11Output *output;
	output = g_object_new (GCM_TYPE_X11_OUTPUT, NULL);
	return GCM_X11_OUTPUT (output);
}


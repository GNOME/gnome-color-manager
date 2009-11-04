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
 * GNU General Public License for more xserver.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:gcm-xserver
 * @short_description: Object to interact with the XServer
 *
 * This object talks to the currently running X Server.
 */

#include "config.h"

#include <glib-object.h>
#include <math.h>
#include <string.h>
#include <gio/gio.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xrandr.h>

#include "gcm-xserver.h"

#include "egg-debug.h"

static void     gcm_xserver_finalize	(GObject     *object);

#define GCM_XSERVER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GCM_TYPE_XSERVER, GcmXserverPrivate))

/**
 * GcmXserverPrivate:
 *
 * Private #GcmXserver data
 **/
struct _GcmXserverPrivate
{
	gchar				*display_name;
	GdkDisplay			*display_gdk;
	GdkWindow			*window_gdk;
	Display				*display;
	Window				 window;
};

enum {
	PROP_0,
	PROP_DISPLAY_NAME,
	PROP_LAST
};

G_DEFINE_TYPE (GcmXserver, gcm_xserver, G_TYPE_OBJECT)

/**
 * gcm_xserver_get_root_window_profile_data:
 *
 * @xserver: a valid %GcmXserver instance
 * @data: the data that is returned from the XServer. Free with g_free()
 * @length: the size of the returned data, or %NULL if you don't care
 * @error: a %GError that is set in the result of an error, or %NULL
 * Return value: %TRUE for success.
 *
 * Gets the ICC profile data from the XServer.
 **/
gboolean
gcm_xserver_get_root_window_profile_data (GcmXserver *xserver, guint8 **data, gsize *length, GError **error)
{
	gboolean ret = FALSE;
	const gchar *atom_name;
	gchar *data_tmp = NULL;
	gint format;
	gint rc;
	gulong bytes_after;
	gulong nitems;
	Atom atom = None;
	Atom type;
	GcmXserverPrivate *priv = xserver->priv;

	g_return_val_if_fail (GCM_IS_XSERVER (xserver), FALSE);
	g_return_val_if_fail (data != NULL, FALSE);

	/* get the atom name */
	atom_name = "_ICC_PROFILE";

	/* get the value */
	gdk_error_trap_push ();
	atom = gdk_x11_get_xatom_by_name_for_display (priv->display_gdk, atom_name);
	rc = XGetWindowProperty (priv->display, priv->window, atom, 0, G_MAXLONG, False, XA_CARDINAL,
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
	if (data_tmp != NULL)
		XFree (data_tmp);
	return ret;
}

/**
 * gcm_xserver_set_root_window_profile:
 * @xserver: a valid %GcmXserver instance
 * @filename: the filename of the ICC profile
 * @error: a %GError that is set in the result of an error, or %NULL
 * Return value: %TRUE for success.
 *
 * Sets the ICC profile data to the XServer.
 **/
gboolean
gcm_xserver_set_root_window_profile (GcmXserver *xserver, const gchar *filename, GError **error)
{
	gboolean ret;
	gchar *data = NULL;
	gsize length;

	g_return_val_if_fail (GCM_IS_XSERVER (xserver), FALSE);
	g_return_val_if_fail (filename != NULL, FALSE);

	egg_debug ("setting root window ICC profile atom from %s", filename);

	/* get contents of file */
	ret = g_file_get_contents (filename, &data, &length, error);
	if (!ret)
		goto out;

	/* send to the XServer */
	ret = gcm_xserver_set_root_window_profile_data (xserver, (const guint8 *) data, length, error);
	if (!ret)
		goto out;
out:
	g_free (data);
	return ret;
}

/**
 * gcm_xserver_set_root_window_profile_data:
 * @xserver: a valid %GcmXserver instance
 * @data: the data that is to be set to the XServer
 * @length: the size of the data
 * @error: a %GError that is set in the result of an error, or %NULL
 * Return value: %TRUE for success.
 *
 * Sets the ICC profile data to the XServer.
 **/
gboolean
gcm_xserver_set_root_window_profile_data (GcmXserver *xserver, const guint8 *data, gsize length, GError **error)
{
	gboolean ret = FALSE;
	const gchar *atom_name;
	gint rc;
	Atom atom = None;
	GcmXserverPrivate *priv = xserver->priv;

	g_return_val_if_fail (GCM_IS_XSERVER (xserver), FALSE);
	g_return_val_if_fail (data != NULL, FALSE);
	g_return_val_if_fail (length != 0, FALSE);

	/* get the atom name */
	atom_name = "_ICC_PROFILE";

	/* get the value */
	gdk_error_trap_push ();
	atom = gdk_x11_get_xatom_by_name_for_display (priv->display_gdk, atom_name);
	rc = XChangeProperty (priv->display, priv->window, atom, XA_CARDINAL, 8, PropModeReplace, (unsigned char*) data, length);
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
	return ret;
}

/**
 * gcm_xserver_get_output_profile_data:
 *
 * @xserver: a valid %GcmXserver instance
 * @output_name: the output name, e.g. "LVDS1"
 * @data: the data that is returned from the XServer. Free with g_free()
 * @length: the size of the returned data, or %NULL if you don't care
 * @error: a %GError that is set in the result of an error, or %NULL
 * Return value: %TRUE for success.
 *
 * Gets the ICC profile data from the specified output.
 **/
gboolean
gcm_xserver_get_output_profile_data (GcmXserver *xserver, const gchar *output_name, guint8 **data, gsize *length, GError **error)
{
	gboolean ret = FALSE;
	const gchar *atom_name;
	gchar *data_tmp = NULL;
	gint format;
	gint rc = Success;
	gulong bytes_after;
	gulong nitems;
	Atom atom = None;
	Atom type;
	gint i;
	XRROutputInfo *output;
	XRRScreenResources *resources = NULL;
	GcmXserverPrivate *priv = xserver->priv;

	g_return_val_if_fail (GCM_IS_XSERVER (xserver), FALSE);
	g_return_val_if_fail (data != NULL, FALSE);

	/* get the atom name */
	atom_name = "_ICC_PROFILE";

	/* get the value */
	gdk_error_trap_push ();
	atom = gdk_x11_get_xatom_by_name_for_display (priv->display_gdk, atom_name);
	resources = XRRGetScreenResources (priv->display, priv->window);
	for (i = 0; i < resources->noutput; ++i) {
		output = XRRGetOutputInfo (priv->display, resources, resources->outputs[i]);
		if (g_strcmp0 (output->name, output_name) == 0) {
			rc = XRRGetOutputProperty (priv->display, resources->outputs[i],
						   atom, 0, ~0, False, False, AnyPropertyType, &type, &format, &nitems, &bytes_after, (unsigned char **) &data_tmp);
			egg_debug ("found %s, got %i bytes", output_name, (guint) nitems);
		}
		XRRFreeOutputInfo (output);
	}
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
	if (resources != NULL)
		XRRFreeScreenResources (resources);
	if (data_tmp != NULL)
		XFree (data_tmp);
	return ret;
}

/**
 * gcm_xserver_set_output_profile:
 * @xserver: a valid %GcmXserver instance
 * @output_name: the output name, e.g. "LVDS1"
 * @filename: the filename of the ICC profile
 * @error: a %GError that is set in the result of an error, or %NULL
 * Return value: %TRUE for success.
 *
 * Sets the ICC profile data to the specified output.
 **/
gboolean
gcm_xserver_set_output_profile (GcmXserver *xserver, const gchar *output_name, const gchar *filename, GError **error)
{
	gboolean ret;
	gchar *data = NULL;
	gsize length;

	g_return_val_if_fail (GCM_IS_XSERVER (xserver), FALSE);
	g_return_val_if_fail (filename != NULL, FALSE);

	egg_debug ("setting output '%s' ICC profile atom from %s", output_name, filename);

	/* get contents of file */
	ret = g_file_get_contents (filename, &data, &length, error);
	if (!ret)
		goto out;

	/* send to the XServer */
	ret = gcm_xserver_set_output_profile_data (xserver, output_name, (const guint8 *) data, length, error);
	if (!ret)
		goto out;
out:
	g_free (data);
	return ret;
}

/**
 * gcm_xserver_set_output_profile_data:
 * @xserver: a valid %GcmXserver instance
 * @output_name: the output name, e.g. "LVDS1"
 * @data: the data that is to be set to the XServer
 * @length: the size of the data
 * @error: a %GError that is set in the result of an error, or %NULL
 * Return value: %TRUE for success.
 *
 * Sets the ICC profile data to the specified output.
 **/
gboolean
gcm_xserver_set_output_profile_data (GcmXserver *xserver, const gchar *output_name, const guint8 *data, gsize length, GError **error)
{
	gboolean ret = FALSE;
	const gchar *atom_name;
	gint rc;
	gint i;
	Atom atom = None;
	XRROutputInfo *output;
	XRRScreenResources *resources = NULL;
	GcmXserverPrivate *priv = xserver->priv;

	g_return_val_if_fail (GCM_IS_XSERVER (xserver), FALSE);
	g_return_val_if_fail (data != NULL, FALSE);
	g_return_val_if_fail (length != 0, FALSE);

	/* get the atom name */
	atom_name = "_ICC_PROFILE";

	/* get the value */
	gdk_error_trap_push ();
	atom = gdk_x11_get_xatom_by_name_for_display (priv->display_gdk, atom_name);
	resources = XRRGetScreenResources (priv->display, priv->window);
	for (i = 0; i < resources->noutput; ++i) {
		output = XRRGetOutputInfo (priv->display, resources, resources->outputs[i]);
		if (g_strcmp0 (output->name, output_name) == 0) {
			egg_debug ("found %s, setting %i bytes", output_name, length);
			XRRChangeOutputProperty (priv->display, resources->outputs[i], atom, XA_CARDINAL, 8, PropModeReplace, (unsigned char*) data, (gint)length);
		}
		XRRFreeOutputInfo (output);
	}
	rc = gdk_error_trap_pop ();

	/* did the call fail */
	if (rc != Success) {
		if (error != NULL)
			*error = g_error_new (1, 0, "failed to set output %s atom with rc %i", atom_name, rc);
		goto out;
	}

	/* success */
	ret = TRUE;
out:
	if (resources != NULL)
		XRRFreeScreenResources (resources);
	return ret;
}

/**
 * gcm_xserver_get_property:
 **/
static void
gcm_xserver_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GcmXserver *xserver = GCM_XSERVER (object);
	GcmXserverPrivate *priv = xserver->priv;

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
 * gcm_xserver_set_property:
 **/
static void
gcm_xserver_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	GcmXserver *xserver = GCM_XSERVER (object);
	GcmXserverPrivate *priv = xserver->priv;

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
 * gcm_xserver_class_init:
 **/
static void
gcm_xserver_class_init (GcmXserverClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gcm_xserver_finalize;
	object_class->get_property = gcm_xserver_get_property;
	object_class->set_property = gcm_xserver_set_property;

	/**
	 * GcmXserver:display-name:
	 */
	pspec = g_param_spec_string ("display-name", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_DISPLAY_NAME, pspec);

	g_type_class_add_private (klass, sizeof (GcmXserverPrivate));
}

/**
 * gcm_xserver_init:
 **/
static void
gcm_xserver_init (GcmXserver *xserver)
{
	xserver->priv = GCM_XSERVER_GET_PRIVATE (xserver);
	xserver->priv->display_name = NULL;

	/* get defaults for single screen */
	xserver->priv->display_gdk = gdk_display_get_default ();
	xserver->priv->window_gdk = gdk_get_default_root_window ();
	xserver->priv->display = GDK_DISPLAY_XDISPLAY (xserver->priv->display_gdk);
	xserver->priv->window = GDK_WINDOW_XID (xserver->priv->window_gdk);
}

/**
 * gcm_xserver_finalize:
 **/
static void
gcm_xserver_finalize (GObject *object)
{
	GcmXserver *xserver = GCM_XSERVER (object);
	GcmXserverPrivate *priv = xserver->priv;

	g_free (priv->display_name);

	G_OBJECT_CLASS (gcm_xserver_parent_class)->finalize (object);
}

/**
 * gcm_xserver_new:
 *
 * Return value: a new GcmXserver object.
 **/
GcmXserver *
gcm_xserver_new (void)
{
	GcmXserver *xserver;
	xserver = g_object_new (GCM_TYPE_XSERVER, NULL);
	return GCM_XSERVER (xserver);
}


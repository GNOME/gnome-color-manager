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
 * SECTION:gcm-x11-screen
 * @short_description: An object to interact with the XServer screen.
 *
 * This object talks to the currently running X Server.
 * The #GcmX11Screen objwect will contain one or many #GcmX11Outputs.
 */

#include "config.h"

#include <glib-object.h>
#include <gdk/gdkx.h>
#include <X11/extensions/Xrandr.h>
#include <X11/Xatom.h>

#include "gcm-x11-screen.h"

static void     gcm_x11_screen_finalize	(GObject     *object);

#define GCM_X11_SCREEN_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GCM_TYPE_X11_SCREEN, GcmX11ScreenPrivate))

/**
 * GcmX11ScreenPrivate:
 *
 * Private #GcmX11Screen data
 **/
struct _GcmX11ScreenPrivate
{
	GdkScreen			*gdk_screen;
	GdkWindow			*gdk_root;
	Display				*xdisplay;
	Screen				*xscreen;
	Window				 xroot;
	guint				 randr_event_base;
	guint				 rr_major_version;
	guint				 rr_minor_version;
	GPtrArray			*outputs;
};

enum {
	SIGNAL_CHANGED,
	SIGNAL_ADDED,
	SIGNAL_REMOVED,
	SIGNAL_LAST
};

static guint signals[SIGNAL_LAST] = { 0 };
static gpointer gcm_x11_screen_object = NULL;
G_DEFINE_TYPE (GcmX11Screen, gcm_x11_screen, G_TYPE_OBJECT)

/**
 * gcm_x11_screen_on_event_cb:
 **/
static GdkFilterReturn
gcm_x11_screen_on_event_cb (GdkXEvent *xevent, GdkEvent *event, gpointer data)
{
	GcmX11Screen *screen = data;
	GcmX11ScreenPrivate *priv = screen->priv;
	XEvent *e = xevent;
	gint event_num;

	if (e == NULL)
		return GDK_FILTER_CONTINUE;

	event_num = e->type - priv->randr_event_base;

	if (event_num == RRScreenChangeNotify) {
		g_debug ("emit changed");
		g_signal_emit (screen, signals[SIGNAL_CHANGED], 0);
	}

	/* Pass the event on to GTK+ */
	return GDK_FILTER_CONTINUE;
}

/**
 * gcm_x11_screen_get_output_for_id:
 **/
static GcmX11Output *
gcm_x11_screen_get_output_for_id (GcmX11Screen *screen, guint id)
{
	GcmX11Output *output = NULL;
	GcmX11Output *output_tmp;
	GcmX11ScreenPrivate *priv = screen->priv;
	guint i;

	/* find by id */
	for (i=0; i<priv->outputs->len; i++) {
		output_tmp = g_ptr_array_index (screen->priv->outputs, i);
		if (id == gcm_x11_output_get_id (output)) {
			output = output_tmp;
			break;
		}
	}
	return output;
}

/**
 * gcm_x11_screen_refresh:
 **/
static gboolean
gcm_x11_screen_refresh (GcmX11Screen *screen, GError **error)
{
	guint i;
	gboolean connected;
	gboolean ret = FALSE;
	GcmX11ScreenPrivate *priv = screen->priv;
	XRRScreenResources *resources;
	RROutput rr_output;
	GcmX11Output *output;
	XRROutputInfo *output_info;
	XRRCrtcInfo *crtc_info;
	gint gamma_size;
	gboolean emit_added;

	/* mark all outputs as disconnected */
	for (i=0; i<priv->outputs->len; i++) {
		output = g_ptr_array_index (priv->outputs, i);
		gcm_x11_output_set_connected (output, FALSE);
	}

	/* get new resources */
	gdk_error_trap_push ();
	resources = XRRGetScreenResources (priv->xdisplay, priv->xroot);
	gdk_flush ();
	if (gdk_error_trap_pop () || resources == NULL) {
		g_set_error_literal (error,
				     GCM_X11_SCREEN_ERROR, GCM_X11_SCREEN_ERROR_INTERNAL,
				     "Failed to get X11 resources");
		goto out;
	}

	/* add each output */
	for (i=0; i < (guint) resources->noutput; i++) {
		rr_output = resources->outputs[i];

		/* get information about the output */
		gdk_error_trap_push ();
		output_info = XRRGetOutputInfo (priv->xdisplay, resources, rr_output);
		gdk_flush ();
		if (gdk_error_trap_pop ()) {
			g_warning ("failed to get output info");
			continue;
		}

		connected = (output_info->connection == RR_Connected);
		if (connected && output_info->crtc != 0) {

			/* get crtc info */
			gdk_error_trap_push ();
			crtc_info = XRRGetCrtcInfo (priv->xdisplay, resources, output_info->crtc);
			gdk_flush ();
			if (gdk_error_trap_pop () || crtc_info == NULL) {
				g_warning ("failed to get crtc info for %s", output_info->name);
				continue;
			}

			/* get gamma size */
			gdk_error_trap_push ();
			gamma_size = XRRGetCrtcGammaSize (priv->xdisplay, output_info->crtc);
			gdk_flush ();
			if (gdk_error_trap_pop ()) {
				g_warning ("failed to get gamma size");
				continue;
			}

			/* try and find an existing device */
			emit_added = FALSE;
			output = gcm_x11_screen_get_output_for_id (screen, rr_output);
			if (output == NULL) {
				output = gcm_x11_output_new ();
				emit_added = TRUE;
			}

			/* create new object and set properties */
			gcm_x11_output_set_name (output, output_info->name);
			gcm_x11_output_set_display (output, priv->xdisplay);
			gcm_x11_output_set_id (output, rr_output);
			gcm_x11_output_set_crtc_id (output, output_info->crtc);
			gcm_x11_output_set_primary (output, (crtc_info->x == 0 && crtc_info->y == 0));
			gcm_x11_output_set_gamma_size (output, gamma_size);
			gcm_x11_output_set_connected (output, connected);
			gcm_x11_output_set_position (output, crtc_info->x, crtc_info->y);
			gcm_x11_output_set_size (output, crtc_info->width, crtc_info->height);

			/* add it to the array */
			if (emit_added) {
				g_ptr_array_add (priv->outputs, output);
				g_debug ("emit added: %s", output_info->name);
				g_signal_emit (screen, signals[SIGNAL_ADDED], 0, output);
			}

			/* free client side X resources */
			XRRFreeCrtcInfo (crtc_info);
		}
		XRRFreeOutputInfo (output_info);
	}

	/* remove any disconnected output */
	for (i=0; i<priv->outputs->len; i++) {
		output = g_ptr_array_index (priv->outputs, i);
		if (!gcm_x11_output_get_connected (output)) {
			g_debug ("emit added: %s", output_info->name);
			g_signal_emit (screen, signals[SIGNAL_REMOVED], 0, output);
			g_ptr_array_remove (priv->outputs, output);
		}
	}

	/* success */
	ret = TRUE;
out:
	return ret;
}

/**
 * gcm_x11_screen_assign:
 * @screen: a valid %GcmX11Screen instance
 * @gdk_screen: a #GdkScreen
 * @error: a %GError or %NULL
 *
 * Assigns a #GdkScreen to this instance.
 *
 * Return value: %TRUE for success.
 **/
gboolean
gcm_x11_screen_assign (GcmX11Screen *screen, GdkScreen *gdk_screen, GError **error)
{
	GcmX11ScreenPrivate *priv = screen->priv;
	Display *dpy;
	gint event_base;
	gint ignore;
	gboolean ret = FALSE;

	g_return_val_if_fail (GCM_IS_X11_SCREEN (screen), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* NULL means default */
	if (gdk_screen != NULL) {
		priv->gdk_screen = gdk_screen;
	} else {
		priv->gdk_screen = gdk_screen_get_default ();
	}

	/* do we even have XRandR? */
	dpy = GDK_SCREEN_XDISPLAY (priv->gdk_screen);
	if (!XRRQueryExtension (dpy, &event_base, &ignore)) {
		g_set_error_literal (error, GCM_X11_SCREEN_ERROR, GCM_X11_SCREEN_ERROR_INTERNAL,
				     "RANDR extension is not present");
		goto out;
	}

	priv->gdk_root = gdk_screen_get_root_window (priv->gdk_screen);
	priv->xroot = gdk_x11_window_get_xid (priv->gdk_root);
	priv->xdisplay = dpy;
	priv->xscreen = gdk_x11_screen_get_xscreen (priv->gdk_screen);
	priv->randr_event_base = event_base;

	/* get version */
	gdk_error_trap_push ();
	XRRQueryVersion (dpy, (gint*)&priv->rr_major_version, (gint*)&priv->rr_minor_version);
	gdk_flush ();
	if (gdk_error_trap_pop ()) {
		g_set_error_literal (error, GCM_X11_SCREEN_ERROR, GCM_X11_SCREEN_ERROR_INTERNAL,
				     "failed to get RANDR extension version");
		goto out;
	}

	/* too small */
	if (priv->rr_major_version > 1 ||
	    (priv->rr_major_version == 1 && priv->rr_minor_version < 2)) {
		g_set_error_literal (error, GCM_X11_SCREEN_ERROR, GCM_X11_SCREEN_ERROR_INTERNAL,
				     "RANDR extension is too old (must be at least 1.2)");
		goto out;
	}

	/* add filter */
	gdk_error_trap_push ();
	XRRSelectInput (priv->xdisplay,
			priv->xroot,
			RRScreenChangeNotifyMask);
	if (gdk_error_trap_pop ()) {
		g_warning ("failed to select input");
		goto out;
	}

	gdk_x11_register_standard_event_type (gdk_screen_get_display (priv->gdk_screen),
					      event_base, RRNotify + 1);

	gdk_window_add_filter (priv->gdk_root, gcm_x11_screen_on_event_cb, screen);

	/* get resources */
	ret = gcm_x11_screen_refresh (screen, error);
out:
	return ret;
}

/**
 * gcm_x11_screen_get_outputs:
 * @screen: a valid %GcmX11Screen instance
 * @error: a %GError or %NULL
 *
 * Gets the list of outputs.
 *
 * Return value: A #GPtrArray of #GcmX11Output's. Free with g_ptr_array_unref() when done.
 **/
GPtrArray *
gcm_x11_screen_get_outputs (GcmX11Screen *screen, GError **error)
{
	GcmX11ScreenPrivate *priv = screen->priv;

	/* not set the display */
	if (priv->gdk_screen == NULL) {
		g_set_error_literal (error,
				     GCM_X11_SCREEN_ERROR, GCM_X11_SCREEN_ERROR_INTERNAL,
				     "no display set, use gcm_x11_screen_assign()");
		return NULL;
	}

	return g_ptr_array_ref (priv->outputs);
}

/**
 * gcm_x11_screen_get_output_by_name:
 * @screen: a valid %GcmX11Screen instance
 * @name: an output name, e.g. "lvds1"
 * @error: a %GError or %NULL
 *
 * Gets a specified output.
 *
 * Return value: A #GcmX11Output, or %NULL if nothing matched.
 **/
GcmX11Output *
gcm_x11_screen_get_output_by_name (GcmX11Screen *screen, const gchar *name, GError **error)
{
	guint i;
	GcmX11Output *output;
	GcmX11ScreenPrivate *priv = screen->priv;

	/* not set the display */
	if (priv->gdk_screen == NULL) {
		g_set_error_literal (error,
				     GCM_X11_SCREEN_ERROR, GCM_X11_SCREEN_ERROR_INTERNAL,
				     "no display set, use gcm_x11_screen_assign()");
		return NULL;
	}

	/* find the output */
	for (i=0; i<priv->outputs->len; i++) {
		output = g_ptr_array_index (priv->outputs, i);
		if (g_strcmp0 (gcm_x11_output_get_name (output), name) == 0)
			return g_object_ref (output);
	}
	g_set_error_literal (error,
			     GCM_X11_SCREEN_ERROR, GCM_X11_SCREEN_ERROR_INTERNAL,
			     "no output with that name");
	return NULL;
}

/**
 * gcm_x11_screen_get_profile_data:
 * @screen: a valid %GcmX11Screen instance
 * @data: the data that is returned from the XServer. Free with g_free()
 * @length: the size of the returned data, or %NULL if you don't care
 * @error: a %GError that is set in the result of an error, or %NULL
 *
 * Gets the ICC profile data from the XServer.
 *
 * Return value: %TRUE for success.
 **/
gboolean
gcm_x11_screen_get_profile_data (GcmX11Screen *screen, guint8 **data, gsize *length, GError **error)
{
	gboolean ret = FALSE;
	gchar *data_tmp = NULL;
	gint format;
	gint rc;
	gulong bytes_after;
	gulong nitems;
	Atom atom = None;
	Atom type;
	GcmX11ScreenPrivate *priv = screen->priv;

	g_return_val_if_fail (GCM_IS_X11_SCREEN (screen), FALSE);
	g_return_val_if_fail (data != NULL, FALSE);

	/* get the value */
	gdk_error_trap_push ();
	atom = XInternAtom (priv->xdisplay, "_ICC_PROFILE", FALSE);
	rc = XGetWindowProperty (priv->xdisplay, priv->xroot, atom, 0, G_MAXLONG, False, XA_CARDINAL,
				 &type, &format, &nitems, &bytes_after, (void*) &data_tmp);
	gdk_error_trap_pop_ignored ();

	/* did the call fail */
	if (rc != Success) {
		g_set_error (error, 1, 0, "failed to get icc profile atom with rc %i", rc);
		goto out;
	}

	/* was nothing found */
	if (nitems == 0) {
		g_set_error (error, 1, 0, "atom has not been set");
		goto out;
	}

	/* allocate the data using Glib, rather than asking the user to use XFree */
	*data = g_memdup (data_tmp, nitems);

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
 * gcm_x11_screen_set_profile:
 * @screen: a valid %GcmX11Screen instance
 * @filename: the filename of the ICC profile
 * @error: a %GError that is set in the result of an error, or %NULL
 *
 * Sets the ICC profile data to the XServer.
 *
 * Return value: %TRUE for success.
 **/
gboolean
gcm_x11_screen_set_profile (GcmX11Screen *screen, const gchar *filename, GError **error)
{
	gboolean ret;
	gchar *data = NULL;
	gsize length;

	g_return_val_if_fail (GCM_IS_X11_SCREEN (screen), FALSE);
	g_return_val_if_fail (filename != NULL, FALSE);

	g_debug ("setting root window ICC profile atom from %s", filename);

	/* get contents of file */
	ret = g_file_get_contents (filename, &data, &length, error);
	if (!ret)
		goto out;

	/* send to the XServer */
	ret = gcm_x11_screen_set_profile_data (screen, (const guint8 *) data, length, error);
	if (!ret)
		goto out;
out:
	g_free (data);
	return ret;
}

/**
 * gcm_x11_screen_set_profile_data:
 * @screen: a valid %GcmX11Screen instance
 * @data: the data that is to be set to the XServer
 * @length: the size of the data
 * @error: a %GError that is set in the result of an error, or %NULL
 *
 * Sets the ICC profile data to the XServer.
 *
 * Return value: %TRUE for success.
 **/
gboolean
gcm_x11_screen_set_profile_data (GcmX11Screen *screen, const guint8 *data, gsize length, GError **error)
{
	gboolean ret = FALSE;
	gint rc;
	Atom atom = None;
	GcmX11ScreenPrivate *priv = screen->priv;

	g_return_val_if_fail (GCM_IS_X11_SCREEN (screen), FALSE);
	g_return_val_if_fail (data != NULL, FALSE);
	g_return_val_if_fail (length != 0, FALSE);

	/* get the value */
	gdk_error_trap_push ();
	atom = XInternAtom (priv->xdisplay, "_ICC_PROFILE", FALSE);
	rc = XChangeProperty (priv->xdisplay, priv->xroot, atom, XA_CARDINAL, 8, PropModeReplace, (unsigned char*) data, length);
	gdk_error_trap_pop_ignored ();

	/* for some reason this fails with BadRequest, but actually sets the value */
	if (rc == BadRequest)
		rc = Success;

	/* did the call fail */
	if (rc != Success) {
		g_set_error (error, 1, 0, "failed to set icc profile atom with rc %i", rc);
		goto out;
	}

	/* success */
	ret = TRUE;
out:
	return ret;
}

/**
 * gcm_x11_screen_set_protocol_version:
 * @screen: a valid %GcmX11Screen instance
 * @major: the major version
 * @minor: the minor version
 * @error: a %GError that is set in the result of an error, or %NULL
 *
 * Sets the ICC Profiles in X supported version to the XServer.
 *
 * Return value: %TRUE for success.
 **/
gboolean
gcm_x11_screen_set_protocol_version (GcmX11Screen *screen, guint major, guint minor, GError **error)
{
	gboolean ret = FALSE;
	gint rc;
	Atom atom = None;
	guint data;
	GcmX11ScreenPrivate *priv = screen->priv;

	g_return_val_if_fail (GCM_IS_X11_SCREEN (screen), FALSE);

	/* get the atom data */
	data = major * 100 + minor * 1;

	/* get the value */
	gdk_error_trap_push ();
	atom = XInternAtom (priv->xdisplay, "_ICC_PROFILE_IN_X_VERSION", FALSE);
	rc = XChangeProperty (priv->xdisplay, priv->xroot, atom, XA_CARDINAL, 8, PropModeReplace, (unsigned char*) &data, 1);
	gdk_error_trap_pop_ignored ();

	/* for some reason this fails with BadRequest, but actually sets the value */
	if (rc == BadRequest)
		rc = Success;

	/* did the call fail */
	if (rc != Success) {
		g_set_error (error, 1, 0, "failed to set icc profile atom with rc %i", rc);
		goto out;
	}

	/* success */
	ret = TRUE;
out:
	return ret;
}

/**
 * gcm_x11_screen_remove_protocol_version:
 * @screen: a valid %GcmX11Screen instance
 * @error: a %GError that is set in the result of an error, or %NULL
 *
 * Removes the ICC profile version data from the XServer.
 *
 * Return value: %TRUE for success.
 **/
gboolean
gcm_x11_screen_remove_protocol_version (GcmX11Screen *screen, GError **error)
{
	Atom atom = None;
	gint rc;
	gboolean ret = TRUE;
	GcmX11ScreenPrivate *priv = screen->priv;

	g_return_val_if_fail (GCM_IS_X11_SCREEN (screen), FALSE);

	g_debug ("removing root window ICC profile atom");

	/* get the value */
	gdk_error_trap_push ();
	atom = XInternAtom (priv->xdisplay, "_ICC_PROFILE_IN_X_VERSION", FALSE);
	rc = XDeleteProperty(priv->xdisplay, priv->xroot, atom);
	gdk_error_trap_pop_ignored ();

	/* this fails with BadRequest if the atom was not set */
	if (rc == BadRequest)
		rc = Success;

	/* did the call fail */
	if (rc != Success) {
		ret = FALSE;
		g_set_error (error, 1, 0, "failed to delete root window atom with rc %i", rc);
		goto out;
	}
out:
	return ret;
}

/**
 * gcm_x11_screen_get_protocol_version:
 * @screen: a valid %GcmX11Screen instance
 * @major: the major version
 * @minor: the minor version
 * @error: a %GError that is set in the result of an error, or %NULL
 *
 * Gets the ICC profile data from the XServer.
 *
 * Return value: %TRUE for success.
 **/
gboolean
gcm_x11_screen_get_protocol_version (GcmX11Screen *screen, guint *major, guint *minor, GError **error)
{
	gboolean ret = FALSE;
	gchar *data_tmp;
	gint format;
	gint rc;
	gulong bytes_after;
	gulong nitems;
	Atom atom = None;
	Atom type;
	GcmX11ScreenPrivate *priv = screen->priv;

	g_return_val_if_fail (GCM_IS_X11_SCREEN (screen), FALSE);
	g_return_val_if_fail (major != NULL, FALSE);
	g_return_val_if_fail (minor != NULL, FALSE);

	/* get the value */
	gdk_error_trap_push ();
	atom = XInternAtom (priv->xdisplay, "_ICC_PROFILE_IN_X_VERSION", FALSE);
	rc = XGetWindowProperty (priv->xdisplay, priv->xroot, atom, 0, G_MAXLONG, False, XA_CARDINAL,
				 &type, &format, &nitems, &bytes_after, (unsigned char **) &data_tmp);
	gdk_error_trap_pop_ignored ();

	/* did the call fail */
	if (rc != Success) {
		g_set_error (error, 1, 0, "failed to get atom with rc %i", rc);
		goto out;
	}

	/* was nothing found */
	if (nitems == 0) {
		g_set_error (error, 1, 0, "icc profile atom has not been set");
		goto out;
	}

	/* set total */
	*major = (guint) data_tmp[0] / 100;
	*minor = (guint) data_tmp[0] % 100;

	/* success */
	ret = TRUE;
out:
	if (data_tmp != NULL)
		XFree (data_tmp);
	return ret;
}

/**
 * gcm_x11_screen_remove_profile:
 * @screen: a valid %GcmX11Screen instance
 * @error: a %GError that is set in the result of an error, or %NULL
 *
 * Removes the ICC profile data from the XServer.
 *
 * Return value: %TRUE for success.
 **/
gboolean
gcm_x11_screen_remove_profile (GcmX11Screen *screen, GError **error)
{
	Atom atom = None;
	gint rc;
	gboolean ret = TRUE;
	GcmX11ScreenPrivate *priv = screen->priv;

	g_return_val_if_fail (GCM_IS_X11_SCREEN (screen), FALSE);

	g_debug ("removing root window ICC profile atom");

	/* get the value */
	gdk_error_trap_push ();
	atom = XInternAtom (priv->xdisplay, "_ICC_PROFILE", FALSE);
	rc = XDeleteProperty (priv->xdisplay, priv->xroot, atom);
	gdk_error_trap_pop_ignored ();

	/* this fails with BadRequest if the atom was not set */
	if (rc == BadRequest)
		rc = Success;

	/* did the call fail */
	if (rc != Success) {
		ret = FALSE;
		g_set_error (error, 1, 0, "failed to delete root window atom with rc %i", rc);
		goto out;
	}
out:
	return ret;
}

/**
 * gcm_x11_screen_get_randr_version:
 * @screen: a valid %GcmX11Screen instance
 * @major: the returned XRandR version major, or %NULL
 * @minor: the returned XRandR version minor, or %NULL
 *
 * Gets the XRandR version from the server.
 **/
void
gcm_x11_screen_get_randr_version (GcmX11Screen *screen, guint *major, guint *minor)
{
	g_return_if_fail (GCM_IS_X11_SCREEN (screen));
	if (major != NULL)
		*major = screen->priv->rr_major_version;
	if (minor != NULL)
		*minor = screen->priv->rr_minor_version;
}

/**
 * gcm_x11_screen_class_init:
 **/
static void
gcm_x11_screen_class_init (GcmX11ScreenClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gcm_x11_screen_finalize;

	/**
	 * GcmX11Screen::changed:
	 **/
	signals[SIGNAL_CHANGED] =
		g_signal_new ("changed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GcmX11ScreenClass, changed),
			      NULL, NULL, g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	/**
	 * GcmX11Screen::added:
	 **/
	signals[SIGNAL_ADDED] =
		g_signal_new ("added",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GcmX11ScreenClass, added),
			      NULL, NULL, g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, GCM_TYPE_X11_OUTPUT);
	/**
	 * GcmX11Screen::removed:
	 **/
	signals[SIGNAL_REMOVED] =
		g_signal_new ("removed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GcmX11ScreenClass, removed),
			      NULL, NULL, g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, GCM_TYPE_X11_OUTPUT);

	g_type_class_add_private (klass, sizeof (GcmX11ScreenPrivate));
}

/**
 * gcm_x11_screen_init:
 **/
static void
gcm_x11_screen_init (GcmX11Screen *screen)
{
	screen->priv = GCM_X11_SCREEN_GET_PRIVATE (screen);
	screen->priv->outputs = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
}

/**
 * gcm_x11_screen_finalize:
 **/
static void
gcm_x11_screen_finalize (GObject *object)
{
	GcmX11Screen *screen = GCM_X11_SCREEN (object);
	GcmX11ScreenPrivate *priv = screen->priv;

	g_ptr_array_unref (screen->priv->outputs);
	gdk_window_remove_filter (priv->gdk_root, gcm_x11_screen_on_event_cb, screen);

	G_OBJECT_CLASS (gcm_x11_screen_parent_class)->finalize (object);
}

/**
 * gcm_x11_screen_new:
 *
 * Return value: a new #GcmX11Screen object.
 **/
GcmX11Screen *
gcm_x11_screen_new (void)
{
	if (gcm_x11_screen_object != NULL) {
		g_object_ref (gcm_x11_screen_object);
	} else {
		gcm_x11_screen_object = g_object_new (GCM_TYPE_X11_SCREEN, NULL);
		g_object_add_weak_pointer (gcm_x11_screen_object, &gcm_x11_screen_object);
	}
	return GCM_X11_SCREEN (gcm_x11_screen_object);
}


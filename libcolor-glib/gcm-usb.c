/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU Lesser General Public License Version 2.1
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

/**
 * SECTION:gcm-usb
 * @short_description: GLib mainloop integration for libusb
 *
 * This object can be used to integrate libusb into the GLib event loop.
 */

#include "config.h"

#include <glib-object.h>
#include <sys/poll.h>
#include <libusb-1.0/libusb.h>

#include "gcm-usb.h"
#include "gcm-compat.h"

static void     gcm_usb_finalize	(GObject     *object);

#define GCM_USB_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GCM_TYPE_USB, GcmUsbPrivate))

typedef struct {
	GSource			 source;
	GSList			*pollfds;
} GcmUsbSource;

/**
 * GcmUsbPrivate:
 *
 * Private #GcmUsb data
 **/
struct _GcmUsbPrivate
{
	gboolean			 connected;
	GcmUsbSource			*source;
	libusb_device_handle		*handle;
	libusb_context			*ctx;
};

enum {
	PROP_0,
	PROP_CONNECTED,
	PROP_LAST
};

G_DEFINE_TYPE (GcmUsb, gcm_usb, G_TYPE_OBJECT)

/**
 * gcm_usb_get_connected:
 *
 * Since: 2.91.1
 **/
gboolean
gcm_usb_get_connected (GcmUsb *usb)
{
	g_return_val_if_fail (GCM_IS_USB (usb), FALSE);
	return usb->priv->connected;
}

/**
 * gcm_libusb_pollfd_add:
 **/
static void
gcm_libusb_pollfd_add (GcmUsb *usb, int fd, short events)
{
	GcmUsbSource *source = usb->priv->source;
	GPollFD *pollfd = g_slice_new (GPollFD);
	pollfd->fd = fd;
	pollfd->events = 0;
	pollfd->revents = 0;
	if (events & POLLIN)
		pollfd->events |= G_IO_IN;
	if (events & POLLOUT)
		pollfd->events |= G_IO_OUT;

	source->pollfds = g_slist_prepend (source->pollfds, pollfd);
	g_source_add_poll ((GSource *) source, pollfd);
}

/**
 * gcm_libusb_pollfd_added_cb:
 **/
static void
gcm_libusb_pollfd_added_cb (int fd, short events, void *user_data)
{
	GcmUsb *usb = user_data;
	gcm_libusb_pollfd_add (usb, fd, events);
}

/**
 * gcm_libusb_pollfd_remove:
 **/
static void
gcm_libusb_pollfd_remove (GcmUsb *usb, int fd)
{
	GcmUsbSource *source = usb->priv->source;
	GPollFD *pollfd;
	GSList *elem = source->pollfds;

	g_debug ("remove pollfd %i", fd);

	/* nothing to see here, move along */
	if (elem == NULL) {
		g_warning("cannot remove from list as list is empty?");
		return;
	}

	/* find the pollfd in the list */
	do {
		pollfd = elem->data;
		if (pollfd->fd != fd)
			continue;

		g_source_remove_poll ((GSource *) source, pollfd);
		g_slice_free (GPollFD, pollfd);
		source->pollfds = g_slist_delete_link (source->pollfds, elem);
		return;
	} while ((elem = g_slist_next(elem)));
	g_warning ("couldn't find fd %d in list", fd);
}

/**
 * gcm_libusb_pollfd_remove_all:
 **/
static void
gcm_libusb_pollfd_remove_all (GcmUsb *usb)
{
	GcmUsbSource *source = usb->priv->source;
	GPollFD *pollfd;
	GSList *elem;

	/* never connected */
	if (source == NULL) {
		g_debug ("never attached to a context");
		return;
	}

	/* nothing to see here, move along */
	elem = source->pollfds;
	if (elem == NULL)
		return;

	/* rip apart all the pollfd's */
	do {
		pollfd = elem->data;
		g_warning ("removing %i", pollfd->fd);
		g_source_remove_poll ((GSource *) source, pollfd);
		g_slice_free (GPollFD, pollfd);
		source->pollfds = g_slist_delete_link (source->pollfds, elem);
	} while ((elem = g_slist_next(elem)));
}

/**
 * gcm_libusb_pollfd_removed_cb:
 **/
static void
gcm_libusb_pollfd_removed_cb (int fd, void *user_data)
{
	GcmUsb *usb = user_data;
	gcm_libusb_pollfd_remove (usb, fd);
}

/**
 * gcm_usb_source_prepare:
 *
 * Called before all the file descriptors are polled.
 * As we are a file descriptor source, the prepare function returns FALSE.
 * It sets the returned timeout to -1 to indicate that it doesn't mind
 * how long the poll() call blocks.
 *
 * No, we're not going to support FreeBSD.
 **/
static gboolean
gcm_usb_source_prepare (GSource *source, gint *timeout)
{
	*timeout = -1;
	return FALSE;
}

/**
 * gcm_usb_source_check:
 *
 * In the check function, it tests the results of the poll() call to see
 * if the required condition has been met, and returns TRUE if so.
 **/
static gboolean
gcm_usb_source_check (GSource *source)
{
	GcmUsbSource *gcm_source = (GcmUsbSource *) source;
	GPollFD *pollfd;
	GSList *elem = gcm_source->pollfds;

	/* no fds */
	if (elem == NULL)
		return FALSE;

	/* check each pollfd */
	do {
		pollfd = elem->data;
		if (pollfd->revents)
			return TRUE;
	} while ((elem = g_slist_next (elem)));

	return FALSE;
}

/**
 * gcm_usb_source_dispatch:
 **/
static gboolean
gcm_usb_source_dispatch (GSource *source, GSourceFunc callback, gpointer user_data)
{
	gint r;
	struct timeval tv = { 0, 0 };
	GcmUsb *usb = user_data;
	r = libusb_handle_events_timeout (usb->priv->ctx, &tv);
	if (r < 0) {
		g_warning ("failed to handle event: %s",
			   libusb_strerror (retval));
	}
	return TRUE;
}

/**
 * gcm_usb_source_finalize:
 *
 * Called when the source is finalized.
 **/
static void
gcm_usb_source_finalize (GSource *source)
{
	GPollFD *pollfd;
	GcmUsbSource *gcm_source = (GcmUsbSource *) source;
	GSList *elem = gcm_source->pollfds;

	if (elem != NULL) {
		do {
			pollfd = elem->data;
			g_source_remove_poll ((GSource *) gcm_source, pollfd);
			g_slice_free (GPollFD, pollfd);
			gcm_source->pollfds = g_slist_delete_link (gcm_source->pollfds, elem);
		} while ((elem = g_slist_next (elem)));
	}

	g_slist_free (gcm_source->pollfds);
}

static GSourceFuncs gcm_usb_source_funcs = {
	gcm_usb_source_prepare,
	gcm_usb_source_check,
	gcm_usb_source_dispatch,
	gcm_usb_source_finalize,
	NULL, NULL
	};

/**
 * gcm_usb_attach_to_context:
 * @usb:  a #GcmUsb instance
 * @context: a #GMainContext or %NULL
 *
 * Connects up usb-1 with the GLib event loop. This functionality
 * allows you to submit async requests using usb, and the callbacks
 * just kinda happen at the right time.
 *
 * Since: 2.91.1
 **/
void
gcm_usb_attach_to_context (GcmUsb *usb, GMainContext *context)
{
	guint i;
	const struct libusb_pollfd **pollfds;
	GcmUsbPrivate *priv = usb->priv;

	/* already connected */
	if (priv->source != NULL) {
		g_warning ("already attached to a context");
		return;
	}

	/* create new GcmUsbSource */
	priv->source = (GcmUsbSource *) g_source_new (&gcm_usb_source_funcs, sizeof(GcmUsbSource));
	priv->source->pollfds = NULL;

	/* assign user_data */
	g_source_set_callback ((GSource *)priv->source, NULL, usb, NULL);

	/* watch the fd's already created */
	pollfds = libusb_get_pollfds (usb->priv->ctx);
	for (i=0; pollfds[i] != NULL; i++)
		gcm_libusb_pollfd_add (usb, pollfds[i]->fd, pollfds[i]->events);

	/* watch for PollFD changes */
	libusb_set_pollfd_notifiers (priv->ctx,
				     gcm_libusb_pollfd_added_cb,
				     gcm_libusb_pollfd_removed_cb,
				     usb);

	/* attach to the mainloop */
	g_source_attach ((GSource *)priv->source, context);
}

/**
 * gcm_usb_load:
 * @usb:  a #GcmUsb instance
 * @error:  a #GError, or %NULL
 *
 * Connects to libusb. You normally don't have to call this method manually.
 *
 * Return value: %TRUE for success
 *
 * Since: 2.91.1
 **/
gboolean
gcm_usb_load (GcmUsb *usb, GError **error)
{
	gboolean ret = FALSE;
	gint retval;
	GcmUsbPrivate *priv = usb->priv;

	/* already done */
	if (priv->ctx != NULL) {
		ret = TRUE;
		goto out;
	}

	/* init */
	retval = libusb_init (&priv->ctx);
	if (retval < 0) {
		g_set_error (error, GCM_USB_ERROR,
			     GCM_USB_ERROR_INTERNAL,
			     "failed to init libusb: %s",
			     libusb_strerror (retval));
		goto out;
	}

	/* enable logging */
	libusb_set_debug (priv->ctx, 3);

	/* success */
	ret = TRUE;
	priv->connected = TRUE;
out:
	return ret;
}

/**
 * gcm_usb_get_device_handle:
 * @usb:  a #GcmUsb instance
 *
 * Gets the low-level device handle
 *
 * Return value: The #libusb_device_handle or %NULL. Do not unref this value.
 *
 * Since: 2.91.1
 **/
libusb_device_handle *
gcm_usb_get_device_handle (GcmUsb *usb)
{
	return usb->priv->handle;
}

/**
 * gcm_usb_connect:
 * @usb:  a #GcmUsb instance
 * @vendor_id: the vendor ID to connect to
 * @product_id: the product ID to connect to
 * @configuration: the configuration index to use, usually '1'
 * @interface: the configuration interface to use, usually '0'
 * @error:  a #GError, or %NULL
 *
 * Connects to a specific device.
 *
 * Return value: %TRUE for success
 *
 * Since: 2.91.1
 **/
gboolean
gcm_usb_connect (GcmUsb *usb, guint vendor_id, guint product_id, guint configuration, guint interface, GError **error)
{
	gint retval;
	gboolean ret = FALSE;
	GcmUsbPrivate *priv = usb->priv;

	/* already connected */
	if (priv->handle != NULL) {
		g_set_error_literal (error, GCM_USB_ERROR,
				     GCM_USB_ERROR_INTERNAL,
				     "already connected to a device");
		goto out;
	}

	/* load libusb if we've not done this already */
	ret = gcm_usb_load (usb, error);
	if (!ret)
		goto out;

	/* open device */
	priv->handle = libusb_open_device_with_vid_pid (priv->ctx, vendor_id, product_id);
	if (priv->handle == NULL) {
		g_set_error (error, GCM_USB_ERROR,
			     GCM_USB_ERROR_INTERNAL,
			     "failed to find device %04x:%04x",
			     vendor_id, product_id);
		ret = FALSE;
		goto out;
	}

	/* set configuration and interface */
	retval = libusb_set_configuration (priv->handle, configuration);
	if (retval < 0) {
		g_set_error (error, GCM_USB_ERROR,
			     GCM_USB_ERROR_INTERNAL,
			     "failed to set configuration 0x%02x: %s",
			     configuration, libusb_strerror (retval));
		ret = FALSE;
		goto out;
	}
	retval = libusb_claim_interface (priv->handle, interface);
	if (retval < 0) {
		g_set_error (error, GCM_USB_ERROR,
			     GCM_USB_ERROR_INTERNAL,
			     "failed to claim interface 0x%02x: %s",
			     interface, libusb_strerror (retval));
		ret = FALSE;
		goto out;
	}
out:
	return ret;
}

/**
 * gcm_usb_get_property:
 **/
static void
gcm_usb_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GcmUsb *usb = GCM_USB (object);
	GcmUsbPrivate *priv = usb->priv;

	switch (prop_id) {
	case PROP_CONNECTED:
		g_value_set_boolean (value, priv->connected);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gcm_usb_set_property:
 **/
static void
gcm_usb_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gcm_usb_class_init:
 **/
static void
gcm_usb_class_init (GcmUsbClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gcm_usb_finalize;
	object_class->get_property = gcm_usb_get_property;
	object_class->set_property = gcm_usb_set_property;

	/**
	 * GcmUsb:connected:
	 */
	pspec = g_param_spec_boolean ("connected", NULL, NULL,
				      FALSE,
				      G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_CONNECTED, pspec);

	g_type_class_add_private (klass, sizeof (GcmUsbPrivate));
}

/**
 * gcm_usb_init:
 **/
static void
gcm_usb_init (GcmUsb *usb)
{
	usb->priv = GCM_USB_GET_PRIVATE (usb);
}

/**
 * gcm_usb_finalize:
 **/
static void
gcm_usb_finalize (GObject *object)
{
	GcmUsb *usb = GCM_USB (object);
	GcmUsbPrivate *priv = usb->priv;

	if (priv->ctx != NULL) {
		libusb_set_pollfd_notifiers (usb->priv->ctx, NULL, NULL, NULL);
		gcm_libusb_pollfd_remove_all (usb);
		libusb_exit (priv->ctx);
	}
	if (priv->handle != NULL)
		libusb_close (priv->handle);

	G_OBJECT_CLASS (gcm_usb_parent_class)->finalize (object);
}

/**
 * gcm_usb_new:
 *
 * Return value: a new #GcmUsb object.
 *
 * Since: 2.91.1
 **/
GcmUsb *
gcm_usb_new (void)
{
	GcmUsb *usb;
	usb = g_object_new (GCM_TYPE_USB, NULL);
	return GCM_USB (usb);
}


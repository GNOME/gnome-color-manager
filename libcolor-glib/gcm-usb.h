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

#ifndef __GCM_USB_H
#define __GCM_USB_H

#include <glib-object.h>

#include <libusb-1.0/libusb.h>

G_BEGIN_DECLS

#define GCM_TYPE_USB			(gcm_usb_get_type ())
#define GCM_USB(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), GCM_TYPE_USB, GcmUsb))
#define GCM_IS_USB(o)			(G_TYPE_CHECK_INSTANCE_TYPE ((o), GCM_TYPE_USB))

typedef struct _GcmUsbPrivate		GcmUsbPrivate;
typedef struct _GcmUsb			GcmUsb;
typedef struct _GcmUsbClass		GcmUsbClass;

/* dummy */
#define GCM_USB_ERROR	1

/**
 * GcmSensorError:
 *
 * The error code.
 **/
typedef enum {
	GCM_USB_ERROR_INTERNAL
} GcmUsbError;

struct _GcmUsb
{
	 GObject			 parent;
	 GcmUsbPrivate			*priv;
};

struct _GcmUsbClass
{
	GObjectClass	parent_class;
};

GType			 gcm_usb_get_type			(void);
gboolean		 gcm_usb_load				(GcmUsb		*usb,
								 GError		**error);
gboolean		 gcm_usb_connect			(GcmUsb		*usb,
								 guint		 vendor_id,
								 guint		 product_id,
								 guint		 configuration,
								 guint		 interface,
								 GError		**error);
gboolean		 gcm_usb_get_connected			(GcmUsb		*usb);

void			 gcm_usb_attach_to_context		(GcmUsb		*usb,
								 GMainContext	*context);
libusb_device_handle	*gcm_usb_get_device_handle		(GcmUsb		*usb);
GcmUsb			*gcm_usb_new				(void);

G_END_DECLS

#endif /* __GCM_USB_H */


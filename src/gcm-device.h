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
 * GNU General Public License for more device.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __GCM_DEVICE_H
#define __GCM_DEVICE_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GCM_TYPE_DEVICE			(gcm_device_get_type ())
#define GCM_DEVICE(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), GCM_TYPE_DEVICE, GcmDevice))
#define GCM_DEVICE_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), GCM_TYPE_DEVICE, GcmDeviceClass))
#define GCM_IS_DEVICE(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GCM_TYPE_DEVICE))
#define GCM_IS_DEVICE_CLASS(k)		(G_TYPE_CHECK_CLASS_TYPE ((k), GCM_TYPE_DEVICE))
#define GCM_DEVICE_GET_CLASS(o)		(G_TYPE_INSTANCE_GET_CLASS ((o), GCM_TYPE_DEVICE, GcmDeviceClass))

typedef struct _GcmDevicePrivate	GcmDevicePrivate;
typedef struct _GcmDevice		GcmDevice;
typedef struct _GcmDeviceClass		GcmDeviceClass;

struct _GcmDevice
{
	 GObject			 parent;
	 GcmDevicePrivate		*priv;
};

struct _GcmDeviceClass
{
	GObjectClass	parent_class;
	/* padding for future expansion */
	void (*_gcm_reserved1) (void);
	void (*_gcm_reserved2) (void);
	void (*_gcm_reserved3) (void);
	void (*_gcm_reserved4) (void);
	void (*_gcm_reserved5) (void);
};

typedef enum {
	GCM_DEVICE_TYPE_UNKNOWN,
	GCM_DEVICE_TYPE_DISPLAY,
	GCM_DEVICE_TYPE_SCANNER,
	GCM_DEVICE_TYPE_PRINTER,
	GCM_DEVICE_TYPE_CAMERA,
	GCM_DEVICE_TYPE_LAST
} GcmDeviceType;

GType		 gcm_device_get_type		  	(void);
GcmDevice	*gcm_device_new				(void);
const gchar	*gcm_device_get_id			(GcmDevice		*device);
gboolean	 gcm_device_load			(GcmDevice		*device,
							 GError			**error);
gboolean	 gcm_device_save			(GcmDevice		*device,
							 GError			**error);
GcmDeviceType	 gcm_device_type_from_text		(const gchar		*type);
const gchar	*gcm_device_type_to_text		(GcmDeviceType		 type);

G_END_DECLS

#endif /* __GCM_DEVICE_H */


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
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __GCM_COLOR_DEVICE_H
#define __GCM_COLOR_DEVICE_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GCM_TYPE_COLOR_DEVICE		(gcm_color_device_get_type ())
#define GCM_COLOR_DEVICE(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GCM_TYPE_COLOR_DEVICE, GcmColorDevice))
#define GCM_COLOR_DEVICE_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), GCM_TYPE_COLOR_DEVICE, GcmColorDeviceClass))
#define GCM_IS_COLOR_DEVICE(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GCM_TYPE_COLOR_DEVICE))
#define GCM_IS_COLOR_DEVICE_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GCM_TYPE_COLOR_DEVICE))
#define GCM_COLOR_DEVICE_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GCM_TYPE_COLOR_DEVICE, GcmColorDeviceClass))

typedef struct _GcmColorDevicePrivate	GcmColorDevicePrivate;
typedef struct _GcmColorDevice		GcmColorDevice;
typedef struct _GcmColorDeviceClass	GcmColorDeviceClass;

struct _GcmColorDevice
{
	 GObject			 parent;
	 GcmColorDevicePrivate		*priv;
};

struct _GcmColorDeviceClass
{
	GObjectClass	parent_class;
	void		(* changed)			(void);
	/* padding for future expansion */
	void (*_gcm_reserved1) (void);
	void (*_gcm_reserved2) (void);
	void (*_gcm_reserved3) (void);
	void (*_gcm_reserved4) (void);
	void (*_gcm_reserved5) (void);
};

GType		 gcm_color_device_get_type		(void);
GcmColorDevice	*gcm_color_device_new			(void);

G_END_DECLS

#endif /* __GCM_COLOR_DEVICE_H */


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
 * GNU General Public License for more client.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __GCM_CLIENT_H
#define __GCM_CLIENT_H

#include <glib-object.h>
#include <gdk/gdk.h>

#include "gcm-device.h"

G_BEGIN_DECLS

#define GCM_TYPE_CLIENT			(gcm_client_get_type ())
#define GCM_CLIENT(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), GCM_TYPE_CLIENT, GcmClient))
#define GCM_CLIENT_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), GCM_TYPE_CLIENT, GcmClientClass))
#define GCM_IS_CLIENT(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GCM_TYPE_CLIENT))
#define GCM_IS_CLIENT_CLASS(k)		(G_TYPE_CHECK_CLASS_TYPE ((k), GCM_TYPE_CLIENT))
#define GCM_CLIENT_GET_CLASS(o)		(G_TYPE_INSTANCE_GET_CLASS ((o), GCM_TYPE_CLIENT, GcmClientClass))

typedef struct _GcmClientPrivate	GcmClientPrivate;
typedef struct _GcmClient		GcmClient;
typedef struct _GcmClientClass		GcmClientClass;

struct _GcmClient
{
	 GObject			 parent;
	 GcmClientPrivate		*priv;
};

struct _GcmClientClass
{
	GObjectClass	parent_class;
	void		(* added)				(GcmDevice	*device);
	void		(* removed)				(GcmDevice	*device);
	/* padding for future expansion */
	void (*_gcm_reserved1) (void);
	void (*_gcm_reserved2) (void);
	void (*_gcm_reserved3) (void);
	void (*_gcm_reserved4) (void);
	void (*_gcm_reserved5) (void);
};

GType		 gcm_client_get_type		  		(void);
GcmClient	*gcm_client_new					(void);

GcmDevice	*gcm_client_get_device_by_id			(GcmClient		*client,
								 const gchar		*id);
GcmDevice	*gcm_client_get_device_by_window		(GcmClient		*client,
								 GdkWindow		*window);
gboolean	 gcm_client_delete_device			(GcmClient		*client,
								 GcmDevice		*device,
								 GError			**error);
gboolean	 gcm_client_add_connected			(GcmClient		*client,
								 GError			**error);
gboolean	 gcm_client_add_saved				(GcmClient		*client,
								 GError			**error);
GPtrArray	*gcm_client_get_devices				(GcmClient		*client);

G_END_DECLS

#endif /* __GCM_CLIENT_H */


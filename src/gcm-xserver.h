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

#ifndef __GCM_XSERVER_H
#define __GCM_XSERVER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GCM_TYPE_XSERVER		(gcm_xserver_get_type ())
#define GCM_XSERVER(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), GCM_TYPE_XSERVER, GcmXserver))
#define GCM_XSERVER_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), GCM_TYPE_XSERVER, GcmXserverClass))
#define GCM_IS_XSERVER(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GCM_TYPE_XSERVER))
#define GCM_IS_XSERVER_CLASS(k)		(G_TYPE_CHECK_CLASS_TYPE ((k), GCM_TYPE_XSERVER))
#define GCM_XSERVER_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GCM_TYPE_XSERVER, GcmXserverClass))

typedef struct _GcmXserverPrivate	GcmXserverPrivate;
typedef struct _GcmXserver		GcmXserver;
typedef struct _GcmXserverClass	GcmXserverClass;

struct _GcmXserver
{
	 GObject		 parent;
	 GcmXserverPrivate	*priv;
};

struct _GcmXserverClass
{
	GObjectClass	parent_class;
	/* padding for future expansion */
	void (*_gcm_reserved1) (void);
	void (*_gcm_reserved2) (void);
	void (*_gcm_reserved3) (void);
	void (*_gcm_reserved4) (void);
	void (*_gcm_reserved5) (void);
};

GType		 gcm_xserver_get_type		  		(void);
GcmXserver	*gcm_xserver_new				(void);

/* per screen */
gboolean	 gcm_xserver_get_root_window_profile_data	(GcmXserver		*xserver,
								 guint8			**data,
								 gsize			*length,
								 GError			**error);
gboolean	 gcm_xserver_set_root_window_profile_data	(GcmXserver		*xserver,
								 const guint8		*data,
								 gsize			 length,
								 GError			**error);
gboolean	 gcm_xserver_set_root_window_profile		(GcmXserver		*xserver,
								 const gchar		*filename,
								 GError			**error);

/* per output */
gboolean	 gcm_xserver_get_output_profile_data		(GcmXserver		*xserver,
								 const gchar		*output_name,
								 guint8			**data,
								 gsize			*length,
								 GError			**error);
gboolean	 gcm_xserver_set_output_profile_data		(GcmXserver		*xserver,
								 const gchar		*output_name,
								 const guint8		*data,
								 gsize			 length,
								 GError			**error);
gboolean	 gcm_xserver_set_output_profile			(GcmXserver		*xserver,
								 const gchar		*output_name,
								 const gchar		*filename,
								 GError			**error);

G_END_DECLS

#endif /* __GCM_XSERVER_H */


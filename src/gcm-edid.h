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
 * GNU General Public License for more edid.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __GCM_EDID_H
#define __GCM_EDID_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GCM_TYPE_EDID		(gcm_edid_get_type ())
#define GCM_EDID(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GCM_TYPE_EDID, GcmEdid))
#define GCM_EDID_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), GCM_TYPE_EDID, GcmEdidClass))
#define GCM_IS_EDID(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GCM_TYPE_EDID))
#define GCM_IS_EDID_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GCM_TYPE_EDID))
#define GCM_EDID_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GCM_TYPE_EDID, GcmEdidClass))

typedef struct _GcmEdidPrivate	GcmEdidPrivate;
typedef struct _GcmEdid		GcmEdid;
typedef struct _GcmEdidClass	GcmEdidClass;

struct _GcmEdid
{
	 GObject		 parent;
	 GcmEdidPrivate		*priv;
};

struct _GcmEdidClass
{
	GObjectClass	parent_class;
	/* padding for future expansion */
	void (*_gcm_reserved1) (void);
	void (*_gcm_reserved2) (void);
	void (*_gcm_reserved3) (void);
	void (*_gcm_reserved4) (void);
	void (*_gcm_reserved5) (void);
};

GType		 gcm_edid_get_type		  	(void);
GcmEdid		*gcm_edid_new				(void);
void		 gcm_edid_reset				(GcmEdid		*edid);
gboolean	 gcm_edid_parse				(GcmEdid		*edid,
							 const guint8		*data,
							 GError			**error);

G_END_DECLS

#endif /* __GCM_EDID_H */


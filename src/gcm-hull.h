/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011 Richard Hughes <richard@hughsie.com>
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

#ifndef __GCM_HULL_H
#define __GCM_HULL_H

#include <glib-object.h>
#include <colord.h>

G_BEGIN_DECLS

#define GCM_TYPE_HULL		(gcm_hull_get_type ())
#define GCM_HULL(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GCM_TYPE_HULL, GcmHull))
#define GCM_HULL_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), GCM_TYPE_HULL, GcmHullClass))
#define GCM_IS_HULL(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GCM_TYPE_HULL))
#define GCM_IS_HULL_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GCM_TYPE_HULL))
#define GCM_HULL_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GCM_TYPE_HULL, GcmHullClass))

typedef struct _GcmHullPrivate	GcmHullPrivate;
typedef struct _GcmHull		GcmHull;
typedef struct _GcmHullClass	GcmHullClass;

struct _GcmHull
{
	 GObject		 parent;
	 GcmHullPrivate		*priv;
};

struct _GcmHullClass
{
	GObjectClass	parent_class;
};

GType		 gcm_hull_get_type		  	(void);
GcmHull		*gcm_hull_new				(void);
void		 gcm_hull_add_vertex			(GcmHull	*hull,
							 CdColorXYZ	*xyz,
							 CdColorRGB	*color);
void		 gcm_hull_add_face			(GcmHull	*hull,
							 const guint	*data,
							 gsize		 size);
gchar		*gcm_hull_export_to_ply			(GcmHull	*hull);
guint		 gcm_hull_get_flags			(GcmHull	*hull);
void		 gcm_hull_set_flags			(GcmHull	*hull,
							 guint		 flags);

G_END_DECLS

#endif /* __GCM_HULL_H */


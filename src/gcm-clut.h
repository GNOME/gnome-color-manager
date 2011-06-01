/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009-2010 Richard Hughes <richard@hughsie.com>
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

#ifndef __GCM_CLUT_H
#define __GCM_CLUT_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GCM_TYPE_CLUT		(gcm_clut_get_type ())
#define GCM_CLUT(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GCM_TYPE_CLUT, GcmClut))
#define GCM_CLUT_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), GCM_TYPE_CLUT, GcmClutClass))
#define GCM_IS_CLUT(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GCM_TYPE_CLUT))
#define GCM_IS_CLUT_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GCM_TYPE_CLUT))
#define GCM_CLUT_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GCM_TYPE_CLUT, GcmClutClass))

typedef struct _GcmClutPrivate	GcmClutPrivate;
typedef struct _GcmClut		GcmClut;
typedef struct _GcmClutClass	GcmClutClass;

struct _GcmClut
{
	 GObject		 parent;
	 GcmClutPrivate		*priv;
};

struct _GcmClutClass
{
	GObjectClass	parent_class;
};

typedef struct {
	guint32		 red;
	guint32		 green;
	guint32		 blue;
} GcmClutData;

GType		 gcm_clut_get_type		  	(void);
GcmClut		*gcm_clut_new				(void);
GPtrArray	*gcm_clut_get_array			(GcmClut		*clut);
gboolean	 gcm_clut_set_source_array		(GcmClut		*clut,
							 GPtrArray		*array);
gboolean	 gcm_clut_reset				(GcmClut		*clut);
void		 gcm_clut_print				(GcmClut		*clut);
guint		 gcm_clut_get_size			(GcmClut		*clut);

G_END_DECLS

#endif /* __GCM_CLUT_H */


/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
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

#ifndef GCM_CELL_RENDERER_PROFILE_DATE_H
#define GCM_CELL_RENDERER_PROFILE_DATE_H

#include <glib-object.h>
#include <gtk/gtk.h>
#include <colord.h>

#define GCM_TYPE_CELL_RENDERER_PROFILE_DATE		(gcm_cell_renderer_profile_date_get_type())
#define GCM_CELL_RENDERER_PROFILE_DATE(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj), GCM_TYPE_CELL_RENDERER_PROFILE_DATE, GcmCellRendererProfileDate))
#define GCM_CELL_RENDERER_PROFILE_DATE_CLASS(cls)	(G_TYPE_CHECK_CLASS_CAST((cls), GCM_TYPE_CELL_RENDERER_PROFILE_DATE, GcmCellRendererProfileDateClass))
#define GCM_IS_CELL_RENDERER_PROFILE_DATE(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), GCM_TYPE_CELL_RENDERER_PROFILE_DATE))
#define GCM_IS_CELL_RENDERER_PROFILE_DATE_CLASS(cls)	(G_TYPE_CHECK_CLASS_TYPE((cls), GCM_TYPE_CELL_RENDERER_PROFILE_DATE))
#define GCM_CELL_RENDERER_PROFILE_DATE_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), GCM_TYPE_CELL_RENDERER_PROFILE_DATE, GcmCellRendererProfileDateClass))

G_BEGIN_DECLS

typedef struct _GcmCellRendererProfileDate		GcmCellRendererProfileDate;
typedef struct _GcmCellRendererProfileDateClass		GcmCellRendererProfileDateClass;

struct _GcmCellRendererProfileDate
{
	GtkCellRendererText	 parent;
	CdProfile		*profile;
	gchar			*markup;
};

struct _GcmCellRendererProfileDateClass
{
	GtkCellRendererTextClass parent_class;
};

GType		 gcm_cell_renderer_profile_date_get_type	(void);
GtkCellRenderer	*gcm_cell_renderer_profile_date_new		(void);

G_END_DECLS

#endif /* GCM_CELL_RENDERER_PROFILE_DATE_H */


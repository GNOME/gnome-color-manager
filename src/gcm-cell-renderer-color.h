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

#ifndef GCM_CELL_RENDERER_COLOR_H
#define GCM_CELL_RENDERER_COLOR_H

#include <glib-object.h>
#include <gtk/gtk.h>
#include <colord.h>

#define GCM_TYPE_CELL_RENDERER_COLOR		(gcm_cell_renderer_color_get_type())
#define GCM_CELL_RENDERER_COLOR(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj), GCM_TYPE_CELL_RENDERER_COLOR, GcmCellRendererColor))
#define GCM_CELL_RENDERER_COLOR_CLASS(cls)	(G_TYPE_CHECK_CLASS_CAST((cls), GCM_TYPE_CELL_RENDERER_COLOR, GcmCellRendererColorClass))
#define GCM_IS_CELL_RENDERER_COLOR(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), GCM_TYPE_CELL_RENDERER_COLOR))
#define GCM_IS_CELL_RENDERER_COLOR_CLASS(cls)	(G_TYPE_CHECK_CLASS_TYPE((cls), GCM_TYPE_CELL_RENDERER_COLOR))
#define GCM_CELL_RENDERER_COLOR_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), GCM_TYPE_CELL_RENDERER_COLOR, GcmCellRendererColorClass))

G_BEGIN_DECLS

typedef struct _GcmCellRendererColor		GcmCellRendererColor;
typedef struct _GcmCellRendererColorClass	GcmCellRendererColorClass;

struct _GcmCellRendererColor
{
	GtkCellRendererPixbuf	 parent;
	CdColorLab		*color;
	CdProfile		*profile;
	gchar			*icon_name;
};

struct _GcmCellRendererColorClass
{
	GtkCellRendererPixbufClass parent_class;
};

GType		 gcm_cell_renderer_color_get_type	(void);
GtkCellRenderer	*gcm_cell_renderer_color_new		(void);

G_END_DECLS

#endif /* GCM_CELL_RENDERER_COLOR_H */


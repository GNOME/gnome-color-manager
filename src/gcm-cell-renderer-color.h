/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#pragma once

#include <glib-object.h>
#include <gtk/gtk.h>
#include <colord.h>

#define GCM_TYPE_CELL_RENDERER_COLOR		(gcm_cell_renderer_color_get_type())
#define GCM_CELL_RENDERER_COLOR(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj), GCM_TYPE_CELL_RENDERER_COLOR, GcmCellRendererColor))
#define GCM_CELL_RENDERER_COLOR_CLASS(cls)	(G_TYPE_CHECK_CLASS_CAST((cls), GCM_TYPE_CELL_RENDERER_COLOR, GcmCellRendererColorClass))
#define GCM_IS_CELL_RENDERER_COLOR(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), GCM_TYPE_CELL_RENDERER_COLOR))
#define GCM_IS_CELL_RENDERER_COLOR_CLASS(cls)	(G_TYPE_CHECK_CLASS_TYPE((cls), GCM_TYPE_CELL_RENDERER_COLOR))
#define GCM_CELL_RENDERER_COLOR_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), GCM_TYPE_CELL_RENDERER_COLOR, GcmCellRendererColorClass))

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

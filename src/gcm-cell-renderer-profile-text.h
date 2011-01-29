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

#ifndef GCM_CELL_RENDERER_PROFILE_TEXT_H
#define GCM_CELL_RENDERER_PROFILE_TEXT_H

#include <glib-object.h>
#include <gtk/gtk.h>
#include <colord.h>

#define GCM_TYPE_CELL_RENDERER_PROFILE_TEXT		(gcm_cell_renderer_profile_text_get_type())
#define GCM_CELL_RENDERER_PROFILE_TEXT(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj), GCM_TYPE_CELL_RENDERER_PROFILE_TEXT, GcmCellRendererProfileText))
#define GCM_CELL_RENDERER_PROFILE_TEXT_CLASS(cls)	(G_TYPE_CHECK_CLASS_CAST((cls), GCM_TYPE_CELL_RENDERER_PROFILE_TEXT, GcmCellRendererProfileTextClass))
#define GCM_IS_CELL_RENDERER_PROFILE_TEXT(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), GCM_TYPE_CELL_RENDERER_PROFILE_TEXT))
#define GCM_IS_CELL_RENDERER_PROFILE_TEXT_CLASS(cls)	(G_TYPE_CHECK_CLASS_TYPE((cls), GCM_TYPE_CELL_RENDERER_PROFILE_TEXT))
#define GCM_CELL_RENDERER_PROFILE_TEXT_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), GCM_TYPE_CELL_RENDERER_PROFILE_TEXT, GcmCellRendererProfileTextClass))

G_BEGIN_DECLS

typedef struct _GcmCellRendererProfileText		GcmCellRendererProfileText;
typedef struct _GcmCellRendererProfileTextClass		GcmCellRendererProfileTextClass;

struct _GcmCellRendererProfileText
{
	GtkCellRendererText	 parent;
	CdProfile		*profile;
	gboolean		 is_default;
	gchar			*markup;
};

struct _GcmCellRendererProfileTextClass
{
	GtkCellRendererTextClass parent_class;
};

GType		 gcm_cell_renderer_profile_text_get_type	(void);
GtkCellRenderer	*gcm_cell_renderer_profile_text_new		(void);

G_END_DECLS

#endif /* GCM_CELL_RENDERER_PROFILE_TEXT_H */


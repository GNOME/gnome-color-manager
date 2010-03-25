/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2006-2010 Richard Hughes <richard@hughsie.com>
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

#ifndef __GCM_TRC_WIDGET_H__
#define __GCM_TRC_WIDGET_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GCM_TYPE_TRC_WIDGET		(gcm_trc_widget_get_type ())
#define GCM_TRC_WIDGET(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GCM_TYPE_TRC_WIDGET, GcmTrcWidget))
#define GCM_TRC_WIDGET_CLASS(obj)	(G_TYPE_CHECK_CLASS_CAST ((obj), GCM_TRC_WIDGET, GcmTrcWidgetClass))
#define GCM_IS_TRC_WIDGET(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GCM_TYPE_TRC_WIDGET))
#define GCM_IS_TRC_WIDGET_CLASS(obj)	(G_TYPE_CHECK_CLASS_TYPE ((obj), EFF_TYPE_TRC_WIDGET))
#define GCM_TRC_WIDGET_GET_CLASS	(G_TYPE_INSTANCE_GET_CLASS ((obj), GCM_TYPE_TRC_WIDGET, GcmTrcWidgetClass))

typedef struct GcmTrcWidget		GcmTrcWidget;
typedef struct GcmTrcWidgetClass	GcmTrcWidgetClass;
typedef struct GcmTrcWidgetPrivate	GcmTrcWidgetPrivate;

struct GcmTrcWidget
{
	GtkDrawingArea		 parent;
	GcmTrcWidgetPrivate	*priv;
};

struct GcmTrcWidgetClass
{
	GtkDrawingAreaClass parent_class;
};

GType		 gcm_trc_widget_get_type		(void);
GtkWidget	*gcm_trc_widget_new			(void);

G_END_DECLS

#endif

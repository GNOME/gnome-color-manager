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

#ifndef __GCM_HULL_WIDGET_H__
#define __GCM_HULL_WIDGET_H__

#include <gtk/gtk.h>
#include <clutter-gtk/clutter-gtk.h>

#include "gcm-profile.h"

G_BEGIN_DECLS

#define GCM_TYPE_HULL_WIDGET		(gcm_hull_widget_get_type ())
#define GCM_HULL_WIDGET(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GCM_TYPE_HULL_WIDGET, GcmHullWidget))
#define GCM_HULL_WIDGET_CLASS(obj)	(G_TYPE_CHECK_CLASS_CAST ((obj), GCM_HULL_WIDGET, GcmHullWidgetClass))
#define GCM_IS_HULL_WIDGET(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GCM_TYPE_HULL_WIDGET))
#define GCM_IS_HULL_WIDGET_CLASS(obj)	(G_TYPE_CHECK_CLASS_TYPE ((obj), EFF_TYPE_HULL_WIDGET))
#define GCM_HULL_WIDGET_GET_CLASS	(G_TYPE_INSTANCE_GET_CLASS ((obj), GCM_TYPE_HULL_WIDGET, GcmHullWidgetClass))

typedef struct GcmHullWidget		GcmHullWidget;
typedef struct GcmHullWidgetClass	GcmHullWidgetClass;
typedef struct GcmHullWidgetPrivate	GcmHullWidgetPrivate;

struct GcmHullWidget
{
	GtkClutterEmbed		 parent;
	GcmHullWidgetPrivate	*priv;
};

struct GcmHullWidgetClass
{
	GtkClutterEmbedClass	 parent_class;
};

GType		 gcm_hull_widget_get_type	(void);
GtkWidget	*gcm_hull_widget_new		(void);
void		 gcm_hull_widget_clear		(GcmHullWidget	*hull_widget);
gboolean	 gcm_hull_widget_add		(GcmHullWidget	*hull_widget,
						 GcmProfile	*profile);

G_END_DECLS

#endif

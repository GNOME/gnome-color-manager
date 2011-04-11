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

#ifndef __GCM_NAMED_COLOR_H
#define __GCM_NAMED_COLOR_H

#include <glib-object.h>
#include <gio/gio.h>
#include <colord.h>

G_BEGIN_DECLS

#define GCM_TYPE_NAMED_COLOR		(gcm_named_color_get_type ())
#define GCM_NAMED_COLOR(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GCM_TYPE_NAMED_COLOR, GcmNamedColor))
#define GCM_NAMED_COLOR_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), GCM_TYPE_NAMED_COLOR, GcmNamedColorClass))
#define GCM_IS_NAMED_COLOR(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GCM_TYPE_NAMED_COLOR))

typedef struct _GcmNamedColorPrivate GcmNamedColorPrivate;

typedef struct
{
	 GObject		 parent;
	 GcmNamedColorPrivate	*priv;
} GcmNamedColor;

typedef struct
{
	GObjectClass		 parent_class;
} GcmNamedColorClass;

GType		 gcm_named_color_get_type	(void);
GcmNamedColor	*gcm_named_color_new		(void);

const gchar	*gcm_named_color_get_title	(GcmNamedColor	*named_color);
void		 gcm_named_color_set_title	(GcmNamedColor	*named_color,
						 const gchar	*title);
const CdColorXYZ *gcm_named_color_get_value	(GcmNamedColor	*named_color);
void		 gcm_named_color_set_value	(GcmNamedColor	*named_color,
						 const CdColorXYZ *value);

G_END_DECLS

#endif /* __GCM_NAMED_COLOR_H */


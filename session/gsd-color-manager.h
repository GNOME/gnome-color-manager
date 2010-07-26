/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __GSD_COLOR_MANAGER_H
#define __GSD_COLOR_MANAGER_H

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define GSD_TYPE_COLOR_MANAGER		(gsd_color_manager_get_type ())
#define GSD_COLOR_MANAGER(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GSD_TYPE_COLOR_MANAGER, GsdColorManager))
#define GSD_COLOR_MANAGER_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST ((k), GSD_TYPE_COLOR_MANAGER, GsdColorManagerClass))
#define GSD_IS_COLOR_MANAGER(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GSD_TYPE_COLOR_MANAGER))
#define GSD_IS_COLOR_MANAGER_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GSD_TYPE_COLOR_MANAGER))
#define GSD_COLOR_MANAGER_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GSD_TYPE_COLOR_MANAGER, GsdColorManagerClass))

typedef struct GsdColorManagerPrivate GsdColorManagerPrivate;

typedef struct
{
	GObject parent;
	GsdColorManagerPrivate *priv;
} GsdColorManager;

typedef struct
{
	GObjectClass parent_class;
} GsdColorManagerClass;

GType		 gsd_color_manager_get_type	(void) G_GNUC_CONST;
GsdColorManager	*gsd_color_manager_new		(void);
gboolean	 gsd_color_manager_start	(GsdColorManager	*manager,
						 GError			**error);
void		 gsd_color_manager_stop		(GsdColorManager	*manager);

G_END_DECLS

#endif /* __GSD_COLOR_MANAGER_H */

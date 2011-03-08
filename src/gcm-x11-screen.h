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

#ifndef __GCM_X11_SCREEN_H
#define __GCM_X11_SCREEN_H

#include <glib-object.h>
#include <gdk/gdk.h>

#include "gcm-x11-output.h"

G_BEGIN_DECLS

#define GCM_TYPE_X11_SCREEN		(gcm_x11_screen_get_type ())
#define GCM_X11_SCREEN(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GCM_TYPE_X11_SCREEN, GcmX11Screen))
#define GCM_IS_X11_SCREEN(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GCM_TYPE_X11_SCREEN))

#define GCM_X11_SCREEN_ERROR		1
#define GCM_X11_SCREEN_ERROR_INTERNAL	0

typedef struct _GcmX11ScreenPrivate	GcmX11ScreenPrivate;
typedef struct _GcmX11Screen		GcmX11Screen;
typedef struct _GcmX11ScreenClass	GcmX11ScreenClass;

struct _GcmX11Screen
{
	 GObject		 parent;
	 GcmX11ScreenPrivate	*priv;
};

struct _GcmX11ScreenClass
{
	GObjectClass		 parent_class;
	void			(* changed)		(GcmX11Screen		*screen);
	void			(* added)		(GcmX11Screen		*screen,
							 GcmX11Output		*output);
	void			(* removed)		(GcmX11Screen		*screen,
							 GcmX11Output		*output);
};

GType		 gcm_x11_screen_get_type		(void);
GcmX11Screen	*gcm_x11_screen_new			(void);

gboolean	 gcm_x11_screen_assign			(GcmX11Screen		*screen,
							 GdkScreen		*gdk_screen,
							 GError			**error);
gboolean	 gcm_x11_screen_refresh			(GcmX11Screen		*screen,
							 GError			**error);
GPtrArray	*gcm_x11_screen_get_outputs		(GcmX11Screen		*screen,
							 GError			**error);
GcmX11Output	*gcm_x11_screen_get_output_by_name	(GcmX11Screen		*screen,
							 const gchar		*name,
							 GError			**error);
GcmX11Output	*gcm_x11_screen_get_output_by_edid	(GcmX11Screen		*screen,
							 const gchar		*edid_md5,
							 GError			**error);
GcmX11Output	*cd_x11_screen_get_output_by_window	(GcmX11Screen		*screen,
							 GdkWindow		*window);
gboolean	 gcm_x11_screen_get_profile_data	(GcmX11Screen		*screen,
							 guint8			**data,
							 gsize			*length,
							 GError			**error);
gboolean	 gcm_x11_screen_set_profile_data	(GcmX11Screen		*screen,
							 const guint8		*data,
							 gsize			 length,
							 GError			**error);
gboolean	 gcm_x11_screen_set_profile		(GcmX11Screen		*screen,
							 const gchar		*filename,
							 GError			**error);
gboolean	 gcm_x11_screen_remove_profile		(GcmX11Screen		*screen,
							 GError			**error);
gboolean	 gcm_x11_screen_set_protocol_version	(GcmX11Screen		*screen,
							 guint			 major,
							 guint			 minor,
							 GError			**error);
gboolean	 gcm_x11_screen_remove_protocol_version	(GcmX11Screen		*screen,
							 GError			**error);
gboolean	 gcm_x11_screen_get_protocol_version	(GcmX11Screen		*screen,
							 guint			*major,
							 guint			*minor,
							 GError			**error);
void		 gcm_x11_screen_get_randr_version	(GcmX11Screen		*screen,
							 guint			*major,
							 guint			*minor);

G_END_DECLS

#endif /* __GCM_X11_SCREEN_H */


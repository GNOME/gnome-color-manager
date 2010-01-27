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

#ifndef __GCM_SCREEN_H
#define __GCM_SCREEN_H

#include <glib-object.h>
#include <gio/gio.h>
#include <libgnomeui/gnome-rr.h>

G_BEGIN_DECLS

#define GCM_TYPE_SCREEN		(gcm_screen_get_type ())
#define GCM_SCREEN(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GCM_TYPE_SCREEN, GcmScreen))
#define GCM_SCREEN_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), GCM_TYPE_SCREEN, GcmScreenClass))
#define GCM_IS_SCREEN(o)	(G_TYPE_CHECK_INSTANCE_TYPE ((o), GCM_TYPE_SCREEN))
#define GCM_IS_SCREEN_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GCM_TYPE_SCREEN))
#define GCM_SCREEN_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GCM_TYPE_SCREEN, GcmScreenClass))
#define GCM_SCREEN_ERROR	(gcm_screen_error_quark ())
#define GCM_SCREEN_TYPE_ERROR	(gcm_screen_error_get_type ())

typedef struct _GcmScreenPrivate	GcmScreenPrivate;
typedef struct _GcmScreen		GcmScreen;
typedef struct _GcmScreenClass		GcmScreenClass;

/**
 * GcmScreenError:
 * @GCM_SCREEN_ERROR_FAILED: the transaction failed for an unknown reason
 *
 * Errors that can be thrown
 */
typedef enum
{
	GCM_SCREEN_ERROR_FAILED
} GcmScreenError;

struct _GcmScreen
{
	 GObject		 parent;
	 GcmScreenPrivate	*priv;
};

struct _GcmScreenClass
{
	GObjectClass	parent_class;

	/* signals */
	void		(* outputs_changed)		(GcmScreen		*screen);
	/* padding for future expansion */
	void (*_gcm_reserved1) (void);
	void (*_gcm_reserved2) (void);
	void (*_gcm_reserved3) (void);
	void (*_gcm_reserved4) (void);
	void (*_gcm_reserved5) (void);
};

GType		 gcm_screen_get_type		  	(void);
GcmScreen	*gcm_screen_new				(void);

GnomeRROutput	*gcm_screen_get_output_by_name		(GcmScreen		*screen,
							 const gchar		*output_name,
							 GError			**error);
GnomeRROutput	**gcm_screen_get_outputs		(GcmScreen		*screen,
							 GError			**error);
G_END_DECLS

#endif /* __GCM_SCREEN_H */


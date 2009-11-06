/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 Richard Hughes <richard@hughsie.com>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __GCM_DBUS_H
#define __GCM_DBUS_H

#include <glib-object.h>
#include <dbus/dbus-glib.h>

G_BEGIN_DECLS

#define GCM_TYPE_DBUS		(gcm_dbus_get_type ())
#define GCM_DBUS(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GCM_TYPE_DBUS, GcmDbus))
#define GCM_DBUS_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), GCM_TYPE_DBUS, GcmDbusClass))
#define PK_IS_DBUS(o)	 	(G_TYPE_CHECK_INSTANCE_TYPE ((o), GCM_TYPE_DBUS))
#define PK_IS_DBUS_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GCM_TYPE_DBUS))
#define GCM_DBUS_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GCM_TYPE_DBUS, GcmDbusClass))
#define GCM_DBUS_ERROR		(gcm_dbus_error_quark ())
#define GCM_DBUS_TYPE_ERROR	(gcm_dbus_error_get_type ())

typedef struct GcmDbusPrivate GcmDbusPrivate;

typedef struct
{
	 GObject		 parent;
	 GcmDbusPrivate	*priv;
} GcmDbus;

typedef struct
{
	GObjectClass	parent_class;
} GcmDbusClass;

typedef enum
{
	GCM_DBUS_ERROR_FAILED,
	GCM_DBUS_ERROR_INTERNAL_ERROR,
	GCM_DBUS_ERROR_LAST
} GcmDbusError;

GQuark		 gcm_dbus_error_quark			(void);
GType		 gcm_dbus_error_get_type		(void);
GType		 gcm_dbus_get_type			(void);
GcmDbus		*gcm_dbus_new				(void);

/* org.gnome.ColorManager */
void		 gcm_dbus_get_profiles_for_device	(GcmDbus	*dbus,
							 const gchar	*sysfs_path,
							 const gchar	*options,
							 DBusGMethodInvocation *context);

G_END_DECLS

#endif /* __GCM_DBUS_H */

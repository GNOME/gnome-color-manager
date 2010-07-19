/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009-2010 Richard Hughes <richard@hughsie.com>
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

#if !defined (__LIBCOLOR_GLIB_H_INSIDE__) && !defined (LIBCOLOR_GLIB_COMPILATION)
#error "Only <libcolor-glib.h> can be included directly."
#endif

#ifndef __GCM_XYZ_H
#define __GCM_XYZ_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GCM_TYPE_XYZ		(gcm_xyz_get_type ())
#define GCM_XYZ(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GCM_TYPE_XYZ, GcmXyz))
#define GCM_XYZ_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), GCM_TYPE_XYZ, GcmXyzClass))
#define GCM_IS_XYZ(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GCM_TYPE_XYZ))
#define GCM_IS_XYZ_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GCM_TYPE_XYZ))
#define GCM_XYZ_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GCM_TYPE_XYZ, GcmXyzClass))

typedef struct _GcmXyzPrivate	GcmXyzPrivate;
typedef struct _GcmXyz		GcmXyz;
typedef struct _GcmXyzClass	GcmXyzClass;

struct _GcmXyz
{
	 GObject		 parent;
	 GcmXyzPrivate		*priv;
};

struct _GcmXyzClass
{
	GObjectClass	parent_class;
	/* padding for future expansion */
	void (*_gcm_reserved1) (void);
	void (*_gcm_reserved2) (void);
	void (*_gcm_reserved3) (void);
	void (*_gcm_reserved4) (void);
	void (*_gcm_reserved5) (void);
};

GType		 gcm_xyz_get_type		  	(void);
GcmXyz		*gcm_xyz_new				(void);
gdouble		 gcm_xyz_get_x				(GcmXyz		*xyz);
gdouble		 gcm_xyz_get_y				(GcmXyz		*xyz);
gdouble		 gcm_xyz_get_z				(GcmXyz		*xyz);
void		 gcm_xyz_print				(GcmXyz		*xyz);
void		 gcm_xyz_clear				(GcmXyz		*xyz);

G_END_DECLS

#endif /* __GCM_XYZ_H */


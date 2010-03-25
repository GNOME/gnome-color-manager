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

#ifndef __GCM_COLORIMETER_H
#define __GCM_COLORIMETER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GCM_TYPE_COLORIMETER		(gcm_colorimeter_get_type ())
#define GCM_COLORIMETER(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GCM_TYPE_COLORIMETER, GcmColorimeter))
#define GCM_COLORIMETER_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), GCM_TYPE_COLORIMETER, GcmColorimeterClass))
#define GCM_IS_COLORIMETER(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GCM_TYPE_COLORIMETER))
#define GCM_IS_COLORIMETER_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GCM_TYPE_COLORIMETER))
#define GCM_COLORIMETER_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GCM_TYPE_COLORIMETER, GcmColorimeterClass))

typedef struct _GcmColorimeterPrivate	GcmColorimeterPrivate;
typedef struct _GcmColorimeter		GcmColorimeter;
typedef struct _GcmColorimeterClass	GcmColorimeterClass;

struct _GcmColorimeter
{
	 GObject			 parent;
	 GcmColorimeterPrivate		*priv;
};

struct _GcmColorimeterClass
{
	GObjectClass	parent_class;
	void		(* changed)			(void);
	/* padding for future expansion */
	void (*_gcm_reserved1) (void);
	void (*_gcm_reserved2) (void);
	void (*_gcm_reserved3) (void);
	void (*_gcm_reserved4) (void);
	void (*_gcm_reserved5) (void);
};

typedef enum {
	GCM_COLORIMETER_KIND_HUEY,
	GCM_COLORIMETER_KIND_COLOR_MUNKI,
	GCM_COLORIMETER_KIND_SPYDER,
	GCM_COLORIMETER_KIND_DTP20,
	GCM_COLORIMETER_KIND_DTP22,
	GCM_COLORIMETER_KIND_DTP41,
	GCM_COLORIMETER_KIND_DTP51,
	GCM_COLORIMETER_KIND_SPECTRO_SCAN,
	GCM_COLORIMETER_KIND_I1_PRO,
	GCM_COLORIMETER_KIND_COLORIMTRE_HCFR,
	GCM_COLORIMETER_KIND_UNKNOWN
} GcmColorimeterKind;

GType			 gcm_colorimeter_get_type		(void);
GcmColorimeter		*gcm_colorimeter_new			(void);

/* accessors */
const gchar		*gcm_colorimeter_get_model		(GcmColorimeter		*colorimeter);
const gchar		*gcm_colorimeter_get_vendor		(GcmColorimeter		*colorimeter);
gboolean		 gcm_colorimeter_get_present		(GcmColorimeter		*colorimeter);
GcmColorimeterKind	 gcm_colorimeter_get_kind		(GcmColorimeter		*colorimeter);
gboolean		 gcm_colorimeter_supports_display	(GcmColorimeter 	*colorimeter);
gboolean		 gcm_colorimeter_supports_projector	(GcmColorimeter 	*colorimeter);
gboolean		 gcm_colorimeter_supports_printer	(GcmColorimeter		*colorimeter);
const gchar		*gcm_colorimeter_kind_to_string		(GcmColorimeterKind	 colorimeter_kind);
GcmColorimeterKind	 gcm_colorimeter_kind_from_string	(const gchar		*colorimeter_kind);

G_END_DECLS

#endif /* __GCM_COLORIMETER_H */


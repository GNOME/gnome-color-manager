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

#ifndef __GCM_EXIF_H
#define __GCM_EXIF_H

#include <glib-object.h>
#include <gio/gio.h>
#include <colord.h>

G_BEGIN_DECLS

#define GCM_TYPE_EXIF		(gcm_exif_get_type ())
#define GCM_EXIF(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GCM_TYPE_EXIF, GcmExif))
#define GCM_EXIF_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), GCM_TYPE_EXIF, GcmExifClass))
#define GCM_IS_EXIF(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GCM_TYPE_EXIF))
#define GCM_IS_EXIF_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GCM_TYPE_EXIF))
#define GCM_EXIF_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GCM_TYPE_EXIF, GcmExifClass))

typedef struct _GcmExifPrivate	GcmExifPrivate;
typedef struct _GcmExif		GcmExif;
typedef struct _GcmExifClass	GcmExifClass;

struct _GcmExif
{
	 GObject		 parent;
	 GcmExifPrivate		*priv;
};

struct _GcmExifClass
{
	GObjectClass	parent_class;
};

typedef enum
{
	GCM_EXIF_ERROR_NO_DATA,
	GCM_EXIF_ERROR_NO_SUPPORT,
	GCM_EXIF_ERROR_INTERNAL
} GcmExifError;

/* dummy */
#define GCM_EXIF_ERROR	1

GType		 gcm_exif_get_type		  	(void);
GcmExif		*gcm_exif_new				(void);

const gchar	*gcm_exif_get_manufacturer		(GcmExif	*exif);
const gchar	*gcm_exif_get_model			(GcmExif	*exif);
const gchar	*gcm_exif_get_serial			(GcmExif	*exif);
CdDeviceKind	 gcm_exif_get_device_kind		(GcmExif	*exif);
gboolean	 gcm_exif_parse				(GcmExif	*exif,
							 GFile		*file,
							 GError		**error);

G_END_DECLS

#endif /* __GCM_EXIF_H */


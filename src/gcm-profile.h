/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009 Richard Hughes <richard@hughsie.com>
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
 * GNU General Public License for more profile.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __GCM_PROFILE_H
#define __GCM_PROFILE_H

#include <glib-object.h>
#include "gcm-clut.h"

G_BEGIN_DECLS

#define GCM_TYPE_PROFILE		(gcm_profile_get_type ())
#define GCM_PROFILE(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), GCM_TYPE_PROFILE, GcmProfile))
#define GCM_PROFILE_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), GCM_TYPE_PROFILE, GcmProfileClass))
#define GCM_IS_PROFILE(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GCM_TYPE_PROFILE))
#define GCM_IS_PROFILE_CLASS(k)		(G_TYPE_CHECK_CLASS_TYPE ((k), GCM_TYPE_PROFILE))
#define GCM_PROFILE_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GCM_TYPE_PROFILE, GcmProfileClass))

typedef struct _GcmProfilePrivate	GcmProfilePrivate;
typedef struct _GcmProfile		GcmProfile;
typedef struct _GcmProfileClass		GcmProfileClass;

struct _GcmProfile
{
	 GObject		 parent;
	 GcmProfilePrivate	*priv;
};

struct _GcmProfileClass
{
	GObjectClass	 parent_class;
	gboolean	 (*parse_data)		(GcmProfile	*profile,
						 const guint8	*data,
						 gsize		 length,
						 GError		**error);
	GcmClut		*(*generate_vcgt)	(GcmProfile	*profile,
						 guint		 size);
	GcmClut		*(*generate_curve)	(GcmProfile	*profile,
						 guint		 size);

	/* padding for future expansion */
	void (*_gcm_reserved1) (void);
	void (*_gcm_reserved2) (void);
	void (*_gcm_reserved3) (void);
	void (*_gcm_reserved4) (void);
	void (*_gcm_reserved5) (void);
};

typedef enum {
	GCM_PROFILE_TYPE_INPUT_DEVICE,
	GCM_PROFILE_TYPE_DISPLAY_DEVICE,
	GCM_PROFILE_TYPE_OUTPUT_DEVICE,
	GCM_PROFILE_TYPE_DEVICELINK,
	GCM_PROFILE_TYPE_COLORSPACE_CONVERSION,
	GCM_PROFILE_TYPE_ABSTRACT,
	GCM_PROFILE_TYPE_NAMED_COLOR,
	GCM_PROFILE_TYPE_UNKNOWN
} GcmProfileType;

typedef enum {
	GCM_PROFILE_COLORSPACE_XYZ,
	GCM_PROFILE_COLORSPACE_LAB,
	GCM_PROFILE_COLORSPACE_LUV,
	GCM_PROFILE_COLORSPACE_YCBCR,
	GCM_PROFILE_COLORSPACE_YXY,
	GCM_PROFILE_COLORSPACE_RGB,
	GCM_PROFILE_COLORSPACE_GRAY,
	GCM_PROFILE_COLORSPACE_HSV,
	GCM_PROFILE_COLORSPACE_CMYK,
	GCM_PROFILE_COLORSPACE_CMY,
	GCM_PROFILE_COLORSPACE_UNKNOWN
} GcmProfileColorspace;

GType		 gcm_profile_get_type		  	(void);
GcmProfile	*gcm_profile_new			(void);
GcmProfile	*gcm_profile_default_new		(void);
gboolean	 gcm_profile_parse			(GcmProfile	*profile,
							 const gchar	*filename,
							 GError		**error);
gboolean	 gcm_profile_parse_data			(GcmProfile	*profile,
							 const guint8	*data,
							 gsize		 length,
							 GError		**error);
GcmClut		*gcm_profile_generate_vcgt		(GcmProfile	*profile,
							 guint		 size);
GcmClut		*gcm_profile_generate_curve		(GcmProfile	*profile,
							 guint		 size);
const gchar	*gcm_profile_type_to_text		(GcmProfileType	 type);
const gchar	*gcm_profile_colorspace_to_text		(GcmProfileColorspace type);

G_END_DECLS

#endif /* __GCM_PROFILE_H */


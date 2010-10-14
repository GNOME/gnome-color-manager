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

#ifndef __GCM_PROFILE_H
#define __GCM_PROFILE_H

#include <glib-object.h>
#include <gio/gio.h>

#include "gcm-clut.h"
#include "gcm-enum.h"
#include "gcm-color.h"

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

	/* padding for future expansion */
	void (*_gcm_reserved1) (void);
	void (*_gcm_reserved2) (void);
	void (*_gcm_reserved3) (void);
	void (*_gcm_reserved4) (void);
	void (*_gcm_reserved5) (void);
};

GType		 gcm_profile_get_type		  	(void);
GcmProfile	*gcm_profile_new			(void);
gboolean	 gcm_profile_parse			(GcmProfile	*profile,
							 GFile		*file,
							 GError		**error);
gboolean	 gcm_profile_parse_data			(GcmProfile	*profile,
							 const guint8	*data,
							 gsize		 length,
							 GError		**error);
gboolean	 gcm_profile_save			(GcmProfile	*profile,
							 const gchar	*filename,
							 GError		**error);
gpointer	 gcm_profile_get_handle			(GcmProfile	*profile);
const gchar	*gcm_profile_get_checksum		(GcmProfile	*profile);
gboolean	 gcm_profile_get_can_delete		(GcmProfile	*profile);
GcmClut		*gcm_profile_generate_vcgt		(GcmProfile	*profile,
							 guint		 size);
GcmClut		*gcm_profile_generate_curve		(GcmProfile	*profile,
							 guint		 size);
gboolean	 gcm_profile_set_vcgt_from_data		(GcmProfile	*profile,
							 guint16	*red,
							 guint16	*green,
							 guint16	*blue,
							 guint		 size,
							 GError		**error);
gboolean	 gcm_profile_set_whitepoint		(GcmProfile	*profile,
							 const GcmColorXYZ *whitepoint,
							 GError		**error);
gboolean	 gcm_profile_set_primaries		(GcmProfile	*profile,
							 const GcmColorXYZ *red,
							 const GcmColorXYZ *green,
							 const GcmColorXYZ *blue,
							 GError		**error);
gboolean	 gcm_profile_create_from_chroma		(GcmProfile	*profile,
							 gdouble	 gamma,
							 const GcmColorYxy *red,
							 const GcmColorYxy *green,
							 const GcmColorYxy *blue,
							 const GcmColorYxy *white,
							 GError		**error);
gboolean	 gcm_profile_guess_and_add_vcgt		(GcmProfile	*profile,
							 GError		**error);
const gchar	*gcm_profile_get_description		(GcmProfile	*profile);
void		 gcm_profile_set_description		(GcmProfile	*profile,
							 const gchar 	*description);
GFile		*gcm_profile_get_file			(GcmProfile	*profile);
void		 gcm_profile_set_file			(GcmProfile	*profile,
							 GFile	 	*file);
const gchar	*gcm_profile_get_filename		(GcmProfile	*profile);
const gchar	*gcm_profile_get_copyright		(GcmProfile	*profile);
void		 gcm_profile_set_copyright		(GcmProfile	*profile,
							 const gchar 	*copyright);
const gchar	*gcm_profile_get_manufacturer		(GcmProfile	*profile);
void		 gcm_profile_set_manufacturer		(GcmProfile	*profile,
							 const gchar 	*manufacturer);
const gchar	*gcm_profile_get_model			(GcmProfile	*profile);
void		 gcm_profile_set_model			(GcmProfile	*profile,
							 const gchar 	*model);
const gchar	*gcm_profile_get_datetime		(GcmProfile	*profile);
void		 gcm_profile_set_datetime		(GcmProfile	*profile,
							 const gchar 	*datetime);
guint		 gcm_profile_get_size			(GcmProfile	*profile);
GcmProfileKind	 gcm_profile_get_kind			(GcmProfile	*profile);
void		 gcm_profile_set_kind			(GcmProfile	*profile,
							 GcmProfileKind	 kind);
GcmColorspace	 gcm_profile_get_colorspace		(GcmProfile	*profile);
void		 gcm_profile_set_colorspace		(GcmProfile	*profile,
							 GcmColorspace	 colorspace);
gboolean	 gcm_profile_get_has_vcgt		(GcmProfile	*profile);
gboolean	 gcm_profile_has_colorspace_description	(GcmProfile	*profile);
guint		 gcm_profile_get_temperature		(GcmProfile	*profile);

G_END_DECLS

#endif /* __GCM_PROFILE_H */


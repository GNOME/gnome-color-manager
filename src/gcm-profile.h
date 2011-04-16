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

#ifndef __GCM_PROFILE_H
#define __GCM_PROFILE_H

#include <glib-object.h>
#include <gio/gio.h>
#include <colord.h>

#include "gcm-clut.h"
#include "gcm-hull.h"

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
const gchar	*gcm_profile_get_version		(GcmProfile	*profile);
gboolean	 gcm_profile_get_can_delete		(GcmProfile	*profile);
GcmClut		*gcm_profile_generate_vcgt		(GcmProfile	*profile,
							 guint		 size);
GcmClut		*gcm_profile_generate_curve		(GcmProfile	*profile,
							 guint		 size);
GcmHull		*gcm_profile_generate_gamut_hull	(GcmProfile	*profile,
							 guint		 res);
gboolean	 gcm_profile_set_vcgt_from_data		(GcmProfile	*profile,
							 guint16	*red,
							 guint16	*green,
							 guint16	*blue,
							 guint		 size,
							 GError		**error);
gboolean	 gcm_profile_set_whitepoint		(GcmProfile	*profile,
							 const CdColorXYZ *whitepoint,
							 GError		**error);
gboolean	 gcm_profile_set_primaries		(GcmProfile	*profile,
							 const CdColorXYZ *red,
							 const CdColorXYZ *green,
							 const CdColorXYZ *blue,
							 GError		**error);
gboolean	 gcm_profile_create_from_chroma		(GcmProfile	*profile,
							 gdouble	 gamma,
							 const CdColorYxy *red,
							 const CdColorYxy *green,
							 const CdColorYxy *blue,
							 const CdColorYxy *white,
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
CdProfileKind	 gcm_profile_get_kind			(GcmProfile	*profile);
void		 gcm_profile_set_kind			(GcmProfile	*profile,
							 CdProfileKind	 kind);
CdColorspace	 gcm_profile_get_colorspace		(GcmProfile	*profile);
void		 gcm_profile_set_colorspace		(GcmProfile	*profile,
							 CdColorspace	 colorspace);
guint		 gcm_profile_get_temperature		(GcmProfile	*profile);
const CdColorXYZ *gcm_profile_get_red			(GcmProfile	*profile);
const CdColorXYZ *gcm_profile_get_green		(GcmProfile	*profile);
const CdColorXYZ *gcm_profile_get_blue			(GcmProfile	*profile);
const CdColorXYZ *gcm_profile_get_white		(GcmProfile	*profile);
const gchar	*gcm_profile_get_data			(GcmProfile	*profile,
							 const gchar	*key);
void		 gcm_profile_set_data			(GcmProfile	*profile,
							 const gchar	*key,
							 const gchar	*data);
GPtrArray	*gcm_profile_get_named_colors		(GcmProfile	*profile,
							 GError		**error);

G_END_DECLS

#endif /* __GCM_PROFILE_H */


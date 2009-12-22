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
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __GCM_PROFILE_LCMS1_H
#define __GCM_PROFILE_LCMS1_H

#include <glib-object.h>

#include "gcm-clut.h"
#include "gcm-profile.h"

G_BEGIN_DECLS

#define GCM_TYPE_PROFILE_LCMS1		(gcm_profile_lcms1_get_type ())
#define GCM_PROFILE_LCMS1(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GCM_TYPE_PROFILE_LCMS1, GcmProfileLcms1))
#define GCM_PROFILE_LCMS1_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), GCM_TYPE_PROFILE_LCMS1, GcmProfileLcms1Class))
#define GCM_IS_PROFILE_LCMS1(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GCM_TYPE_PROFILE_LCMS1))
#define GCM_IS_PROFILE_LCMS1_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GCM_TYPE_PROFILE_LCMS1))
#define GCM_PROFILE_LCMS1_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GCM_TYPE_PROFILE_LCMS1, GcmProfileLcms1Class))

typedef struct _GcmProfileLcms1Private	GcmProfileLcms1Private;
typedef struct _GcmProfileLcms1		GcmProfileLcms1;
typedef struct _GcmProfileLcms1Class	GcmProfileLcms1Class;

struct _GcmProfileLcms1
{
	 GcmProfile		 parent;
	 GcmProfileLcms1Private	*priv;
};

struct _GcmProfileLcms1Class
{
	GcmProfileClass		parent_class;
	/* padding for future expansion */
	void (*_gcm_reserved1) (void);
	void (*_gcm_reserved2) (void);
	void (*_gcm_reserved3) (void);
	void (*_gcm_reserved4) (void);
	void (*_gcm_reserved5) (void);
};

GType		 gcm_profile_lcms1_get_type		(void);
GcmProfileLcms1	*gcm_profile_lcms1_new			(void);

G_END_DECLS

#endif /* __GCM_PROFILE_LCMS1_H */


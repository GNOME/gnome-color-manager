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

#ifndef __GCM_LIST_STORE_PROFILES_H__
#define __GCM_LIST_STORE_PROFILES_H__

#include <gtk/gtk.h>
#include <gcm-device.h>

G_BEGIN_DECLS

#define GCM_TYPE_LIST_STORE_PROFILES		(gcm_list_store_profiles_get_type ())
#define GCM_LIST_STORE_PROFILES(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GCM_TYPE_LIST_STORE_PROFILES, GcmListStoreProfiles))
#define GCM_LIST_STORE_PROFILES_CLASS(obj)	(G_TYPE_CHECK_CLASS_CAST ((obj), GCM_LIST_STORE_PROFILES, GcmListStoreProfilesClass))
#define GCM_IS_LIST_STORE_PROFILES(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GCM_TYPE_LIST_STORE_PROFILES))
#define GCM_IS_LIST_STORE_PROFILES_CLASS(obj)	(G_TYPE_CHECK_CLASS_TYPE ((obj), EFF_TYPE_LIST_STORE_PROFILES))
#define GCM_LIST_STORE_PROFILES_GET_CLASS	(G_TYPE_INSTANCE_GET_CLASS ((obj), GCM_TYPE_LIST_STORE_PROFILES, GcmListStoreProfilesClass))

enum {
	GCM_LIST_STORE_PROFILES_COLUMN_SORT,
	GCM_LIST_STORE_PROFILES_COLUMN_PROFILE,
	GCM_LIST_STORE_PROFILES_COLUMN_IS_DEFAULT,
	GCM_LIST_STORE_PROFILES_COLUMN_TOOLTIP,
	GCM_LIST_STORE_PROFILES_COLUMN_LAST
};

typedef struct GcmListStoreProfiles		GcmListStoreProfiles;
typedef struct GcmListStoreProfilesClass	GcmListStoreProfilesClass;
typedef struct GcmListStoreProfilesPrivate	GcmListStoreProfilesPrivate;

struct GcmListStoreProfiles
{
	GtkListStore			 parent;
	GcmListStoreProfilesPrivate	*priv;
};

struct GcmListStoreProfilesClass
{
	GtkListStoreClass		 parent_class;
};

GType		 gcm_list_store_profiles_get_type		(void);
GtkListStore	*gcm_list_store_profiles_new			(void);
void		 gcm_list_store_profiles_set_from_device	(GtkListStore	*list_store,
								 GcmDevice	*device);

G_END_DECLS

#endif

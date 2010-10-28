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

#include "config.h"
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "gcm-device.h"
#include "gcm-list-store-profiles.h"

G_DEFINE_TYPE (GcmListStoreProfiles, gcm_list_store_profiles, GTK_TYPE_LIST_STORE);

#define GCM_LIST_STORE_PROFILES_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GCM_TYPE_LIST_STORE_PROFILES, GcmListStoreProfilesPrivate))

struct GcmListStoreProfilesPrivate
{
	GcmDevice		*device;
	guint			 changed_id;
};

/**
 * cc_color_panel_profile_get_tooltip:
 **/
static const gchar *
cc_color_panel_profile_get_tooltip (GcmProfile *profile)
{
	const gchar *tooltip = NULL;

	/* VCGT warning */
	if (gcm_profile_get_kind (profile) == GCM_PROFILE_KIND_DISPLAY_DEVICE &&
	    !gcm_profile_get_has_vcgt (profile)) {
		/* TRANSLATORS: this is displayed when the profile is crap */
		tooltip = _("This profile does not have the information required for whole-screen color correction.");
		goto out;
	}
out:
	return tooltip;
}

/**
 * gcm_list_store_refresh_profiles:
 **/
static void
gcm_list_store_refresh_profiles (GtkListStore *list_store)
{
	GPtrArray *profiles;
	GtkTreeIter iter;
	GcmProfile *profile;
	guint i;
	GcmListStoreProfilesPrivate *priv = GCM_LIST_STORE_PROFILES(list_store)->priv;

	/* clear existing list */
	gtk_list_store_clear (list_store);

	/* add profiles for the device */
	profiles = gcm_device_get_profiles (priv->device);
	for (i=0; i<profiles->len; i++) {
		profile = g_ptr_array_index (profiles, i);
		gtk_list_store_append (list_store, &iter);
		gtk_list_store_set (list_store, &iter,
				    GCM_LIST_STORE_PROFILES_COLUMN_PROFILE, profile,
				    GCM_LIST_STORE_PROFILES_COLUMN_SORT, (i == 0) ? "0" : "1",
				    GCM_LIST_STORE_PROFILES_COLUMN_IS_DEFAULT, (i == 0),
				    GCM_LIST_STORE_PROFILES_COLUMN_TOOLTIP, cc_color_panel_profile_get_tooltip (profile),
				    -1);
	}

	g_ptr_array_unref (profiles);
}

/**
 * gcm_list_store_profiles_device_changed_cb:
 **/
static void
gcm_list_store_profiles_device_changed_cb (GcmDevice *device, GtkListStore *list_store)
{
	gcm_list_store_refresh_profiles (list_store);
}

/**
 * gcm_list_store_profiles_set_from_device:
 **/
void
gcm_list_store_profiles_set_from_device (GtkListStore *list_store, GcmDevice *device)
{
	GcmListStoreProfilesPrivate *priv = GCM_LIST_STORE_PROFILES(list_store)->priv;

	g_return_if_fail (device != NULL);

	/* cache */
	if (priv->device != NULL)
		g_object_unref (priv->device);
	if (priv->changed_id != 0)
		g_source_remove (priv->changed_id);
	priv->device = g_object_ref (device);
	priv->changed_id = g_signal_connect (priv->device, "changed",
					     G_CALLBACK (gcm_list_store_profiles_device_changed_cb),
					     list_store);

	/* coldplug */
	gcm_list_store_refresh_profiles (list_store);
}

/**
 * gcm_list_store_profiles_init:
 **/
static void
gcm_list_store_profiles_init (GcmListStoreProfiles *list_store)
{
	GType types[] = { G_TYPE_STRING, GCM_TYPE_PROFILE, G_TYPE_BOOLEAN, G_TYPE_STRING };
	list_store->priv = GCM_LIST_STORE_PROFILES_GET_PRIVATE (list_store);
	gtk_list_store_set_column_types (GTK_LIST_STORE (list_store), GCM_LIST_STORE_PROFILES_COLUMN_LAST, types);
}

/**
 * gcm_list_store_profiles_finalize:
 **/
static void
gcm_list_store_profiles_finalize (GObject *object)
{
	GcmListStoreProfilesPrivate *priv = GCM_LIST_STORE_PROFILES(object)->priv;

	g_object_unref (priv->device);
	if (priv->changed_id != 0)
		g_source_remove (priv->changed_id);

	G_OBJECT_CLASS (gcm_list_store_profiles_parent_class)->finalize (object);
}

/**
 * gcm_list_store_profiles_class_init:
 **/
static void
gcm_list_store_profiles_class_init (GcmListStoreProfilesClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	object_class->finalize = gcm_list_store_profiles_finalize;

	g_type_class_add_private (class, sizeof (GcmListStoreProfilesPrivate));
}

/**
 * gcm_list_store_profiles_new:
 * Return value: A new GcmListStoreProfiles object.
 **/
GtkListStore *
gcm_list_store_profiles_new (void)
{
	return g_object_new (GCM_TYPE_LIST_STORE_PROFILES, NULL);
}


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
 * GNU General Public License for more profile_store.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:gcm-profile-store
 * @short_description: object to hold an array of profiles.
 *
 * This object holds an array of %GcmProfiles, and watches both the directories
 * for changes.
 */

#include "config.h"

#include <glib-object.h>
#include <gio/gio.h>

#include "gcm-profile-store.h"
#include "gcm-utils.h"

#include "egg-debug.h"

static void     gcm_profile_store_finalize	(GObject     *object);

#define GCM_PROFILE_STORE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GCM_TYPE_PROFILE_STORE, GcmProfileStorePrivate))

static gboolean	gcm_profile_store_add_profiles_for_path	(GcmProfileStore *profile_store, const gchar *path);
static void	gcm_profile_store_add_profiles		(GcmProfileStore *profile_store);

/**
 * GcmProfileStorePrivate:
 *
 * Private #GcmProfileStore data
 **/
struct _GcmProfileStorePrivate
{
	GPtrArray			*profile_array;
	GPtrArray			*monitor_array;
	GPtrArray			*directory_array;
};

enum {
	SIGNAL_ADDED,
	SIGNAL_REMOVED,
	SIGNAL_CHANGED,
	SIGNAL_LAST
};

static guint signals[SIGNAL_LAST] = { 0 };

G_DEFINE_TYPE (GcmProfileStore, gcm_profile_store, G_TYPE_OBJECT)

/**
 * gcm_profile_store_get_array:
 *
 * @profile_store: a valid %GcmProfileStore instance
 *
 * Gets the profile list.
 *
 * Return value: an array, free with g_ptr_array_unref()
 **/
GPtrArray *
gcm_profile_store_get_array (GcmProfileStore *profile_store)
{
	g_return_val_if_fail (GCM_IS_PROFILE_STORE (profile_store), NULL);
	return g_ptr_array_ref (profile_store->priv->profile_array);
}

/**
 * gcm_profile_store_in_array:
 **/
static gboolean
gcm_profile_store_in_array (GPtrArray *array, const gchar *text)
{
	const gchar *tmp;
	guint i;

	for (i=0; i<array->len; i++) {
		tmp = g_ptr_array_index (array, i);
		if (g_strcmp0 (text, tmp) == 0)
			return TRUE;
	}
	return FALSE;
}

/**
 * gcm_profile_store_get_by_filename:
 *
 * @profile_store: a valid %GcmProfileStore instance
 * @filename: the profile filename
 *
 * Gets a profile.
 *
 * Return value: a valid %GcmProfile or %NULL. Free with g_object_unref()
 **/
GcmProfile *
gcm_profile_store_get_by_filename (GcmProfileStore *profile_store, const gchar *filename)
{
	guint i;
	GcmProfile *profile = NULL;
	GcmProfile *profile_tmp;
	gchar *filename_tmp;
	GcmProfileStorePrivate *priv = profile_store->priv;

	g_return_val_if_fail (GCM_IS_PROFILE_STORE (profile_store), NULL);
	g_return_val_if_fail (filename != NULL, NULL);

	/* find profile */
	for (i=0; i<priv->profile_array->len; i++) {
		profile_tmp = g_ptr_array_index (priv->profile_array, i);
		g_object_get (profile_tmp,
			      "filename", &filename_tmp,
			      NULL);
		if (g_strcmp0 (filename, filename_tmp) == 0) {
			profile = g_object_ref (profile_tmp);
			g_free (filename_tmp);
			goto out;
		}
		g_free (filename_tmp);
	}

out:
	return profile;
}

/**
 * gcm_profile_store_notify_filename_cb:
 **/
static void
gcm_profile_store_notify_filename_cb (GcmProfile *profile, GParamSpec *pspec, GcmProfileStore *profile_store)
{
	gchar *description;
	GcmProfileStorePrivate *priv = profile_store->priv;

	/* get data, as the filename is no longer valid */
	g_object_get (profile,
		      "description", &description,
		      NULL);

	/* remove from list */
	g_ptr_array_remove (priv->profile_array, profile);

	/* emit a signal */
	egg_debug ("emit removed (and changed): %s", description);
	g_signal_emit (profile_store, signals[SIGNAL_REMOVED], 0, profile);
	g_signal_emit (profile_store, signals[SIGNAL_CHANGED], 0);

	g_free (description);
}

/**
 * gcm_profile_store_add_profile:
 **/
static gboolean
gcm_profile_store_add_profile (GcmProfileStore *profile_store, const gchar *filename)
{
	gboolean ret = FALSE;
	GcmProfile *profile = NULL;
	GError *error = NULL;
	GcmProfileStorePrivate *priv = profile_store->priv;

	/* already added? */
	profile = gcm_profile_store_get_by_filename (profile_store, filename);
	if (profile != NULL)
		goto out;

	/* parse the profile name */
	profile = gcm_profile_default_new ();
	ret = gcm_profile_parse (profile, filename, &error);
	if (!ret) {
		egg_warning ("failed to add profile '%s': %s", filename, error->message);
		g_error_free (error);
		goto out;		
	}

	/* add to array */
	egg_debug ("parsed new profile '%s'", filename);
	g_ptr_array_add (priv->profile_array, g_object_ref (profile));
	g_signal_connect (profile, "notify::filename", G_CALLBACK(gcm_profile_store_notify_filename_cb), profile_store);

	/* emit a signal */
	egg_debug ("emit added (and changed): %s", filename);
	g_signal_emit (profile_store, signals[SIGNAL_ADDED], 0, profile);
	g_signal_emit (profile_store, signals[SIGNAL_CHANGED], 0);
out:
	if (profile != NULL)
		g_object_unref (profile);
	return ret;
}

/**
 * gcm_profile_store_file_monitor_changed_cb:
 **/
static void
gcm_profile_store_file_monitor_changed_cb (GFileMonitor *monitor, GFile *file, GFile *other_file, GFileMonitorEvent event_type, GcmProfileStore *profile_store)
{
	gchar *path = NULL;

	/* only care about created objects */
	if (event_type != G_FILE_MONITOR_EVENT_CREATED)
		goto out;

	/* just rescan everything */
	path = g_file_get_path (file);
	egg_debug ("%s was added, rescanning everything", path);
	gcm_profile_store_add_profiles (profile_store);
out:
	g_free (path);
}

/**
 * gcm_profile_store_add_profiles_for_path:
 **/
static gboolean
gcm_profile_store_add_profiles_for_path (GcmProfileStore *profile_store, const gchar *path)
{
	GDir *dir = NULL;
	GError *error = NULL;
	gboolean ret = TRUE;
	const gchar *name;
	gchar *full_path;
	GcmProfileStorePrivate *priv = profile_store->priv;
	GFileMonitor *monitor = NULL;
	GFile *file = NULL;

	/* add if correct type */
	if (g_file_test (path, G_FILE_TEST_IS_REGULAR)) {

		/* check the file actually is a profile */
		ret = gcm_utils_is_icc_profile (path);
		if (ret) {
			gcm_profile_store_add_profile (profile_store, path);
			goto out;
		}

		/* invalid file */
		egg_debug ("not recognised as ICC profile: %s", path);
		goto out;
	}

	/* get contents */
	dir = g_dir_open (path, 0, &error);
	if (dir == NULL) {
		egg_debug ("failed to open: %s", error->message);
		g_error_free (error);
		ret = FALSE;
		goto out;
	}

	/* add an inotify watch if not already added? */
	ret = gcm_profile_store_in_array (priv->directory_array, path);
	if (!ret) {
		file = g_file_new_for_path (path);
		monitor = g_file_monitor_directory (file, G_FILE_MONITOR_NONE, NULL, &error);
		if (monitor == NULL) {
			egg_debug ("failed to monitor path: %s", error->message);
			g_error_free (error);
			goto out;
		}

		/* don't allow many files to cause re-scan after rescan */
		g_file_monitor_set_rate_limit (monitor, 1000);
		g_signal_connect (monitor, "changed", G_CALLBACK(gcm_profile_store_file_monitor_changed_cb), profile_store);
		g_ptr_array_add (priv->monitor_array, g_object_ref (monitor));
		g_ptr_array_add (priv->directory_array, g_strdup (path));
	}

	/* process entire tree */
	do {
		name = g_dir_read_name (dir);
		if (name == NULL)
			break;

		/* make the compete path */
		full_path = g_build_filename (path, name, NULL);
		gcm_profile_store_add_profiles_for_path (profile_store, full_path);
		g_free (full_path);
	} while (TRUE);
out:
	if (monitor != NULL)
		g_object_unref (monitor);
	if (file != NULL)
		g_object_unref (file);
	if (dir != NULL)
		g_dir_close (dir);
	return ret;
}

/**
 * gcm_profile_store_add_profiles:
 **/
static void
gcm_profile_store_add_profiles (GcmProfileStore *profile_store)
{
	gchar *user;

	/* get systemwide profiles */
	gcm_profile_store_add_profiles_for_path (profile_store, "/usr/share/color/icc");
	gcm_profile_store_add_profiles_for_path (profile_store, "/usr/local/share/color/icc");

	/* get Linux per-user profiles */
	user = g_build_filename (g_get_home_dir (), ".color", "icc", NULL);
	gcm_profile_store_add_profiles_for_path (profile_store, user);
	g_free (user);

	/* get OSX per-user profiles */
	user = g_build_filename (g_get_home_dir (), "Library", "ColorSync", "Profiles", NULL);
	gcm_profile_store_add_profiles_for_path (profile_store, user);
	g_free (user);
}

/**
 * gcm_profile_store_class_init:
 **/
static void
gcm_profile_store_class_init (GcmProfileStoreClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gcm_profile_store_finalize;

	/**
	 * GcmProfileStore::added
	 **/
	signals[SIGNAL_ADDED] =
		g_signal_new ("added",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GcmProfileStoreClass, added),
			      NULL, NULL, g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, G_TYPE_OBJECT);
	/**
	 * GcmProfileStore::removed
	 **/
	signals[SIGNAL_REMOVED] =
		g_signal_new ("removed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GcmProfileStoreClass, removed),
			      NULL, NULL, g_cclosure_marshal_VOID__OBJECT,
			      G_TYPE_NONE, 1, G_TYPE_OBJECT);
	/**
	 * GcmProfileStore::changed
	 **/
	signals[SIGNAL_CHANGED] =
		g_signal_new ("changed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GcmProfileStoreClass, changed),
			      NULL, NULL, g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	g_type_class_add_private (klass, sizeof (GcmProfileStorePrivate));
}

/**
 * gcm_profile_store_init:
 **/
static void
gcm_profile_store_init (GcmProfileStore *profile_store)
{
	profile_store->priv = GCM_PROFILE_STORE_GET_PRIVATE (profile_store);
	profile_store->priv->profile_array = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	profile_store->priv->monitor_array = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	profile_store->priv->directory_array = g_ptr_array_new_with_free_func ((GDestroyNotify) g_free);

	/* get profiles */
	gcm_profile_store_add_profiles (profile_store);
}

/**
 * gcm_profile_store_finalize:
 **/
static void
gcm_profile_store_finalize (GObject *object)
{
	GcmProfileStore *profile_store = GCM_PROFILE_STORE (object);
	GcmProfileStorePrivate *priv = profile_store->priv;

	g_ptr_array_unref (priv->profile_array);
	g_ptr_array_unref (priv->monitor_array);
	g_ptr_array_unref (priv->directory_array);

	G_OBJECT_CLASS (gcm_profile_store_parent_class)->finalize (object);
}

/**
 * gcm_profile_store_new:
 *
 * Return value: a new GcmProfileStore object.
 **/
GcmProfileStore *
gcm_profile_store_new (void)
{
	GcmProfileStore *profile_store;
	profile_store = g_object_new (GCM_TYPE_PROFILE_STORE, NULL);
	return GCM_PROFILE_STORE (profile_store);
}


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

#include "egg-debug.h"

static void     gcm_profile_store_finalize	(GObject     *object);

#define GCM_PROFILE_STORE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GCM_TYPE_PROFILE_STORE, GcmProfileStorePrivate))

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
	GVolumeMonitor			*volume_monitor;
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
	const gchar *filename_tmp;
	GcmProfileStorePrivate *priv = profile_store->priv;

	g_return_val_if_fail (GCM_IS_PROFILE_STORE (profile_store), NULL);
	g_return_val_if_fail (filename != NULL, NULL);

	/* find profile */
	for (i=0; i<priv->profile_array->len; i++) {
		profile_tmp = g_ptr_array_index (priv->profile_array, i);
		filename_tmp = gcm_profile_get_filename (profile_tmp);
		if (g_strcmp0 (filename, filename_tmp) == 0) {
			profile = g_object_ref (profile_tmp);
			goto out;
		}
	}
out:
	return profile;
}

/**
 * gcm_profile_store_get_by_checksum:
 *
 * @profile_store: a valid %GcmProfileStore instance
 * @checksum: the profile checksum
 *
 * Gets a profile.
 *
 * Return value: a valid %GcmProfile or %NULL. Free with g_object_unref()
 **/
GcmProfile *
gcm_profile_store_get_by_checksum (GcmProfileStore *profile_store, const gchar *checksum)
{
	guint i;
	GcmProfile *profile = NULL;
	GcmProfile *profile_tmp;
	const gchar *checksum_tmp;
	GcmProfileStorePrivate *priv = profile_store->priv;

	g_return_val_if_fail (GCM_IS_PROFILE_STORE (profile_store), NULL);
	g_return_val_if_fail (checksum != NULL, NULL);

	/* find profile */
	for (i=0; i<priv->profile_array->len; i++) {
		profile_tmp = g_ptr_array_index (priv->profile_array, i);
		checksum_tmp = gcm_profile_get_checksum (profile_tmp);
		if (g_strcmp0 (checksum, checksum_tmp) == 0) {
			profile = g_object_ref (profile_tmp);
			goto out;
		}
	}
out:
	return profile;
}

/**
 * gcm_profile_store_remove_profile:
 **/
static gboolean
gcm_profile_store_remove_profile (GcmProfileStore *profile_store, GcmProfile *profile)
{
	gboolean ret;
	GcmProfileStorePrivate *priv = profile_store->priv;

	/* grab a temporary reference on this profile */
	g_object_ref (profile);

	/* remove from list */
	ret = g_ptr_array_remove (priv->profile_array, profile);
	if (!ret) {
		egg_warning ("failed to remove %s", gcm_profile_get_filename (profile));
		goto out;
	}

	/* emit a signal */
	egg_debug ("emit removed (and changed): %s", gcm_profile_get_checksum (profile));
	g_signal_emit (profile_store, signals[SIGNAL_REMOVED], 0, profile);
	g_signal_emit (profile_store, signals[SIGNAL_CHANGED], 0);
out:
	g_object_unref (profile);
	return ret;
}

/**
 * gcm_profile_store_notify_filename_cb:
 **/
static void
gcm_profile_store_notify_filename_cb (GcmProfile *profile, GParamSpec *pspec, GcmProfileStore *profile_store)
{
	gcm_profile_store_remove_profile (profile_store, profile);
}

/**
 * gcm_profile_store_add_profile:
 **/
static gboolean
gcm_profile_store_add_profile (GcmProfileStore *profile_store, GFile *file)
{
	gboolean ret = FALSE;
	GcmProfile *profile = NULL;
	GcmProfile *profile_tmp = NULL;
	GError *error = NULL;
	gchar *filename = NULL;
	const gchar *checksum;
	GcmProfileStorePrivate *priv = profile_store->priv;

	/* already added? */
	filename = g_file_get_path (file);
	profile = gcm_profile_store_get_by_filename (profile_store, filename);
	if (profile != NULL)
		goto out;

	/* parse the profile name */
	profile = gcm_profile_new ();
	ret = gcm_profile_parse (profile, file, &error);
	if (!ret) {
		egg_warning ("failed to add profile '%s': %s", filename, error->message);
		g_error_free (error);
		goto out;		
	}

	/* check the profile has not been added already */
	checksum = gcm_profile_get_checksum (profile);
	profile_tmp = gcm_profile_store_get_by_checksum (profile_store, checksum);
	if (profile_tmp != NULL) {

		/* we value a local file higher than the shared file */
		if (gcm_profile_get_can_delete (profile_tmp)) {
			egg_debug ("already added a deletable profile %s, cannot add %s",
				   gcm_profile_get_filename (profile_tmp), filename);
			goto out;
		}

		/* remove the old profile in favour of the new one */
		gcm_profile_store_remove_profile (profile_store, profile_tmp);
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
	g_free (filename);
	if (profile_tmp != NULL)
		g_object_unref (profile_tmp);
	if (profile != NULL)
		g_object_unref (profile);
	return ret;
}

/**
 * gcm_profile_store_file_monitor_changed_cb:
 **/
static void
gcm_profile_store_file_monitor_changed_cb (GFileMonitor *monitor, GFile *file, GFile *other_file,
					   GFileMonitorEvent event_type, GcmProfileStore *profile_store)
{
	gchar *path = NULL;
	gchar *parent_path = NULL;
	GFile *parent = NULL;

	/* only care about created objects */
	if (event_type != G_FILE_MONITOR_EVENT_CREATED)
		goto out;

	/* ignore temp files */
	path = g_file_get_path (file);
	if (g_strrstr (path, ".goutputstream") != NULL) {
		egg_debug ("ignoring gvfs temporary file");
		goto out;
	}

	/* just rescan the correct directory */
	parent = g_file_get_parent (file);
	parent_path = g_file_get_path (parent);
	egg_debug ("%s was added, rescanning %s", path, parent_path);
	gcm_profile_store_search_path (profile_store, parent_path);
out:
	if (parent != NULL)
		g_object_unref (parent);
	g_free (path);
	g_free (parent_path);
}

/**
 * gcm_profile_store_search_path:
 *
 * Return value: if any profile were added
 **/
gboolean
gcm_profile_store_search_path (GcmProfileStore *profile_store, const gchar *path)
{
	GDir *dir = NULL;
	GError *error = NULL;
	gboolean ret;
	gboolean success = FALSE;
	const gchar *name;
	gchar *full_path;
	GcmProfileStorePrivate *priv = profile_store->priv;
	GFileMonitor *monitor = NULL;
	GFile *file = NULL;

	/* add if correct type */
	if (g_file_test (path, G_FILE_TEST_IS_REGULAR)) {

		/* check the file actually is a profile when we try to parse it */
		file = g_file_new_for_path (path);
		success = gcm_profile_store_add_profile (profile_store, file);
		goto out;
	}

	/* get contents */
	dir = g_dir_open (path, 0, &error);
	if (dir == NULL) {
		egg_debug ("failed to open: %s", error->message);
		g_error_free (error);
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
		ret = gcm_profile_store_search_path (profile_store, full_path);
		if (ret)
			success = TRUE;
		g_free (full_path);
	} while (TRUE);
out:
	if (monitor != NULL)
		g_object_unref (monitor);
	if (file != NULL)
		g_object_unref (file);
	if (dir != NULL)
		g_dir_close (dir);
	return success;
}

/**
 * gcm_profile_store_add_profiles_from_mounted_volume:
 **/
static gboolean
gcm_profile_store_add_profiles_from_mounted_volume (GcmProfileStore *profile_store, GMount *mount)
{
	GFile *root;
	gchar *path;
	gchar *path_root;
	const gchar *type;
	GFileInfo *info;
	GError *error = NULL;
	gboolean ret;
	gboolean success = FALSE;

	/* get the mount root */
	root = g_mount_get_root (mount);
	path_root = g_file_get_path (root);
	if (path_root == NULL)
		goto out;

	/* get the filesystem type */
	info = g_file_query_filesystem_info (root, G_FILE_ATTRIBUTE_FILESYSTEM_TYPE, NULL, &error);
	if (info == NULL) {
		egg_warning ("failed to get filesystem type: %s", error->message);
		g_error_free (error);
		goto out;
	}
	type = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_FILESYSTEM_TYPE);
	egg_debug ("filesystem mounted on %s has type %s", path_root, type);

	/* only scan hfs volumes for OSX */
	if (g_strcmp0 (type, "hfs") == 0) {
		path = g_build_filename (path_root, "Library", "ColorSync", "Profiles", "Displays", NULL);
		ret = gcm_profile_store_search_path (profile_store, path);
		if (ret)
			success = TRUE;
		g_free (path);

		/* no more matching */
		goto out;
	}

	/* and fat32 and ntfs for windows */
	if (g_strcmp0 (type, "ntfs") == 0 || g_strcmp0 (type, "msdos") == 0) {

		/* Windows XP */
		path = g_build_filename (path_root, "Windows", "system32", "spool", "drivers", "color", NULL);
		ret = gcm_profile_store_search_path (profile_store, path);
		if (ret)
			success = TRUE;
		g_free (path);

		/* Windows 2000 */
		path = g_build_filename (path_root, "Winnt", "system32", "spool", "drivers", "color", NULL);
		ret = gcm_profile_store_search_path (profile_store, path);
		if (ret)
			success = TRUE;
		g_free (path);

		/* Windows 98 and ME */
		path = g_build_filename (path_root, "Windows", "System", "Color", NULL);
		ret = gcm_profile_store_search_path (profile_store, path);
		if (ret)
			success = TRUE;
		g_free (path);

		/* no more matching */
		goto out;
	}
out:
	g_free (path_root);
	g_object_unref (root);
	return success;
}

/**
 * gcm_profile_store_add_profiles_from_mounted_volumes:
 **/
static gboolean
gcm_profile_store_add_profiles_from_mounted_volumes (GcmProfileStore *profile_store)
{
	gboolean ret;
	gboolean success = FALSE;
	GList *mounts, *l;
	GMount *mount;
	GcmProfileStorePrivate *priv = profile_store->priv;

	/* get all current mounts */
	mounts = g_volume_monitor_get_mounts (priv->volume_monitor);
	for (l = mounts; l != NULL; l = l->next) {
		mount = l->data;
		ret = gcm_profile_store_add_profiles_from_mounted_volume (profile_store, mount);
		if (ret)
			success = TRUE;
		g_object_unref (mount);
	}
	g_list_free (mounts);
	return success;
}

/**
 * gcm_profile_store_mkdir_with_parents:
 **/
static gboolean
gcm_profile_store_mkdir_with_parents (const gchar *filename, GError **error)
{
	gboolean ret;
	GFile *file = NULL;

	/* ensure desination exists */
	ret = g_file_test (filename, G_FILE_TEST_EXISTS);
	if (!ret) {
		file = g_file_new_for_path (filename);
		ret = g_file_make_directory_with_parents (file, NULL, error);
		if (!ret)
			goto out;
	}
out:
	if (file != NULL)
		g_object_unref (file);
	return ret;
}

/**
 * gcm_profile_store_search:
 *
 * Return value: if any profile were added
 **/
gboolean
gcm_profile_store_search (GcmProfileStore *profile_store, GcmProfileSearchFlags flags)
{
	gchar *path;
	gboolean ret;
	gboolean success = FALSE;
	GError *error;

	/* get OSX and Linux system-wide profiles */
	if (flags == GCM_PROFILE_STORE_SEARCH_ALL ||
	    flags & GCM_PROFILE_STORE_SEARCH_SYSTEM) {
		ret = gcm_profile_store_search_path (profile_store, "/usr/share/color/icc");
		if (ret)
			success = TRUE;
		ret = gcm_profile_store_search_path (profile_store, "/usr/local/share/color/icc");
		if (ret)
			success = TRUE;
		ret = gcm_profile_store_search_path (profile_store, "/Library/ColorSync/Profiles/Displays");
		if (ret)
			success = TRUE;
	}

	/* get OSX and Windows system-wide profiles when using Linux */
	if (flags == GCM_PROFILE_STORE_SEARCH_ALL ||
	    flags & GCM_PROFILE_STORE_SEARCH_VOLUMES) {
		ret = gcm_profile_store_add_profiles_from_mounted_volumes (profile_store);
		if (ret)
			success = TRUE;
	}

	/* get Linux per-user profiles */
	if (flags == GCM_PROFILE_STORE_SEARCH_ALL ||
	    flags & GCM_PROFILE_STORE_SEARCH_USER) {
		path = g_build_filename (g_get_user_data_dir (), "icc", NULL);
		ret = gcm_profile_store_mkdir_with_parents (path, &error);
		if (!ret) {
			egg_warning ("failed to create directory on startup: %s", error->message);
			g_error_free (error);
		} else {
			ret = gcm_profile_store_search_path (profile_store, path);
			if (ret)
				success = TRUE;
		}
		g_free (path);

		/* get per-user profiles from obsolete location */
		path = g_build_filename (g_get_home_dir (), ".color", "icc", NULL);
		ret = gcm_profile_store_search_path (profile_store, path);
		if (ret)
			success = TRUE;
		g_free (path);

		/* get OSX per-user profiles */
		path = g_build_filename (g_get_home_dir (), "Library", "ColorSync", "Profiles", NULL);
		ret = gcm_profile_store_search_path (profile_store, path);
		if (ret)
			success = TRUE;
		g_free (path);
	}

	/* get machine specific profiles */
	if (flags == GCM_PROFILE_STORE_SEARCH_ALL ||
	    flags & GCM_PROFILE_STORE_SEARCH_MACHINE) {
		ret = gcm_profile_store_search_path (profile_store, "/var/lib/color/icc");
		if (ret)
			success = TRUE;
	}

	return success;
}

/**
 * gcm_profile_store_volume_monitor_mount_added_cb:
 **/
static void
gcm_profile_store_volume_monitor_mount_added_cb (GVolumeMonitor *volume_monitor, GMount *mount, GcmProfileStore *profile_store)
{
	gcm_profile_store_add_profiles_from_mounted_volume (profile_store, mount);
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

	/* watch for volumes to be connected */
	profile_store->priv->volume_monitor = g_volume_monitor_get ();
	g_signal_connect (profile_store->priv->volume_monitor,
			  "mount-added",
			  G_CALLBACK(gcm_profile_store_volume_monitor_mount_added_cb),
			  profile_store);
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
	g_object_unref (priv->volume_monitor);

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


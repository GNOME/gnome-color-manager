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
 * @short_description: Object to search for profiles and keep a list up to date.
 *
 * This object holds an array of %GcmProfiles, and watches both the directories
 * for changes.
 */

#include "config.h"

#include <glib-object.h>
#include <gio/gio.h>

#include "gcm-profile-store.h"

static void     gcm_profile_store_finalize	(GObject     *object);
static gboolean	gcm_profile_store_search_path (GcmProfileStore *profile_store, const gchar *path);

#define GCM_PROFILE_STORE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GCM_TYPE_PROFILE_STORE, GcmProfileStorePrivate))

/**
 * GcmProfileStorePrivate:
 *
 * Private #GcmProfileStore data
 **/
struct _GcmProfileStorePrivate
{
	GPtrArray			*filename_array;
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
 * gcm_profile_store_remove_profile:
 **/
static gboolean
gcm_profile_store_remove_profile (GcmProfileStore *profile_store, const gchar *filename)
{
	gboolean ret;
	GcmProfileStorePrivate *priv = profile_store->priv;

	/* remove from list */
	ret = g_ptr_array_remove (priv->filename_array, (gchar*)filename);
	if (!ret) {
		g_warning ("failed to remove %s", filename);
		goto out;
	}

	/* emit a signal */
	g_debug ("emit removed: %s", filename);
	g_signal_emit (profile_store, signals[SIGNAL_REMOVED], 0, filename);
out:
	return ret;
}

/**
 * gcm_profile_store_add_profile:
 **/
static void
gcm_profile_store_add_profile (GcmProfileStore *profile_store, const gchar *filename)
{
	GcmProfileStorePrivate *priv = profile_store->priv;

	/* remove from list */
	g_ptr_array_add (priv->filename_array, g_strdup (filename));

	/* emit a signal */
	g_debug ("emit add: %s", filename);
	g_signal_emit (profile_store, signals[SIGNAL_ADDED], 0, filename);
}

/**
 * gcm_profile_store_file_monitor_changed_cb:
 **/
static void
gcm_profile_store_file_monitor_changed_cb (GFileMonitor *monitor,
					   GFile *file,
					   GFile *other_file,
					   GFileMonitorEvent event_type,
					   GcmProfileStore *profile_store)
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
		g_debug ("ignoring gvfs temporary file");
		goto out;
	}

if (0)	gcm_profile_store_remove_profile (profile_store, NULL);

	/* just rescan the correct directory */
	parent = g_file_get_parent (file);
	parent_path = g_file_get_path (parent);
	g_debug ("%s was added, rescanning %s", path, parent_path);
	gcm_profile_store_search_path (profile_store, parent_path);
out:
	if (parent != NULL)
		g_object_unref (parent);
	g_free (path);
	g_free (parent_path);
}

/**
 * gcm_profile_store_search_path:
 **/
static gboolean
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
		gcm_profile_store_add_profile (profile_store, path);
		goto out;
	}

	/* get contents */
	dir = g_dir_open (path, 0, &error);
	if (dir == NULL) {
		g_debug ("failed to open: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* add an inotify watch if not already added? */
	ret = gcm_profile_store_in_array (priv->directory_array, path);
	if (!ret) {
		file = g_file_new_for_path (path);
		monitor = g_file_monitor_directory (file, G_FILE_MONITOR_NONE, NULL, &error);
		if (monitor == NULL) {
			g_debug ("failed to monitor path: %s", error->message);
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
 * @profile_store: a valid %GcmProfileStore instance
 *
 * Searches specified locations for ICC profiles.
 *
 * Return value: %TRUE if any profile were added
 **/
gboolean
gcm_profile_store_search (GcmProfileStore *profile_store)
{
	gchar *path;
	gboolean ret;
	gboolean success = FALSE;
	GError *error;

	/* get Linux per-user profiles */
	path = g_build_filename (g_get_user_data_dir (), "icc", NULL);
	ret = gcm_profile_store_mkdir_with_parents (path, &error);
	if (!ret) {
		g_warning ("failed to create directory on startup: %s", error->message);
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
	return success;
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
			      NULL, NULL, g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);
	/**
	 * GcmProfileStore::removed
	 **/
	signals[SIGNAL_REMOVED] =
		g_signal_new ("removed",
			      G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (GcmProfileStoreClass, removed),
			      NULL, NULL, g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);

	g_type_class_add_private (klass, sizeof (GcmProfileStorePrivate));
}

/**
 * gcm_profile_store_init:
 **/
static void
gcm_profile_store_init (GcmProfileStore *profile_store)
{
	profile_store->priv = GCM_PROFILE_STORE_GET_PRIVATE (profile_store);
	profile_store->priv->filename_array = g_ptr_array_new_with_free_func (g_free);
	profile_store->priv->monitor_array = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	profile_store->priv->directory_array = g_ptr_array_new_with_free_func ((GDestroyNotify) g_free);
}

/**
 * gcm_profile_store_finalize:
 **/
static void
gcm_profile_store_finalize (GObject *object)
{
	GcmProfileStore *profile_store = GCM_PROFILE_STORE (object);
	GcmProfileStorePrivate *priv = profile_store->priv;

	g_ptr_array_unref (priv->filename_array);
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


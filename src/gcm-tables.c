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
 * GNU General Public License for more tables.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:gcm-tables
 * @short_description: An object to convert ID values into text
 *
 * This object parses the USB, PCI and PNP tables to return text for numbers.
 */

#include "config.h"

#include <glib-object.h>

#include "gcm-tables.h"

#include "egg-debug.h"

static void     gcm_tables_finalize	(GObject     *object);

#define GCM_TABLES_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GCM_TYPE_TABLES, GcmTablesPrivate))

/**
 * GcmTablesPrivate:
 *
 * Private #GcmTables data
 **/
struct _GcmTablesPrivate
{
	gchar				*data_dir;
	GHashTable			*pnp_table;
};

enum {
	PROP_0,
	PROP_DATA_DIR,
	PROP_LAST
};

G_DEFINE_TYPE (GcmTables, gcm_tables, G_TYPE_OBJECT)

/**
 * gcm_tables_get_pnp_id:
 **/
gchar *
gcm_tables_get_pnp_id (GcmTables *tables, const gchar *pnp_id, GError **error)
{
	GcmTablesPrivate *priv = tables->priv;
	gchar *retval = NULL;
	gpointer found;
	guint size;
	gchar *filename = NULL;
	gboolean ret;
	gchar *data = NULL;
	gchar **split = NULL;
	guint i;

	g_return_val_if_fail (GCM_IS_TABLES (tables), NULL);
	g_return_val_if_fail (pnp_id != NULL, NULL);

	/* if table is empty, try to load it */
	size = g_hash_table_size (priv->pnp_table);
	if (size == 0) {

		/* check it exists */
		filename = g_build_filename (priv->data_dir, "pnp.ids", NULL);
		ret = g_file_test (filename, G_FILE_TEST_EXISTS);
		if (!ret) {
			if (error != NULL)
				*error = g_error_new (1, 0, "could not load %s", filename);
			goto out;
		}

		/* load the contents */
		egg_debug ("loading: %s", filename);
		ret = g_file_get_contents (filename, &data, NULL, error);
		if (!ret)
			goto out;

		/* parse into lines */
		split = g_strsplit (data, "\n", -1);
		for (i=0; split[i] != NULL; i++) {
			if (split[i][0] == '\0')
				continue;
			split[i][3] = '\0';
			g_hash_table_insert (priv->pnp_table, g_strdup (split[i]), g_strdup (&split[i][4]));
		}
	}

	/* look this up in the table */
	found = g_hash_table_lookup (priv->pnp_table, pnp_id);
	if (found == NULL) {
		if (error != NULL)
			*error = g_error_new (1, 0, "could not find %s", pnp_id);
		goto out;
	}

	/* return a copy */
	retval = g_strdup (found);
out:
	g_free (data);
	g_free (filename);
	g_strfreev (split);
	return retval;
}

/**
 * gcm_tables_get_property:
 **/
static void
gcm_tables_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GcmTables *tables = GCM_TABLES (object);
	GcmTablesPrivate *priv = tables->priv;

	switch (prop_id) {
	case PROP_DATA_DIR:
		g_value_set_string (value, priv->data_dir);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gcm_tables_set_property:
 **/
static void
gcm_tables_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	GcmTables *tables = GCM_TABLES (object);
	GcmTablesPrivate *priv = tables->priv;

	switch (prop_id) {
	case PROP_DATA_DIR:
		g_free (priv->data_dir);
		priv->data_dir = g_strdup (g_value_get_string (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gcm_tables_class_init:
 **/
static void
gcm_tables_class_init (GcmTablesClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gcm_tables_finalize;
	object_class->get_property = gcm_tables_get_property;
	object_class->set_property = gcm_tables_set_property;

	/**
	 * GcmTables:data-dir:
	 */
	pspec = g_param_spec_string ("data-dir", NULL, NULL,
				     NULL,
				     G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_DATA_DIR, pspec);

	g_type_class_add_private (klass, sizeof (GcmTablesPrivate));
}

/**
 * gcm_tables_set_default_data_dir:
 **/
static gboolean
gcm_tables_set_default_data_dir (GcmTables *tables)
{
	gboolean ret;

	/* shipped in hwdata, e.g. Red Hat */
	ret = g_file_test ("/usr/share/hwdata/pnp.ids", G_FILE_TEST_EXISTS);
	if (ret) {
		tables->priv->data_dir = g_strdup ("/usr/share/hwdata");
		goto out;
	}

	/* shipped in pnputils, e.g. Debian */
	ret = g_file_test ("/usr/share/misc/pnp.ids", G_FILE_TEST_EXISTS);
	if (ret) {
		tables->priv->data_dir = g_strdup ("/usr/share/misc");
		goto out;
	}

	/* need to install package? */
	egg_warning ("cannot find pnp.ids");
out:
	return ret;
}

/**
 * gcm_tables_init:
 **/
static void
gcm_tables_init (GcmTables *tables)
{
	tables->priv = GCM_TABLES_GET_PRIVATE (tables);
	tables->priv->data_dir = NULL;
	tables->priv->pnp_table = g_hash_table_new_full (g_str_hash, g_str_equal, (GDestroyNotify) g_free, (GDestroyNotify) g_free);

	/* the default location differs on debian and other distros */
	gcm_tables_set_default_data_dir (tables);
}

/**
 * gcm_tables_finalize:
 **/
static void
gcm_tables_finalize (GObject *object)
{
	GcmTables *tables = GCM_TABLES (object);
	GcmTablesPrivate *priv = tables->priv;

	g_free (priv->data_dir);
	g_hash_table_unref (priv->pnp_table);

	G_OBJECT_CLASS (gcm_tables_parent_class)->finalize (object);
}

/**
 * gcm_tables_new:
 *
 * Return value: a new GcmTables object.
 **/
GcmTables *
gcm_tables_new (void)
{
	GcmTables *tables;
	tables = g_object_new (GCM_TYPE_TABLES, NULL);
	return GCM_TABLES (tables);
}

/***************************************************************************
 ***                          MAKE CHECK TESTS                           ***
 ***************************************************************************/
#ifdef EGG_TEST
#include "egg-test.h"

void
gcm_tables_test (EggTest *test)
{
	GcmTables *tables;
	gboolean ret;
	GError *error = NULL;
	gchar *vendor;

	if (!egg_test_start (test, "GcmTables"))
		return;

	/************************************************************/
	egg_test_title (test, "get a tables object");
	tables = gcm_tables_new ();
	egg_test_assert (test, tables != NULL);

	/************************************************************/
	egg_test_title (test, "check pnp id 'IBM'");
	vendor = gcm_tables_get_pnp_id (tables, "IBM", &error);
	if (vendor == NULL)
		egg_test_failed (test, "failed to get value: %s", error->message);
	if (g_strcmp0 (vendor, "IBM France") == 0)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "invalid value: %s", vendor);
	g_free (vendor);

	/************************************************************/
	egg_test_title (test, "check pnp id 'MIL'");
	vendor = gcm_tables_get_pnp_id (tables, "MIL", &error);
	if (vendor == NULL)
		egg_test_failed (test, "failed to get value: %s", error->message);
	if (g_strcmp0 (vendor, "Marconi Instruments Ltd") == 0)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "invalid value: %s", vendor);
	g_free (vendor);

	/************************************************************/
	egg_test_title (test, "check pnp id 'XXX'");
	vendor = gcm_tables_get_pnp_id (tables, "XXX", &error);
	if (vendor == NULL) {
		if (error == NULL)
			egg_test_failed (test, "failed to get value and no error set");
		g_clear_error (&error);
		egg_test_success (test, NULL);
	} else {
		egg_test_failed (test, "vendor should not exist");
	}
	g_free (vendor);

	g_object_unref (tables);

	egg_test_end (test);
}
#endif


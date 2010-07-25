/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU Lesser General Public License Version 2.1
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

/**
 * SECTION:gcm-ddc-client
 * @short_description: For managing all the #GcmDdcDevice's.
 *
 * A GObject to use for managing a list of #GcmDdcDevice's.
 * A #GcmDdcClient will contain many #GcmDdcDevice's.
 */

#include "config.h"

#include <glib-object.h>
#include <stdlib.h>

#include <gcm-ddc-client.h>
#include <gcm-ddc-device.h>

static void     gcm_ddc_client_finalize	(GObject     *object);

#define GCM_DDC_CLIENT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GCM_TYPE_DDC_CLIENT, GcmDdcClientPrivate))

/**
 * GcmDdcClientPrivate:
 *
 * Private #GcmDdcClient data
 **/
struct _GcmDdcClientPrivate
{
	GPtrArray		*devices;
	gboolean		 has_coldplug;
	GcmVerbose		 verbose;
};

enum {
	PROP_0,
	PROP_HAS_COLDPLUG,
	PROP_LAST
};

G_DEFINE_TYPE (GcmDdcClient, gcm_ddc_client, G_TYPE_OBJECT)

/**
 * gcm_ddc_client_ensure_coldplug:
 **/
static gboolean
gcm_ddc_client_ensure_coldplug (GcmDdcClient *client, GError **error)
{
	gboolean ret = FALSE;
	gboolean any_found = FALSE;
	guint i;
	gchar *filename;
	GError *error_local = NULL;
	GcmDdcDevice *device;

	g_return_val_if_fail (GCM_IS_DDC_CLIENT(client), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* already done */
	if (client->priv->has_coldplug)
		return TRUE;

	/* ensure we have the module loaded */
	ret = g_file_test ("/sys/module/i2c_dev/srcversion", G_FILE_TEST_EXISTS);
	if (!ret) {
		g_set_error_literal (error, GCM_DDC_CLIENT_ERROR, GCM_DDC_CLIENT_ERROR_FAILED,
				     "unable to use I2C, you need to 'modprobe i2c-dev'");
		goto out;
	}

	/* try each i2c port */
	for (i=0; i<16; i++) {
		filename = g_strdup_printf ("/dev/i2c-%i", i);
		if (!g_file_test (filename, G_FILE_TEST_EXISTS)) {
			g_free (filename);
			break;
		}
		device = gcm_ddc_device_new ();
		gcm_ddc_device_set_verbose (device, client->priv->verbose);
		ret = gcm_ddc_device_open (device, filename, &error_local);
		if (!ret) {
			if (client->priv->verbose == GCM_VERBOSE_OVERVIEW)
				g_warning ("failed to open %s: %s", filename, error_local->message);
			g_clear_error (&error_local);
		} else {
			if (client->priv->verbose == GCM_VERBOSE_OVERVIEW)
				g_debug ("success, adding %s", filename);
			any_found = TRUE;
			g_ptr_array_add (client->priv->devices, g_object_ref (device));
		}
		g_object_unref (device);
		g_free (filename);
	}

	/* nothing found */
	if (!any_found) {
		g_set_error_literal (error, GCM_DDC_CLIENT_ERROR, GCM_DDC_CLIENT_ERROR_FAILED,
				     "No devices found");
		goto out;
	}

	/* success */
	client->priv->has_coldplug = TRUE;
out:
	return any_found;
}

/**
 * gcm_ddc_client_close:
 * @client: a valid #GcmDdcClient instance
 * @error: a valid #GError, or %NULL
 *
 * Closes the client, releasing all devices.
 *
 * Return value: %TRUE for success.
 *
 * Since: 0.0.1
 **/
gboolean
gcm_ddc_client_close (GcmDdcClient *client, GError **error)
{
	guint i;
	gboolean ret = TRUE;
	GcmDdcDevice *device;

	g_return_val_if_fail (GCM_IS_DDC_CLIENT(client), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* iterate each device */
	for (i=0; i<client->priv->devices->len; i++) {
		device = g_ptr_array_index (client->priv->devices, i);
		ret = gcm_ddc_device_close (device, error);
		if (!ret)
			goto out;
	}
out:
	return ret;
}

/**
 * gcm_ddc_client_get_devices:
 * @client: a valid #GcmDdcClient instance
 * @error: a valid #GError, or %NULL
 *
 * Get all the #GcmDdcDevice's from the client.
 * If the client has not been loaded it will be done automatically.
 *
 * Return value: a #GPtrArray of #GcmDdcDevice's, free with g_ptr_array_unref()
 *
 * Since: 0.0.1
 **/
GPtrArray *
gcm_ddc_client_get_devices (GcmDdcClient *client, GError **error)
{
	gboolean ret;
	GPtrArray *devices = NULL;

	g_return_val_if_fail (GCM_IS_DDC_CLIENT(client), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	/* get capabilities */
	ret = gcm_ddc_client_ensure_coldplug (client, error);
	if (!ret)
		goto out;

	/* success */
	devices = g_ptr_array_ref (client->priv->devices);
out:
	return devices;
}

/**
 * gcm_ddc_client_get_device_from_edid:
 * @client: a valid #GcmDdcClient instance
 * @edid_md5: a EDID checksum
 * @error: a valid #GError, or %NULL
 *
 * Get a DDC device from it's EDID value.
 * If the client has not been loaded it will be done automatically.
 *
 * Return value: A refcounted #GcmDdcDevice, or %NULL.
 *
 * Since: 0.0.1
 **/
GcmDdcDevice *
gcm_ddc_client_get_device_from_edid (GcmDdcClient *client, const gchar *edid_md5, GError **error)
{
	guint i;
	gboolean ret;
	const gchar *edid_md5_tmp;
	GcmDdcDevice *device = NULL;
	GcmDdcDevice *device_tmp;

	g_return_val_if_fail (GCM_IS_DDC_CLIENT(client), NULL);
	g_return_val_if_fail (edid_md5 != NULL, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	/* get capabilities */
	ret = gcm_ddc_client_ensure_coldplug (client, error);
	if (!ret)
		goto out;

	/* iterate each device */
	for (i=0; i<client->priv->devices->len; i++) {
		device_tmp = g_ptr_array_index (client->priv->devices, i);

		/* get the md5 of the device */
		edid_md5_tmp = gcm_ddc_device_get_edid_md5 (device_tmp, error);
		if (edid_md5_tmp == NULL)
			goto out;

		/* matches? */
		if (g_strcmp0 (edid_md5, edid_md5_tmp) == 0) {
			device = device_tmp;
			break;
		}
	}

	/* failure */
	if (device == NULL) {
		g_set_error (error, GCM_DDC_CLIENT_ERROR, GCM_DDC_CLIENT_ERROR_FAILED,
			     "No devices found with edid %s", edid_md5);
	}
out:
	return device;
}

/**
 * gcm_ddc_client_set_verbose:
 * @client: a valid #GcmDdcClient instance
 * @verbose: the logging setting, e.g. %GCM_VERBOSE_PROTOCOL.
 *
 * Sets the logging level for this instance.
 *
 * Since: 0.0.1
 **/
void
gcm_ddc_client_set_verbose (GcmDdcClient *client, GcmVerbose verbose)
{
	g_return_if_fail (GCM_IS_DDC_CLIENT(client));
	client->priv->verbose = verbose;
}

/**
 * gcm_ddc_client_error_quark:
 *
 * Return value: Our personal error quark.
 *
 * Since: 0.0.1
 **/
GQuark
gcm_ddc_client_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("gcm_ddc_client_error");
	return quark;
}

/**
 * gcm_ddc_client_get_property:
 **/
static void
gcm_ddc_client_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GcmDdcClient *client = GCM_DDC_CLIENT (object);
	GcmDdcClientPrivate *priv = client->priv;

	switch (prop_id) {
	case PROP_HAS_COLDPLUG:
		g_value_set_boolean (value, priv->has_coldplug);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gcm_ddc_client_set_property:
 **/
static void
gcm_ddc_client_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gcm_ddc_client_class_init:
 **/
static void
gcm_ddc_client_class_init (GcmDdcClientClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gcm_ddc_client_finalize;
	object_class->get_property = gcm_ddc_client_get_property;
	object_class->set_property = gcm_ddc_client_set_property;

	/**
	 * GcmDdcClient:has-coldplug:
	 *
	 * Since: 0.0.1
	 */
	pspec = g_param_spec_boolean ("has-coldplug", NULL, "if there are no transactions in progress on this client",
				      TRUE,
				      G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_HAS_COLDPLUG, pspec);

	g_type_class_add_private (klass, sizeof (GcmDdcClientPrivate));
}

/**
 * gcm_ddc_client_init:
 **/
static void
gcm_ddc_client_init (GcmDdcClient *client)
{
	client->priv = GCM_DDC_CLIENT_GET_PRIVATE (client);
	client->priv->devices = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
}

/**
 * gcm_ddc_client_finalize:
 **/
static void
gcm_ddc_client_finalize (GObject *object)
{
	GcmDdcClient *client = GCM_DDC_CLIENT (object);
	GcmDdcClientPrivate *priv = client->priv;

	g_return_if_fail (GCM_IS_DDC_CLIENT(client));

	g_ptr_array_unref (priv->devices);
	G_OBJECT_CLASS (gcm_ddc_client_parent_class)->finalize (object);
}

/**
 * gcm_ddc_client_new:
 *
 * Return value: A new #GcmDdcClient instance
 *
 * Since: 0.0.1
 **/
GcmDdcClient *
gcm_ddc_client_new (void)
{
	GcmDdcClient *client;
	client = g_object_new (GCM_TYPE_DDC_CLIENT, NULL);
	return GCM_DDC_CLIENT (client);
}

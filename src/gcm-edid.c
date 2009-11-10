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
 * GNU General Public License for more edid.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:gcm-edid
 * @short_description: EDID parsing object
 *
 * This object parses EDID data blocks.
 */

#include "config.h"

#include <glib-object.h>
#include <math.h>
#include <string.h>
#include <gio/gio.h>
#include <stdlib.h>

#include "gcm-edid.h"
#include "gcm-tables.h"

#include "egg-debug.h"

static void     gcm_edid_finalize	(GObject     *object);

#define GCM_EDID_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GCM_TYPE_EDID, GcmEdidPrivate))

/**
 * GcmEdidPrivate:
 *
 * Private #GcmEdid data
 **/
struct _GcmEdidPrivate
{
	gchar				*monitor_name;
	gchar				*vendor_name;
	gchar				*serial_number;
	gchar				*ascii_string;
	gchar				*pnp_id;
	guint				 width;
	guint				 height;
	gfloat				 gamma;
	GcmTables			*tables;
};

enum {
	PROP_0,
	PROP_MONITOR_NAME,
	PROP_VENDOR_NAME,
	PROP_SERIAL_NUMBER,
	PROP_ASCII_STRING,
	PROP_GAMMA,
	PROP_PNP_ID,
	PROP_WIDTH,
	PROP_HEIGHT,
	PROP_LAST
};

G_DEFINE_TYPE (GcmEdid, gcm_edid, G_TYPE_OBJECT)

#define GCM_EDID_OFFSET_GAMMA				0x17
#define GCM_EDID_OFFSET_PNPID				0x08
#define GCM_EDID_OFFSET_SIZE				0x15
#define GCM_EDID_OFFSET_DATA_BLOCKS			0x36
#define GCM_EDID_OFFSET_LAST_BLOCK			0x6c

#define GCM_DESCRIPTOR_DISPLAY_PRODUCT_NAME		0xfc
#define GCM_DESCRIPTOR_DISPLAY_PRODUCT_SERIAL_NUMBER	0xff
#define GCM_DESCRIPTOR_COLOR_MANAGEMENT_DATA		0xf9
#define GCM_DESCRIPTOR_ALPHANUMERIC_DATA_STRING		0xfe
#define GCM_DESCRIPTOR_COLOR_POINT			0xfb


/**
 * gcm_edid_parse:
 **/
gboolean
gcm_edid_parse (GcmEdid *edid, const guint8 *data, GError **error)
{
	gboolean ret = TRUE;
	guint i;
	GcmEdidPrivate *priv = edid->priv;

	g_return_val_if_fail (GCM_IS_EDID (edid), FALSE);
	g_return_val_if_fail (data != NULL, FALSE);

	/* check header */
	if (data[0] != 0x00 || data[1] != 0xff) {
		if (error != NULL)
			*error = g_error_new (1, 0, "failed to parse header");
		ret = FALSE;
		goto out;
	}

	/* free old data */
	g_free (priv->monitor_name);
	g_free (priv->vendor_name);
	g_free (priv->serial_number);
	g_free (priv->ascii_string);
	priv->monitor_name = NULL;
	priv->vendor_name = NULL;
	priv->serial_number = NULL;
	priv->ascii_string = NULL;

	/* decode the PNP ID from three 5 bit words packed into 2 bytes
	 * /--08--\/--09--\
	 * 7654321076543210
	 * |\---/\---/\---/
	 * R  C1   C2   C3 */
	priv->pnp_id[0] = 'A' + ((data[GCM_EDID_OFFSET_PNPID+0] & 0x7c) / 4) - 1;
	priv->pnp_id[1] = 'A' + ((data[GCM_EDID_OFFSET_PNPID+0] & 0x3) * 8) + ((data[GCM_EDID_OFFSET_PNPID+1] & 0xe0) / 32) - 1;
	priv->pnp_id[2] = 'A' + (data[GCM_EDID_OFFSET_PNPID+1] & 0x1f) - 1;
	egg_debug ("PNPID: %s", priv->pnp_id);

	/* get the size */
	priv->width = data[GCM_EDID_OFFSET_SIZE+0];
	priv->height = data[GCM_EDID_OFFSET_SIZE+1];

	/* we don't care about aspect */
	if (priv->width == 0 || priv->height == 0) {
		priv->width = 0;
		priv->height = 0;
	}

	/* get gamma */
	if (data[GCM_EDID_OFFSET_GAMMA] == 0xff) {
		priv->gamma = 1.0f;
		egg_debug ("gamma is stored in an extension block");
	} else {
		priv->gamma = ((gfloat) data[GCM_EDID_OFFSET_GAMMA] / 100) + 1;
		egg_debug ("gamma is reported as %f", priv->gamma);
	}

	/* parse EDID data */
	for (i=GCM_EDID_OFFSET_DATA_BLOCKS; i <= GCM_EDID_OFFSET_LAST_BLOCK; i+=18) {
		/* ignore pixel clock data */
		if (data[i] != 0)
			continue;
		if (data[i+2] != 0)
			continue;

		/* any useful blocks? */
		if (data[i+3] == GCM_DESCRIPTOR_DISPLAY_PRODUCT_NAME)
			priv->monitor_name = g_strdup ((const gchar *) &data[i+5]);
		else if (data[i+3] == GCM_DESCRIPTOR_DISPLAY_PRODUCT_SERIAL_NUMBER)
			priv->serial_number = g_strdup ((const gchar *) &data[i+5]);
		else if (data[i+3] == GCM_DESCRIPTOR_COLOR_MANAGEMENT_DATA)
			egg_warning ("failing to parse color management data");
		else if (data[i+3] == GCM_DESCRIPTOR_ALPHANUMERIC_DATA_STRING)
			priv->ascii_string = g_strdup ((const gchar *) &data[i+5]);
		else if (data[i+3] == GCM_DESCRIPTOR_COLOR_POINT) {
			if (data[i+3+9] != 0xff) {
				egg_debug ("extended EDID block(1) which contains a better gamma value");
				priv->gamma = ((gfloat) data[i+3+9] / 100) + 1;
				egg_debug ("gamma is overridden as %f", priv->gamma);
			}
			if (data[i+3+14] != 0xff) {
				egg_debug ("extended EDID block(2) which contains a better gamma value");
				priv->gamma = ((gfloat) data[i+3+9] / 100) + 1;
				egg_debug ("gamma is overridden as %f", priv->gamma);
			}
		}
	}

	/* remove embedded newlines */
	if (priv->monitor_name != NULL)
		g_strdelimit (priv->monitor_name, "\n", '\0');
	if (priv->serial_number != NULL)
		g_strdelimit (priv->serial_number, "\n", '\0');
	if (priv->ascii_string != NULL)
		g_strdelimit (priv->ascii_string, "\n", '\0');
out:
	return ret;
}

/**
 * gcm_edid_get_property:
 **/
static void
gcm_edid_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GcmEdid *edid = GCM_EDID (object);
	GcmEdidPrivate *priv = edid->priv;

	switch (prop_id) {
	case PROP_MONITOR_NAME:
		g_value_set_string (value, priv->monitor_name);
		break;
	case PROP_VENDOR_NAME:
		if (priv->vendor_name == NULL)
			priv->vendor_name = gcm_tables_get_pnp_id (priv->tables, priv->pnp_id, NULL);
		g_value_set_string (value, priv->vendor_name);
		break;
	case PROP_SERIAL_NUMBER:
		g_value_set_string (value, priv->serial_number);
		break;
	case PROP_ASCII_STRING:
		g_value_set_string (value, priv->ascii_string);
		break;
	case PROP_GAMMA:
		g_value_set_float (value, priv->gamma);
		break;
	case PROP_PNP_ID:
		g_value_set_string (value, priv->pnp_id);
		break;
	case PROP_WIDTH:
		g_value_set_uint (value, priv->width);
		break;
	case PROP_HEIGHT:
		g_value_set_uint (value, priv->height);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gcm_edid_set_property:
 **/
static void
gcm_edid_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gcm_edid_class_init:
 **/
static void
gcm_edid_class_init (GcmEdidClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gcm_edid_finalize;
	object_class->get_property = gcm_edid_get_property;
	object_class->set_property = gcm_edid_set_property;

	/**
	 * GcmEdid:monitor-name:
	 */
	pspec = g_param_spec_string ("monitor-name", NULL, NULL,
				     NULL,
				     G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_MONITOR_NAME, pspec);

	/**
	 * GcmEdid:vendor-name:
	 */
	pspec = g_param_spec_string ("vendor-name", NULL, NULL,
				     NULL,
				     G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_VENDOR_NAME, pspec);

	/**
	 * GcmEdid:serial-number:
	 */
	pspec = g_param_spec_string ("serial-number", NULL, NULL,
				     NULL,
				     G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_SERIAL_NUMBER, pspec);

	/**
	 * GcmEdid:ascii-string:
	 */
	pspec = g_param_spec_string ("ascii-string", NULL, NULL,
				     NULL,
				     G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_ASCII_STRING, pspec);

	/**
	 * GcmEdid:gamma:
	 */
	pspec = g_param_spec_float ("gamma", NULL, NULL,
				    1.0f, 5.0f, 1.0f,
				    G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_GAMMA, pspec);

	/**
	 * GcmEdid:pnp-id:
	 */
	pspec = g_param_spec_string ("pnp-id", NULL, NULL,
				     NULL,
				     G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_PNP_ID, pspec);

	/**
	 * GcmEdid:width:
	 */
	pspec = g_param_spec_uint ("width", "in cm", NULL,
				   0, G_MAXUINT, 0,
				   G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_WIDTH, pspec);

	/**
	 * GcmEdid:height:
	 */
	pspec = g_param_spec_uint ("height", "in cm", NULL,
				   0, G_MAXUINT, 0,
				   G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_HEIGHT, pspec);

	g_type_class_add_private (klass, sizeof (GcmEdidPrivate));
}

/**
 * gcm_edid_init:
 **/
static void
gcm_edid_init (GcmEdid *edid)
{
	edid->priv = GCM_EDID_GET_PRIVATE (edid);
	edid->priv->monitor_name = NULL;
	edid->priv->vendor_name = NULL;
	edid->priv->serial_number = NULL;
	edid->priv->ascii_string = NULL;
	edid->priv->tables = gcm_tables_new ();
	edid->priv->pnp_id = g_new0 (gchar, 4);
}

/**
 * gcm_edid_finalize:
 **/
static void
gcm_edid_finalize (GObject *object)
{
	GcmEdid *edid = GCM_EDID (object);
	GcmEdidPrivate *priv = edid->priv;

	g_free (priv->monitor_name);
	g_free (priv->vendor_name);
	g_free (priv->serial_number);
	g_free (priv->ascii_string);
	g_free (priv->pnp_id);
	g_object_unref (priv->tables);

	G_OBJECT_CLASS (gcm_edid_parent_class)->finalize (object);
}

/**
 * gcm_edid_new:
 *
 * Return value: a new GcmEdid object.
 **/
GcmEdid *
gcm_edid_new (void)
{
	GcmEdid *edid;
	edid = g_object_new (GCM_TYPE_EDID, NULL);
	return GCM_EDID (edid);
}


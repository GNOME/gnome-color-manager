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
	gchar				*serial_number;
	gchar				*ascii_string;
};

enum {
	PROP_0,
	PROP_MONITOR_NAME,
	PROP_SERIAL_NUMBER,
	PROP_ASCII_STRING,
	PROP_LAST
};

G_DEFINE_TYPE (GcmEdid, gcm_edid, G_TYPE_OBJECT)

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

	/* chekc header */
	if (data[0] != 0x00 || data[1] != 0xff) {
		if (error != NULL)
			*error = g_error_new (1, 0, "failed to parse header");
		ret = FALSE;
		goto out;
	}

	/* free old data */
	g_free (priv->monitor_name);
	g_free (priv->serial_number);
	g_free (priv->ascii_string);
	priv->monitor_name = NULL;
	priv->serial_number = NULL;
	priv->ascii_string = NULL;

	/* parse EDID data */
	for (i=54; i <= 108; i+=18) {
		/* ignore pixel clock data */
		if (data[i] != 0)
			continue;
		if (data[i+2] != 0)
			continue;

		/* any useful blocks? */
		if (data[i+3] == 0xfc)
			priv->monitor_name = g_strdup (&data[i+5]);
		else if (data[i+3] == 0xff)
			priv->serial_number = g_strdup (&data[i+5]);
		else if (data[i+3] == 0xfe)
			priv->ascii_string = g_strdup (&data[i+5]);
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
	case PROP_SERIAL_NUMBER:
		g_value_set_string (value, priv->serial_number);
		break;
	case PROP_ASCII_STRING:
		g_value_set_string (value, priv->ascii_string);
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
	GcmEdid *edid = GCM_EDID (object);
	GcmEdidPrivate *priv = edid->priv;

	switch (prop_id) {
	case PROP_MONITOR_NAME:
		g_free (priv->monitor_name);
		priv->monitor_name = g_strdup (g_value_get_string (value));
		break;
	case PROP_SERIAL_NUMBER:
		g_free (priv->serial_number);
		priv->serial_number = g_strdup (g_value_get_string (value));
		break;
	case PROP_ASCII_STRING:
		g_free (priv->ascii_string);
		priv->ascii_string = g_strdup (g_value_get_string (value));
		break;
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
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_MONITOR_NAME, pspec);

	/**
	 * GcmEdid:serial-number:
	 */
	pspec = g_param_spec_string ("serial-number", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_SERIAL_NUMBER, pspec);

	/**
	 * GcmEdid:ascii-string:
	 */
	pspec = g_param_spec_string ("ascii-string", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_ASCII_STRING, pspec);

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
	edid->priv->serial_number = NULL;
	edid->priv->ascii_string = NULL;
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
	g_free (priv->serial_number);
	g_free (priv->ascii_string);

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


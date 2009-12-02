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

#define GCM_EDID_OFFSET_PNPID				0x08
#define GCM_EDID_OFFSET_SERIAL				0x0c
#define GCM_EDID_OFFSET_SIZE				0x15
#define GCM_EDID_OFFSET_GAMMA				0x17
#define GCM_EDID_OFFSET_DATA_BLOCKS			0x36
#define GCM_EDID_OFFSET_LAST_BLOCK			0x6c
#define GCM_EDID_OFFSET_EXTENSION_BLOCK_COUNT		0x7e

#define GCM_DESCRIPTOR_DISPLAY_PRODUCT_NAME		0xfc
#define GCM_DESCRIPTOR_DISPLAY_PRODUCT_SERIAL_NUMBER	0xff
#define GCM_DESCRIPTOR_COLOR_MANAGEMENT_DATA		0xf9
#define GCM_DESCRIPTOR_ALPHANUMERIC_DATA_STRING		0xfe
#define GCM_DESCRIPTOR_COLOR_POINT			0xfb


/**
 * gcm_edid_reset:
 **/
void
gcm_edid_reset (GcmEdid *edid)
{
	GcmEdidPrivate *priv = edid->priv;

	g_return_if_fail (GCM_IS_EDID (edid));

	/* free old data */
	g_free (priv->monitor_name);
	g_free (priv->vendor_name);
	g_free (priv->serial_number);
	g_free (priv->ascii_string);

	/* do not deallocate, just blank */
	priv->pnp_id[0] = '\0';

	/* set to default values */
	priv->monitor_name = NULL;
	priv->vendor_name = NULL;
	priv->serial_number = NULL;
	priv->ascii_string = NULL;
	priv->width = 0;
	priv->height = 0;
	priv->gamma = 0.0f;
}

/**
 * gcm_edid_parse:
 **/
gboolean
gcm_edid_parse (GcmEdid *edid, const guint8 *data, GError **error)
{
	gboolean ret = TRUE;
	guint i;
	GcmEdidPrivate *priv = edid->priv;
	guint32 serial;
	guint extension_blocks;

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
	gcm_edid_reset (edid);

	/* decode the PNP ID from three 5 bit words packed into 2 bytes
	 * /--08--\/--09--\
	 * 7654321076543210
	 * |\---/\---/\---/
	 * R  C1   C2   C3 */
	priv->pnp_id[0] = 'A' + ((data[GCM_EDID_OFFSET_PNPID+0] & 0x7c) / 4) - 1;
	priv->pnp_id[1] = 'A' + ((data[GCM_EDID_OFFSET_PNPID+0] & 0x3) * 8) + ((data[GCM_EDID_OFFSET_PNPID+1] & 0xe0) / 32) - 1;
	priv->pnp_id[2] = 'A' + (data[GCM_EDID_OFFSET_PNPID+1] & 0x1f) - 1;
	egg_debug ("PNPID: %s", priv->pnp_id);

	/* maybe there isn't a ASCII serial number descriptor, so use this instead */
	serial = (guint32) data[GCM_EDID_OFFSET_SERIAL+0];
	serial += (guint32) data[GCM_EDID_OFFSET_SERIAL+1] * 0x100;
	serial += (guint32) data[GCM_EDID_OFFSET_SERIAL+2] * 0x10000;
	serial += (guint32) data[GCM_EDID_OFFSET_SERIAL+3] * 0x1000000;
	if (serial > 0) {
		priv->serial_number = g_strdup_printf ("%" G_GUINT32_FORMAT, serial);
		egg_debug ("Serial: %s", priv->serial_number);
	}

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
		if (data[i+3] == GCM_DESCRIPTOR_DISPLAY_PRODUCT_NAME) {
			priv->monitor_name = g_strndup ((const gchar *) &data[i+5], 12);
			egg_debug ("[extended EDID block] monitor name: %s", priv->monitor_name);
		} else if (data[i+3] == GCM_DESCRIPTOR_DISPLAY_PRODUCT_SERIAL_NUMBER) {
			g_free (priv->serial_number);
			priv->serial_number = g_strndup ((const gchar *) &data[i+5], 12);
			egg_debug ("[extended EDID block] serial number: %s", priv->serial_number);
		} else if (data[i+3] == GCM_DESCRIPTOR_COLOR_MANAGEMENT_DATA) {
			egg_warning ("failing to parse color management data");
		} else if (data[i+3] == GCM_DESCRIPTOR_ALPHANUMERIC_DATA_STRING) {
			priv->ascii_string = g_strndup ((const gchar *) &data[i+5], 12);
			egg_debug ("[extended EDID block] ascii string: %s", priv->ascii_string);
		} else if (data[i+3] == GCM_DESCRIPTOR_COLOR_POINT) {
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

	/* extension blocks */
	extension_blocks = data[GCM_EDID_OFFSET_EXTENSION_BLOCK_COUNT];
	if (extension_blocks > 0)
		egg_warning ("%i extension blocks to parse", extension_blocks);

	/* remove embedded newlines */
	if (priv->monitor_name != NULL) {
		g_strchomp (priv->monitor_name);
		g_strdelimit (priv->monitor_name, "\n\r", '\0');
	}
	if (priv->serial_number != NULL) {
		g_strchomp (priv->serial_number);
		g_strdelimit (priv->serial_number, "\n\r", '\0');
	}
	if (priv->ascii_string != NULL) {
		g_strchomp (priv->ascii_string);
		g_strdelimit (priv->ascii_string, "\n\r", '\0');
	}
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

/***************************************************************************
 ***                          MAKE CHECK TESTS                           ***
 ***************************************************************************/
#ifdef EGG_TEST
#include "egg-test.h"

typedef struct {
	const gchar *monitor_name;
	const gchar *vendor_name;
	const gchar *serial_number;
	const gchar *ascii_string;
	const gchar *pnp_id;
	guint width;
	guint height;
	gfloat gamma;
} GcmEdidTestData;

void
gcm_edid_test_parse_edid_file (EggTest *test, GcmEdid *edid, const gchar *datafile, GcmEdidTestData *test_data)
{
	gchar *filename;
	gchar *monitor_name;
	gchar *vendor_name;
	gchar *serial_number;
	gchar *ascii_string;
	gchar *pnp_id;
	gchar *data;
	guint width;
	guint height;
	gfloat gamma;
	gboolean ret;
	GError *error = NULL;

	/************************************************************/
	egg_test_title (test, "get filename of data file");
	filename = egg_test_get_data_file (datafile);
	ret = g_file_get_contents (filename, &data, NULL, &error);
	if (ret)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "failed to load: %s", error->message);

	/************************************************************/
	egg_test_title (test, "parse an example edid block");
	ret = gcm_edid_parse (edid, data, &error);
	if (ret)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "failed to parse: %s", error->message);

	g_object_get (edid,
		      "monitor-name", &monitor_name,
		      "vendor-name", &vendor_name,
		      "serial-number", &serial_number,
		      "ascii-string", &ascii_string,
		      "pnp-id", &pnp_id,
		      "width", &width,
		      "height", &height,
		      "gamma", &gamma,
		      NULL);

	/************************************************************/
	egg_test_title (test, "check monitor name for %s", datafile);
	if (g_strcmp0 (monitor_name, test_data->monitor_name) == 0)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "invalid value: %s, expecting: %s", monitor_name, test_data->monitor_name);

	/************************************************************/
	egg_test_title (test, "check vendor name for %s", datafile);
	if (g_strcmp0 (vendor_name, test_data->vendor_name) == 0)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "invalid value: %s, expecting: %s", vendor_name, test_data->vendor_name);

	/************************************************************/
	egg_test_title (test, "check serial number for %s", datafile);
	if (g_strcmp0 (serial_number, test_data->serial_number) == 0)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "invalid value: %s, expecting: %s", serial_number, test_data->serial_number);

	/************************************************************/
	egg_test_title (test, "check ascii string for %s", datafile);
	if (g_strcmp0 (ascii_string, test_data->ascii_string) == 0)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "invalid value: %s, expecting: %s", ascii_string, test_data->ascii_string);

	/************************************************************/
	egg_test_title (test, "check pnp id for %s", datafile);
	if (g_strcmp0 (pnp_id, test_data->pnp_id) == 0)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "invalid value: %s, expecting: %s", pnp_id, test_data->pnp_id);

	/************************************************************/
	egg_test_title (test, "check height for %s", datafile);
	if (height == test_data->height)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "invalid value: %i, expecting: %i", height, test_data->height);

	/************************************************************/
	egg_test_title (test, "check width for %s", datafile);
	if (width == test_data->width)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "invalid value: %i, expecting: %i", width, test_data->width);

	/************************************************************/
	egg_test_title (test, "check gamma for %s", datafile);
	if (gamma > (test_data->gamma - 0.01) && gamma < (test_data->gamma + 0.01))
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "invalid value: %f, expecting: %f", gamma, test_data->gamma);

	g_free (monitor_name);
	g_free (vendor_name);
	g_free (serial_number);
	g_free (ascii_string);
	g_free (pnp_id);
	g_free (data);
	g_free (filename);
}

void
gcm_edid_test (EggTest *test)
{
	GcmEdid *edid;
	gboolean ret;
	GError *error = NULL;
	GcmEdidTestData test_data;

	if (!egg_test_start (test, "GcmEdid"))
		return;

	/************************************************************/
	egg_test_title (test, "get a edid object");
	edid = gcm_edid_new ();
	egg_test_assert (test, edid != NULL);

	/* LG 21" LCD panel */
	test_data.monitor_name = "L225W";
	test_data.vendor_name = "Goldstar Company Ltd";
	test_data.serial_number = "34398";
	test_data.ascii_string = NULL;
	test_data.pnp_id = "GSM";
	test_data.height = 30;
	test_data.width = 47;
	test_data.gamma = 2.2f;
	gcm_edid_test_parse_edid_file (test, edid, "LG-L225W-External.bin", &test_data);

	/* Lenovo T61 Intel Panel */
	test_data.monitor_name = NULL;
	test_data.vendor_name = "IBM France";
	test_data.serial_number = NULL;
	test_data.ascii_string = "LTN154P2-L05";
	test_data.pnp_id = "IBM";
	test_data.height = 21;
	test_data.width = 33;
	test_data.gamma = 2.2f;
	gcm_edid_test_parse_edid_file (test, edid, "Lenovo-T61-Internal.bin", &test_data);

	g_object_unref (edid);

	egg_test_end (test);
}
#endif


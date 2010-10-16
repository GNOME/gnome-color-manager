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

#include "gcm-color.h"
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
	gchar				*eisa_id;
	gchar				*checksum;
	gchar				*pnp_id;
	guint				 width;
	guint				 height;
	gfloat				 gamma;
	GcmColorYxy			*red;
	GcmColorYxy			*green;
	GcmColorYxy			*blue;
	GcmColorYxy			*white;
	GcmTables			*tables;
};

enum {
	PROP_0,
	PROP_MONITOR_NAME,
	PROP_VENDOR_NAME,
	PROP_SERIAL_NUMBER,
	PROP_EISA_ID,
	PROP_CHECKSUM,
	PROP_GAMMA,
	PROP_PNP_ID,
	PROP_WIDTH,
	PROP_HEIGHT,
	PROP_RED,
	PROP_GREEN,
	PROP_BLUE,
	PROP_WHITE,
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
 * gcm_edid_get_monitor_name:
 * @edid: a valid #GcmEdid instance
 *
 * Gets the monitor name.
 *
 * Return value: a null terminaled string value
 *
 * Since: 2.91.1
 **/
const gchar *
gcm_edid_get_monitor_name (GcmEdid *edid)
{
	g_return_val_if_fail (GCM_IS_EDID (edid), NULL);
	return edid->priv->monitor_name;
}

/**
 * gcm_edid_get_vendor_name:
 * @edid: a valid #GcmEdid instance
 *
 * Gets the vendor name.
 *
 * Return value: a null terminaled string value
 *
 * Since: 2.91.1
 **/
const gchar *
gcm_edid_get_vendor_name (GcmEdid *edid)
{
	GcmEdidPrivate *priv = edid->priv;
	g_return_val_if_fail (GCM_IS_EDID (edid), NULL);

	if (priv->vendor_name == NULL)
		priv->vendor_name = gcm_tables_get_pnp_id (priv->tables, priv->pnp_id, NULL);
	return priv->vendor_name;
}

/**
 * gcm_edid_get_serial_number:
 * @edid: a valid #GcmEdid instance
 *
 * Gets the device serial number.
 *
 * Return value: a null terminaled string value
 *
 * Since: 2.91.1
 **/
const gchar *
gcm_edid_get_serial_number (GcmEdid *edid)
{
	g_return_val_if_fail (GCM_IS_EDID (edid), NULL);
	return edid->priv->serial_number;
}

/**
 * gcm_edid_get_eisa_id:
 * @edid: a valid #GcmEdid instance
 *
 * Gets the EISA ID.
 *
 * Return value: a null terminaled string value
 *
 * Since: 2.91.1
 **/
const gchar *
gcm_edid_get_eisa_id (GcmEdid *edid)
{
	g_return_val_if_fail (GCM_IS_EDID (edid), NULL);
	return edid->priv->eisa_id;
}

/**
 * gcm_edid_get_checksum:
 * @edid: a valid #GcmEdid instance
 *
 * Gets the EDID MD5 checksum, which is often useful to compare binary
 * blobs.
 *
 * Return value: a null terminaled string value
 *
 * Since: 2.91.1
 **/
const gchar *
gcm_edid_get_checksum (GcmEdid *edid)
{
	g_return_val_if_fail (GCM_IS_EDID (edid), NULL);
	return edid->priv->checksum;
}

/**
 * gcm_edid_get_pnp_id:
 * @edid: a valid #GcmEdid instance
 *
 * Gets the device PNP ID..
 *
 * Return value: a null terminaled string value
 *
 * Since: 2.91.1
 **/
const gchar *
gcm_edid_get_pnp_id (GcmEdid *edid)
{
	g_return_val_if_fail (GCM_IS_EDID (edid), NULL);
	return edid->priv->pnp_id;
}

/**
 * gcm_edid_get_width:
 * @edid: a valid #GcmEdid instance
 *
 * Gets the width of the panel.
 *
 * Return value: the width in mm.
 *
 * Since: 2.91.1
 **/
guint
gcm_edid_get_width (GcmEdid *edid)
{
	g_return_val_if_fail (GCM_IS_EDID (edid), 0);
	return edid->priv->width;
}

/**
 * gcm_edid_get_height:
 * @edid: a valid #GcmEdid instance
 *
 * Gets the panel height.
 *
 * Return value: the height in mm.
 *
 * Since: 2.91.1
 **/
guint
gcm_edid_get_height (GcmEdid *edid)
{
	g_return_val_if_fail (GCM_IS_EDID (edid), 0);
	return edid->priv->height;
}

/**
 * gcm_edid_get_gamma:
 * @edid: a valid #GcmEdid instance
 *
 * Gets the monitor gamma.
 *
 * Return value: the gamma value, although this is typically 2.2
 *
 * Since: 2.91.1
 **/
gfloat
gcm_edid_get_gamma (GcmEdid *edid)
{
	g_return_val_if_fail (GCM_IS_EDID (edid), 0.0f);
	return edid->priv->gamma;
}

/**
 * gcm_edid_get_red:
 * @edid: a valid #GcmEdid instance
 *
 * Gets the monitor red chromaticity value.
 *
 * Return value: the #GcmColorYxy value
 *
 * Since: 2.91.1
 **/
const GcmColorYxy *
gcm_edid_get_red (GcmEdid *edid)
{
	g_return_val_if_fail (GCM_IS_EDID (edid), NULL);
	return edid->priv->red;
}

/**
 * gcm_edid_get_green:
 * @edid: a valid #GcmEdid instance
 *
 * Gets the monitor green chromaticity value.
 *
 * Return value: the #GcmColorYxy value
 *
 * Since: 2.91.1
 **/
const GcmColorYxy *
gcm_edid_get_green (GcmEdid *edid)
{
	g_return_val_if_fail (GCM_IS_EDID (edid), NULL);
	return edid->priv->green;
}

/**
 * gcm_edid_get_blue:
 * @edid: a valid #GcmEdid instance
 *
 * Gets the monitor red chromaticity value.
 *
 * Return value: the #GcmColorYxy value
 *
 * Since: 2.91.1
 **/
const GcmColorYxy *
gcm_edid_get_blue (GcmEdid *edid)
{
	g_return_val_if_fail (GCM_IS_EDID (edid), NULL);
	return edid->priv->blue;
}

/**
 * gcm_edid_get_white:
 * @edid: a valid #GcmEdid instance
 *
 * Gets the monitor white chromaticity value.
 *
 * Return value: the #GcmColorYxy value
 *
 * Since: 2.91.1
 **/
const GcmColorYxy *
gcm_edid_get_white (GcmEdid *edid)
{
	g_return_val_if_fail (GCM_IS_EDID (edid), NULL);
	return edid->priv->white;
}

/**
 * gcm_edid_reset:
 * @edid: a valid #GcmEdid instance
 *
 * Resets all the cached values in this object.
 *
 * Since: 2.91.1
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
	g_free (priv->eisa_id);
	g_free (priv->checksum);

	/* do not deallocate, just blank */
	priv->pnp_id[0] = '\0';

	/* set to default values */
	priv->monitor_name = NULL;
	priv->vendor_name = NULL;
	priv->serial_number = NULL;
	priv->eisa_id = NULL;
	priv->checksum = NULL;
	priv->width = 0;
	priv->height = 0;
	priv->gamma = 0.0f;
}

/**
 * gcm_edid_get_bit:
 *
 * Originally Copyright Soren Sandmann <sandmann@redhat.com>
 **/
static gint
gcm_edid_get_bit (gint in, gint bit)
{
	return (in & (1 << bit)) >> bit;
}

/**
 * gcm_edid_get_bits:
 **/
static gint
gcm_edid_get_bits (gint in, gint begin, gint end)
{
	gint mask = (1 << (end - begin + 1)) - 1;

	return (in >> begin) & mask;
}

/**
 * gcm_edid_decode_fraction:
 **/
static gdouble
gcm_edid_decode_fraction (gint high, gint low)
{
	gdouble result = 0.0;
	gint i;

	high = (high << 2) | low;
	for (i = 0; i < 10; ++i)
		result += gcm_edid_get_bit (high, i) * pow (2, i - 10);
	return result;
}

/**
 * gcm_edid_parse_string:
 *
 * This is always 12 bytes, but we can't guarantee it's null terminated
 * or not junk.
 *
 * Return value: the sane text, or %NULL, use g_free() to unref.
 **/
static gchar *
gcm_edid_parse_string (const guint8 *data)
{
	gchar *text;
	guint i;
	guint replaced = 0;

	/* copy 12 bytes */
	text = g_strndup ((const gchar *) data, 12);

	/* remove insane newline chars */
	g_strdelimit (text, "\n\r", '\0');

	/* remove spaces */
	g_strchomp (text);

	/* nothing left? */
	if (text[0] == '\0') {
		g_free (text);
		text = NULL;
		goto out;
	}

	/* ensure string is printable */
	for (i=0; text[i] != '\0'; i++) {
		if (!g_ascii_isprint (text[i])) {
			text[i] = '-';
			replaced++;
		}
	}

	/* if the string is junk, ignore the string */
	if (replaced > 4) {
		g_free (text);
		text = NULL;
		goto out;
	}
out:
	return text;
}

/**
 * gcm_edid_parse:
 * @edid: a valid #GcmEdid instance
 * @data: the EDID block
 * @length: the data length, typically a multiple of 128
 * @error: a valid #GError or %NULL
 *
 * Parses an EDID block, checking for sanity and decoding anything
 * important.
 *
 * Return value: %TRUE for success.
 *
 * Since: 2.91.1
 **/
gboolean
gcm_edid_parse (GcmEdid *edid, const guint8 *data, gsize length, GError **error)
{
	gboolean ret = TRUE;
	guint i;
	GcmEdidPrivate *priv = edid->priv;
	guint32 serial;
	guint extension_blocks;
	gchar *tmp;

	g_return_val_if_fail (GCM_IS_EDID (edid), FALSE);
	g_return_val_if_fail (data != NULL, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* check header */
	if (data[0] != 0x00 || data[1] != 0xff) {
		g_set_error_literal (error, 1, 0, "failed to parse header");
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

	/* get color red */
	priv->red->x = gcm_edid_decode_fraction (data[0x1b], gcm_edid_get_bits (data[0x19], 6, 7));
	priv->red->y = gcm_edid_decode_fraction (data[0x1c], gcm_edid_get_bits (data[0x19], 5, 4));
	egg_debug ("red x=%f,y=%f", priv->red->x, priv->red->y);

	/* get color green */
	priv->green->x = gcm_edid_decode_fraction (data[0x1d], gcm_edid_get_bits (data[0x19], 2, 3));
	priv->green->y = gcm_edid_decode_fraction (data[0x1e], gcm_edid_get_bits (data[0x19], 0, 1));
	egg_debug ("green x=%f,y=%f", priv->green->x, priv->green->y);

	/* get color blue */
	priv->blue->x = gcm_edid_decode_fraction (data[0x1f], gcm_edid_get_bits (data[0x1a], 6, 7));
	priv->blue->y = gcm_edid_decode_fraction (data[0x20], gcm_edid_get_bits (data[0x1a], 4, 5));
	egg_debug ("blue x=%f,y=%f", priv->blue->x, priv->blue->y);

	/* get color white */
	priv->white->x = gcm_edid_decode_fraction (data[0x21], gcm_edid_get_bits (data[0x1a], 2, 3));
	priv->white->y = gcm_edid_decode_fraction (data[0x22], gcm_edid_get_bits (data[0x1a], 0, 1));
	egg_debug ("white x=%f,y=%f", priv->white->x, priv->white->y);

	/* parse EDID data */
	for (i=GCM_EDID_OFFSET_DATA_BLOCKS; i <= GCM_EDID_OFFSET_LAST_BLOCK; i+=18) {
		/* ignore pixel clock data */
		if (data[i] != 0)
			continue;
		if (data[i+2] != 0)
			continue;

		/* any useful blocks? */
		if (data[i+3] == GCM_DESCRIPTOR_DISPLAY_PRODUCT_NAME) {
			tmp = gcm_edid_parse_string (&data[i+5]);
			if (tmp != NULL) {
				g_free (priv->monitor_name);
				priv->monitor_name = tmp;
			}
		} else if (data[i+3] == GCM_DESCRIPTOR_DISPLAY_PRODUCT_SERIAL_NUMBER) {
			tmp = gcm_edid_parse_string (&data[i+5]);
			if (tmp != NULL) {
				g_free (priv->serial_number);
				priv->serial_number = tmp;
			}
		} else if (data[i+3] == GCM_DESCRIPTOR_COLOR_MANAGEMENT_DATA) {
			egg_warning ("failing to parse color management data");
		} else if (data[i+3] == GCM_DESCRIPTOR_ALPHANUMERIC_DATA_STRING) {
			tmp = gcm_edid_parse_string (&data[i+5]);
			if (tmp != NULL) {
				g_free (priv->eisa_id);
				priv->eisa_id = tmp;
			}
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

	/* calculate checksum */
	priv->checksum = g_compute_checksum_for_data (G_CHECKSUM_MD5, data, 0x6c);

	/* print what we've got */
	egg_debug ("monitor name: %s", priv->monitor_name);
	egg_debug ("serial number: %s", priv->serial_number);
	egg_debug ("ascii string: %s", priv->eisa_id);
	egg_debug ("checksum: %s", priv->checksum);
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
		g_value_set_string (value, gcm_edid_get_vendor_name (edid));
		break;
	case PROP_SERIAL_NUMBER:
		g_value_set_string (value, priv->serial_number);
		break;
	case PROP_EISA_ID:
		g_value_set_string (value, priv->eisa_id);
		break;
	case PROP_CHECKSUM:
		g_value_set_string (value, priv->checksum);
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
	case PROP_WHITE:
		g_value_set_boxed (value, g_boxed_copy (GCM_TYPE_COLOR_XYZ, priv->white));
		break;
	case PROP_RED:
		g_value_set_boxed (value, g_boxed_copy (GCM_TYPE_COLOR_XYZ, priv->red));
		break;
	case PROP_GREEN:
		g_value_set_boxed (value, g_boxed_copy (GCM_TYPE_COLOR_XYZ, priv->green));
		break;
	case PROP_BLUE:
		g_value_set_boxed (value, g_boxed_copy (GCM_TYPE_COLOR_XYZ, priv->blue));
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
	 * GcmEdid:eisa-id:
	 */
	pspec = g_param_spec_string ("eisa-id", NULL, NULL,
				     NULL,
				     G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_EISA_ID, pspec);

	/**
	 * GcmEdid:checksum:
	 */
	pspec = g_param_spec_string ("checksum", NULL, NULL,
				     NULL,
				     G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_CHECKSUM, pspec);

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

	/**
	 * GcmEdid:red:
	 */
	g_object_class_install_property (object_class,
					 PROP_RED,
					 g_param_spec_boxed ("red", NULL, NULL,
							     GCM_TYPE_COLOR_YXY,
							     G_PARAM_READABLE));
	/**
	 * GcmEdid:green:
	 */
	g_object_class_install_property (object_class,
					 PROP_GREEN,
					 g_param_spec_boxed ("green", NULL, NULL,
							     GCM_TYPE_COLOR_YXY,
							     G_PARAM_READABLE));
	/**
	 * GcmEdid:blue:
	 */
	g_object_class_install_property (object_class,
					 PROP_BLUE,
					 g_param_spec_boxed ("blue", NULL, NULL,
							     GCM_TYPE_COLOR_YXY,
							     G_PARAM_READABLE));
	/**
	 * GcmEdid:white:
	 */
	g_object_class_install_property (object_class,
					 PROP_WHITE,
					 g_param_spec_boxed ("white", NULL, NULL,
							     GCM_TYPE_COLOR_YXY,
							     G_PARAM_READABLE));

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
	edid->priv->eisa_id = NULL;
	edid->priv->checksum = NULL;
	edid->priv->tables = gcm_tables_new ();
	edid->priv->pnp_id = g_new0 (gchar, 4);
	edid->priv->red = gcm_color_new_Yxy ();
	edid->priv->green = gcm_color_new_Yxy ();
	edid->priv->blue = gcm_color_new_Yxy ();
	edid->priv->white = gcm_color_new_Yxy ();
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
	g_free (priv->eisa_id);
	g_free (priv->checksum);
	g_free (priv->pnp_id);
	gcm_color_free_Yxy (priv->white);
	gcm_color_free_Yxy (priv->red);
	gcm_color_free_Yxy (priv->green);
	gcm_color_free_Yxy (priv->blue);
	g_object_unref (priv->tables);

	G_OBJECT_CLASS (gcm_edid_parent_class)->finalize (object);
}

/**
 * gcm_edid_new:
 *
 * Return value: a new #GcmEdid object.
 *
 * Since: 2.91.1
 **/
GcmEdid *
gcm_edid_new (void)
{
	GcmEdid *edid;
	edid = g_object_new (GCM_TYPE_EDID, NULL);
	return GCM_EDID (edid);
}


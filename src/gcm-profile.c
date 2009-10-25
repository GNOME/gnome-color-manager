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
 * GNU General Public License for more profile.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:gcm-profile
 * @short_description: A parser object that understands the ICC profile data format.
 *
 * This object is a simple parser for the ICC binary profile data. If only understands
 * a subset of the ICC profile, just enought to get some metadata and the LUT.
 */

#include "config.h"

#include <glib-object.h>
#include <math.h>

#include "egg-debug.h"

#include "gcm-profile.h"

static void     gcm_profile_finalize	(GObject     *object);

#define GCM_PROFILE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GCM_TYPE_PROFILE, GcmProfilePrivate))

typedef enum {
	GCM_DATA_TYPE_MLUT,
	GCM_DATA_TYPE_VCGT_FORMULA,
	GCM_DATA_TYPE_VCGT_TABLE,
	GCM_DATA_TYPE_UNKNOWN
} GcmDataType;

#define GCM_HEADER			0x00
#define GCM_NUMTAGS			0x80
#define GCM_BODY			0x84

#define GCM_TAG_ID			0x00
#define GCM_TAG_OFFSET			0x04
#define GCM_TAG_SIZE			0x08
#define GCM_TAG_WIDTH			0x0c

#define GCM_TAG_ID_TEXT			0x63707274
#define GCM_TAG_ID_DESC			0x64657363
#define GCM_TAG_ID_VCGT			0x76636774
#define GCM_TAG_ID_MLUT			0x6d4c5554

#define GCM_MLUT_RED			0x000
#define GCM_MLUT_GREEN			0x200
#define GCM_MLUT_BLUE			0x400

#define GCM_VCGT_ID			0x00
#define GCM_VCGT_DUMMY			0x04
#define GCM_VCGT_GAMMA_TYPE		0x08
#define GCM_VCGT_GAMMA_DATA		0x0c

#define GCM_VCGT_FORMULA_GAMMA_RED	0x00
#define GCM_VCGT_FORMULA_MIN_RED	0x04
#define GCM_VCGT_FORMULA_MAX_RED	0x08
#define GCM_VCGT_FORMULA_GAMMA_GREEN	0x0c
#define GCM_VCGT_FORMULA_MIN_GREEN	0x10
#define GCM_VCGT_FORMULA_MAX_GREEN	0x14
#define GCM_VCGT_FORMULA_GAMMA_BLUE	0x18
#define GCM_VCGT_FORMULA_MIN_BLUE	0x1c
#define GCM_VCGT_FORMULA_MAX_BLUE	0x20

#define GCM_VCGT_TABLE_NUM_CHANNELS	0x00
#define GCM_VCGT_TABLE_NUM_ENTRIES	0x02
#define GCM_VCGT_TABLE_NUM_SIZE		0x04
#define GCM_VCGT_TABLE_NUM_DATA		0x06

/**
 * GcmProfilePrivate:
 *
 * Private #GcmProfile data
 **/
struct _GcmProfilePrivate
{
	gchar				*description;
	gchar				*copyright;
	GcmDataType			 data_type;
	GcmClutData			*ramp_data;
	guint				 ramp_data_size;
	gboolean			 adobe_gamma_workaround;
};

enum {
	PROP_0,
	PROP_COPYRIGHT,
	PROP_DESCRIPTION,
	PROP_LAST
};

G_DEFINE_TYPE (GcmProfile, gcm_profile, G_TYPE_OBJECT)

/**
 * gcm_parser_unencode_32:
 **/
static guint
gcm_parser_unencode_32 (const gchar *data, gsize offset)
{
	guint retval;
	retval = (data[offset+0] << 0) + (data[offset+1] << 8) + (data[offset+2] << 16) + (data[offset+3] << 24);
	return GUINT32_FROM_BE (retval);
}

/**
 * gcm_parser_unencode_16:
 **/
static guint
gcm_parser_unencode_16 (const gchar *data, gsize offset)
{
	guint retval;
	retval = (data[offset+0] << 0) + (data[offset+1] << 8);
	return GUINT16_FROM_BE (retval);
}

/**
 * gcm_parser_unencode_8:
 **/
static guint
gcm_parser_unencode_8 (const gchar *data, gsize offset)
{
	guint retval;
	retval = (data[offset+0] << 0);
	return GUINT16_FROM_BE (retval);
}

/**
 * gcm_parser_load_icc_mlut:
 **/
static gboolean
gcm_parser_load_icc_mlut (GcmProfile *profile, const gchar *data, gsize offset)
{
	gboolean ret = TRUE;
	guint i;
	GcmClutData *ramp_data;

	/* just load in data into a fixed size LUT */
	profile->priv->ramp_data = g_new0 (GcmClutData, 256);
	ramp_data = profile->priv->ramp_data;

	for (i=0; i<256; i++)
		ramp_data[i].red = gcm_parser_unencode_16 (data, offset + GCM_MLUT_RED + i*2);
	for (i=0; i<256; i++)
		ramp_data[i].green = gcm_parser_unencode_16 (data, offset + GCM_MLUT_GREEN + i*2);
	for (i=0; i<256; i++)
		ramp_data[i].blue = gcm_parser_unencode_16 (data, offset + GCM_MLUT_BLUE + i*2);

	/* save datatype */
	profile->priv->data_type = GCM_DATA_TYPE_MLUT;
	return ret;
}

/**
 * gcm_parser_load_icc_vcgt_formula:
 **/
static gboolean
gcm_parser_load_icc_vcgt_formula (GcmProfile *profile, const gchar *data, gsize offset)
{
	gboolean ret = FALSE;
	GcmClutData *ramp_data;

	egg_debug ("loading a formula encoded gamma table");

	/* just load in data into a temporary array */
	profile->priv->ramp_data = g_new0 (GcmClutData, 4);
	ramp_data = profile->priv->ramp_data;

	/* read in block of data */
	ramp_data[0].red = gcm_parser_unencode_32 (data, GCM_VCGT_FORMULA_GAMMA_RED);
	ramp_data[0].green = gcm_parser_unencode_32 (data, GCM_VCGT_FORMULA_GAMMA_GREEN);
	ramp_data[0].blue = gcm_parser_unencode_32 (data, GCM_VCGT_FORMULA_GAMMA_BLUE);

	ramp_data[1].red = gcm_parser_unencode_32 (data, GCM_VCGT_FORMULA_MIN_RED);
	ramp_data[1].green = gcm_parser_unencode_32 (data, GCM_VCGT_FORMULA_MIN_GREEN);
	ramp_data[1].blue = gcm_parser_unencode_32 (data, GCM_VCGT_FORMULA_MIN_BLUE);

	ramp_data[2].red = gcm_parser_unencode_32 (data, GCM_VCGT_FORMULA_MAX_RED);
	ramp_data[2].green = gcm_parser_unencode_32 (data, GCM_VCGT_FORMULA_MAX_GREEN);
	ramp_data[2].blue = gcm_parser_unencode_32 (data, GCM_VCGT_FORMULA_MAX_BLUE);

	/* check if valid */
	if (ramp_data[0].red / 65536.0 > 5.0 || ramp_data[0].green / 65536.0 > 5.0 || ramp_data[0].blue / 65536.0 > 5.0) {
		egg_warning ("Gamma values out of range: [R:%u G:%u B:%u]", ramp_data[0].red, ramp_data[0].green, ramp_data[0].blue);
		goto out;
	}
	if (ramp_data[1].red / 65536.0 >= 1.0 || ramp_data[1].green / 65536.0 >= 1.0 || ramp_data[1].blue / 65536.0 >= 1.0) {
		egg_warning ("Gamma min limit out of range: [R:%u G:%u B:%u]", ramp_data[1].red, ramp_data[1].green, ramp_data[1].blue);
		goto out;
	}
	if (ramp_data[2].red / 65536.0 > 1.0 || ramp_data[2].green / 65536.0 > 1.0 || ramp_data[2].blue / 65536.0 > 1.0) {
		egg_warning ("Gamma max limit out of range: [R:%u G:%u B:%u]", ramp_data[2].red, ramp_data[2].green, ramp_data[2].blue);
		goto out;
	}

	/* save datatype */
	profile->priv->data_type = GCM_DATA_TYPE_VCGT_FORMULA;
	profile->priv->ramp_data_size = 3;
	ret = TRUE;
out:
	return ret;
}

/**
 * gcm_parser_load_icc_vcgt_table:
 **/
static gboolean
gcm_parser_load_icc_vcgt_table (GcmProfile *profile, const gchar *data, gsize offset)
{
	gboolean ret = TRUE;
	guint num_channels = 0;
	guint num_entries = 0;
	guint entry_size = 0;
	guint i;
	GcmClutData *ramp_data;

	egg_debug ("loading a table encoded gamma table");

	num_channels = gcm_parser_unencode_16 (data, offset + GCM_VCGT_TABLE_NUM_CHANNELS);
	num_entries = gcm_parser_unencode_16 (data, offset + GCM_VCGT_TABLE_NUM_ENTRIES);
	entry_size = gcm_parser_unencode_16 (data, offset + GCM_VCGT_TABLE_NUM_SIZE);

	/* work-around for AdobeGamma-Profiles (taken from xcalib) */
	if (profile->priv->adobe_gamma_workaround) {
		egg_debug ("Working around AdobeGamma profile");
		entry_size = 2;
		num_entries = 256;
		num_channels = 3;
	}

	egg_debug ("channels: %u", num_channels);
	egg_debug ("entry size: %ubits", entry_size * 8);
	egg_debug ("entries/channel: %u", num_entries);

	/* only able to parse RGB data */
	if (num_channels != 3) {
		egg_warning ("cannot parse non RGB entries");
		goto out;
	}

	/* bigger than will fit in 16 bits? */
	if (entry_size > 2) {
		egg_warning ("cannot parse large entries");
		goto out;
	}

	/* allocate ramp, plus one entry for extrapolation */
	profile->priv->ramp_data = g_new0 (GcmClutData, num_entries+1);
	ramp_data = profile->priv->ramp_data;

	if (entry_size == 1) {
		for (i=0; i<num_entries; i++)
			ramp_data[i].red = gcm_parser_unencode_8 (data, offset + GCM_VCGT_TABLE_NUM_DATA + (num_entries * 0) + i);
		for (i=0; i<num_entries; i++)
			ramp_data[i].green = gcm_parser_unencode_8 (data, offset + GCM_VCGT_TABLE_NUM_DATA + (num_entries * 1) + i);
		for (i=0; i<num_entries; i++)
			ramp_data[i].blue = gcm_parser_unencode_8 (data, offset + GCM_VCGT_TABLE_NUM_DATA + (num_entries * 2) + i);
	} else {
		for (i=0; i<num_entries; i++)
			ramp_data[i].red = gcm_parser_unencode_16 (data, offset + GCM_VCGT_TABLE_NUM_DATA + (num_entries * 0) + (i*2));
		for (i=0; i<num_entries; i++)
			ramp_data[i].green = gcm_parser_unencode_16 (data, offset + GCM_VCGT_TABLE_NUM_DATA + (num_entries * 2) + (i*2));
		for (i=0; i<num_entries; i++)
			ramp_data[i].blue = gcm_parser_unencode_16 (data, offset + GCM_VCGT_TABLE_NUM_DATA + (num_entries * 4) + (i*2));
	}

	/* save datatype */
	profile->priv->data_type = GCM_DATA_TYPE_VCGT_TABLE;
	profile->priv->ramp_data_size = num_entries;
out:
	return ret;
}

/**
 * gcm_parser_load_icc_vcgt:
 **/
static gboolean
gcm_parser_load_icc_vcgt (GcmProfile *profile, const gchar *data, gsize offset)
{
	gboolean ret = FALSE;
	guint tag_id;
	guint gamma_type;

	/* check we have a VCGT block */
	tag_id = gcm_parser_unencode_32 (data, offset);
	if (tag_id != GCM_TAG_ID_VCGT) {
		egg_warning ("invalid content of table vcgt, starting with %x", tag_id);
		goto out;
	}

	/* check what type of gamma encoding we have */
	gamma_type = gcm_parser_unencode_32 (data, offset + GCM_VCGT_GAMMA_TYPE);
	if (gamma_type == 0) {
		ret = gcm_parser_load_icc_vcgt_table (profile, data, offset + GCM_VCGT_GAMMA_DATA);
		goto out;
	}
	if (gamma_type == 1) {
		ret = gcm_parser_load_icc_vcgt_formula (profile, data, offset + GCM_VCGT_GAMMA_DATA);
		goto out;
	}

	/* we didn't understand the encoding */
	egg_warning ("gamma type encoding not recognised");
out:
	return ret;
}

/**
 * gcm_profile_load:
 **/
gboolean
gcm_profile_load (GcmProfile *profile, const gchar *filename, GError **error)
{
	gchar *data = NULL;
	gboolean ret;
	gsize length;
	GError *error_local = NULL;
	guint num_tags;
	guint i;
	guint tag_id;
	guint offset;
	guint tag_size;
	guint tag_offset;

	g_return_val_if_fail (GCM_IS_PROFILE (profile), FALSE);
	g_return_val_if_fail (filename != NULL, FALSE);

	egg_debug ("loading %s", filename);

	/* load files */
	ret = g_file_get_contents (filename, &data, &length, &error_local);
	if (!ret) {
		*error = g_error_new (1, 0, "failed to open file: %s", error_local->message);
		g_error_free (error_local);
		goto out;
	}

	/* get the number of tags in the file */
	num_tags = gcm_parser_unencode_32 (data, GCM_NUMTAGS);
	egg_debug ("number of tags: %i", num_tags);

	for (i=0; i<num_tags; i++) {
		offset = GCM_TAG_WIDTH * i;
		tag_id = gcm_parser_unencode_32 (data, GCM_BODY + offset + GCM_TAG_ID);
		tag_offset = gcm_parser_unencode_32 (data, GCM_BODY + offset + GCM_TAG_OFFSET);
		tag_size = gcm_parser_unencode_32 (data, GCM_BODY + offset + GCM_TAG_SIZE);
		egg_debug ("tag %x (%s) is present at %u with size %u", tag_id, data+offset, offset, tag_size);

		if (tag_id == GCM_TAG_ID_DESC) {
			egg_debug ("found DESC: %s", data+tag_offset+12);
			profile->priv->description = g_strdup (data+tag_offset+12);
		}
		if (tag_id == GCM_TAG_ID_TEXT) {
			egg_debug ("found TEXT: %s", data+tag_offset+8);
			profile->priv->copyright = g_strdup (data+tag_offset+8);
		}
		if (tag_id == GCM_TAG_ID_MLUT) {
			egg_debug ("found MLUT which is a fixed size block");
			ret = gcm_parser_load_icc_mlut (profile, data, tag_offset);
			if (!ret) {
				*error = g_error_new (1, 0, "failed to load mlut");
				goto out;
			}
		}
		if (tag_id == GCM_TAG_ID_VCGT) {
			egg_debug ("found VCGT");
			if (tag_size == 1584)
				profile->priv->adobe_gamma_workaround = TRUE;
			ret = gcm_parser_load_icc_vcgt (profile, data, tag_offset);
			if (!ret) {
				*error = g_error_new (1, 0, "failed to load vcgt");
				goto out;
			}
		}
	}
out:
	g_free (data);
	return ret;
}

/**
 * gcm_profile_generate:
 *
 * Free with g_free();
 **/
GcmClutData *
gcm_profile_generate (GcmProfile *profile, guint size)
{
	guint i;
	guint ratio;
	GcmClutData *gamma_data = NULL;
	GcmClutData *ramp_data;
	gfloat gamma_red, min_red, max_red;
	gfloat gamma_green, min_green, max_green;
	gfloat gamma_blue, min_blue, max_blue;
	guint num_entries;

	g_return_val_if_fail (GCM_IS_PROFILE (profile), NULL);
	g_return_val_if_fail (size != 0, FALSE);
	g_return_val_if_fail (profile->priv->data_type != GCM_DATA_TYPE_UNKNOWN, NULL);

	/* create new output array */
	gamma_data = g_new0 (GcmClutData, size);
	ramp_data = profile->priv->ramp_data;

	if (profile->priv->data_type == GCM_DATA_TYPE_MLUT) {

		/* roughly interpolate table */
		ratio = (guint) (256 / (size));
		for (i = 0; i<size; i++) {
			gamma_data[i].red = ramp_data[ratio*i].red;
			gamma_data[i].green = ramp_data[ratio*i].green;
			gamma_data[i].blue = ramp_data[ratio*i].blue;
		}

	} else if (profile->priv->data_type == GCM_DATA_TYPE_VCGT_FORMULA) {

		/* create mapping of desired size */
		for (i = 0; i<size; i++) {
			gamma_red = (gfloat) ramp_data[0].red / 65536.0;
			gamma_green = (gfloat) ramp_data[0].green / 65536.0;
			gamma_blue = (gfloat) ramp_data[0].blue / 65536.0;
			min_red = (gfloat) ramp_data[1].red / 65536.0;
			min_green = (gfloat) ramp_data[1].green / 65536.0;
			min_blue = (gfloat) ramp_data[1].blue / 65536.0;
			max_red = (gfloat) ramp_data[2].red / 65536.0;
			max_green = (gfloat) ramp_data[2].green / 65536.0;
			max_blue = (gfloat) ramp_data[2].blue / 65536.0;

			egg_debug ("Red	 Gamma:%f \tMin:%f \tMax:%f", gamma_red, min_red, max_red);
			egg_debug ("Green  Gamma:%f \tMin:%f \tMax:%f", gamma_green, min_green, max_green);
			egg_debug ("Blue 	 Gamma:%f \tMin:%f \tMax:%f", gamma_blue, min_blue, max_blue);

			gamma_data[i].red = 65536.0 * ((gdouble) pow ((gdouble) i / (gdouble) size, gamma_red) * (max_red - min_red) + min_red);
			gamma_data[i].green = 65536.0 * ((gdouble) pow ((gdouble) i / (gdouble) size, gamma_green) * (max_green - min_green) + min_green);
			gamma_data[i].blue = 65536.0 * ((gdouble) pow ((gdouble) i / (gdouble) size, gamma_blue) * (max_blue - min_blue) + min_blue);
		}
	}

	if (profile->priv->data_type == GCM_DATA_TYPE_VCGT_TABLE) {

		/* simply subsample if the LUT is smaller than the number of entries in the file */
		num_entries = profile->priv->ramp_data_size;
		if (num_entries >= size) {
			ratio = (guint) (num_entries / size);
			for (i=0; i<size; i++) {
				gamma_data[i].red = ramp_data[ratio*i].red;
				gamma_data[i].green = ramp_data[ratio*i].green;
				gamma_data[i].blue = ramp_data[ratio*i].blue;
			}
			goto out;
		}

		/* add extrapolated upper limit to the arrays - handle overflow */
		ratio = (guint) (size / num_entries);
		ramp_data[num_entries].red = (ramp_data[num_entries-1].red + (ramp_data[num_entries-1].red - ramp_data[num_entries-2].red)) & 0xffff;
		if (ramp_data[num_entries].red < 0x4000)
			ramp_data[num_entries].red = 0xffff;

		ramp_data[num_entries].green = (ramp_data[num_entries-1].green + (ramp_data[num_entries-1].green - ramp_data[num_entries-2].green)) & 0xffff;
		if (ramp_data[num_entries].green < 0x4000)
			ramp_data[num_entries].green = 0xffff;

		ramp_data[num_entries].blue = (ramp_data[num_entries-1].blue + (ramp_data[num_entries-1].blue - ramp_data[num_entries-2].blue)) & 0xffff;
		if (ramp_data[num_entries].blue < 0x4000)
			ramp_data[num_entries].blue = 0xffff;
	 
	 	/* interpolate */
		for (i=0; i<num_entries; i++) {
			for (i = 0; i<ratio; i++) {
				gamma_data[i*ratio+i].red = (ramp_data[i].red * (ratio-i) + ramp_data[i+1].red * i) / ratio;
				gamma_data[i*ratio+i].green = (ramp_data[i].green * (ratio-i) + ramp_data[i+1].green * i) / ratio;
				gamma_data[i*ratio+i].blue = (ramp_data[i].blue * (ratio-i) + ramp_data[i+1].blue * i) / ratio;
			}
		}
	}
out:
	return gamma_data;
}

/**
 * gcm_profile_get_property:
 **/
static void
gcm_profile_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GcmProfile *profile = GCM_PROFILE (object);
	GcmProfilePrivate *priv = profile->priv;

	switch (prop_id) {
	case PROP_COPYRIGHT:
		g_value_set_string (value, priv->copyright);
		break;
	case PROP_DESCRIPTION:
		g_value_set_string (value, priv->description);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gcm_profile_set_property:
 **/
static void
gcm_profile_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	GcmProfile *profile = GCM_PROFILE (object);
	GcmProfilePrivate *priv = profile->priv;

	switch (prop_id) {
	case PROP_COPYRIGHT:
		g_free (priv->copyright);
		priv->copyright = g_strdup (g_value_get_string (value));
		break;
	case PROP_DESCRIPTION:
		g_free (priv->description);
		priv->description = g_strdup (g_value_get_string (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gcm_profile_class_init:
 **/
static void
gcm_profile_class_init (GcmProfileClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gcm_profile_finalize;
	object_class->get_property = gcm_profile_get_property;
	object_class->set_property = gcm_profile_set_property;

	/**
	 * GcmProfile:copyright:
	 */
	pspec = g_param_spec_string ("copyright", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_COPYRIGHT, pspec);

	/**
	 * GcmProfile:description:
	 */
	pspec = g_param_spec_string ("description", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_DESCRIPTION, pspec);

	g_type_class_add_private (klass, sizeof (GcmProfilePrivate));
}

/**
 * gcm_profile_init:
 **/
static void
gcm_profile_init (GcmProfile *profile)
{
	profile->priv = GCM_PROFILE_GET_PRIVATE (profile);
	profile->priv->data_type = GCM_DATA_TYPE_UNKNOWN;
	profile->priv->ramp_data = NULL;
	profile->priv->adobe_gamma_workaround = FALSE;
}

/**
 * gcm_profile_finalize:
 **/
static void
gcm_profile_finalize (GObject *object)
{
	GcmProfile *profile = GCM_PROFILE (object);
	GcmProfilePrivate *priv = profile->priv;

	g_free (priv->copyright);
	g_free (priv->ramp_data);

	G_OBJECT_CLASS (gcm_profile_parent_class)->finalize (object);
}

/**
 * gcm_profile_new:
 *
 * Return value: a new GcmProfile object.
 **/
GcmProfile *
gcm_profile_new (void)
{
	GcmProfile *profile;
	profile = g_object_new (GCM_TYPE_PROFILE, NULL);
	return GCM_PROFILE (profile);
}


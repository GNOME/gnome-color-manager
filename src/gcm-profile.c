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

#define GCM_HEADER			0x00
#define GCM_TYPE			0x0c
#define GCM_SIGNATURE			0x24
#define GCM_NUMTAGS			0x80
#define GCM_BODY			0x84

#define GCM_TAG_ID			0x00
#define GCM_TAG_OFFSET			0x04
#define GCM_TAG_SIZE			0x08
#define GCM_TAG_WIDTH			0x0c

#define GCM_TAG_ID_COPYRIGHT			0x63707274
#define GCM_TAG_ID_PROFILE_DESCRIPTION		0x64657363
#define GCM_TAG_ID_VCGT				0x76636774
#define GCM_TAG_ID_MLUT				0x6d4c5554
#define	GCM_TAG_ID_DEVICE_MFG_DESC		0x646d6e64
#define	GCM_TAG_ID_DEVICE_MODEL_DESC		0x646d6464
#define	GCM_TAG_ID_VIEWING_COND_DESC		0x76756564
#define	GCM_TAG_ID_VIEWING_CONDITIONS		0x76696577
#define	GCM_TAG_ID_LUMINANCE			0x6c756d69
#define	GCM_TAG_ID_MEASUREMENT			0x6d656173
#define	GCM_TAG_ID_RED_MATRIX_COLUMN		0x7258595a
#define	GCM_TAG_ID_GREEN_MATRIX_COLUMN		0x6758595a
#define	GCM_TAG_ID_BLUE_MATRIX_COLUMN		0x6258595a
#define	GCM_TAG_ID_RED_TRC			0x72545243
#define	GCM_TAG_ID_GREEN_TRC			0x67545243
#define	GCM_TAG_ID_BLUE_TRC			0x62545243
#define	GCM_TAG_ID_MEDIA_WHITE_POINT		0x77747074
#define	GCM_TAG_ID_MEDIA_BLACK_POINT		0x626b7074
#define	GCM_TAG_ID_TECHNOLOGY			0x74656368
#define	GCM_TAG_ID_CALIBRATION_DATE_TIME	0x63616C74
#define GCM_TRC_TYPE_CURVE			0x63757276
#define GCM_TRC_TYPE_PARAMETRIC_CURVE		0x70617261
#define	GCM_TAG_ID_COLORANT_TABLE		0x636C7274

#define GCM_PROFILE_CLASS_INPUT_DEVICE		0x73636e72
#define GCM_PROFILE_CLASS_DISPLAY_DEVICE	0x6d6e7472
#define GCM_PROFILE_CLASS_OUTPUT_DEVICE		0x70727472
#define GCM_PROFILE_CLASS_DEVICELINK		0x6c696e6b
#define GCM_PROFILE_CLASS_COLORSPACE_CONVERSION	0x73706163
#define GCM_PROFILE_CLASS_ABSTRACT		0x61627374
#define GCM_PROFILE_CLASS_NAMED_COLOUR		0x6e6d636c

#define GCM_TRC_SIZE			0x08
#define GCM_TRC_DATA			0x0c

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
	gboolean			 loaded;
	guint				 profile_type;
	gchar				*description;
	gchar				*copyright;
	gchar				*vendor;
	gboolean			 has_mlut;
	gboolean			 has_vcgt_formula;
	gboolean			 has_vcgt_table;
	gboolean			 has_curve_table;
	gboolean			 has_curve_fixed;
	GcmClutData			*trc_data;
	guint				 trc_data_size;
	GcmClutData			*vcgt_data;
	guint				 vcgt_data_size;
	GcmClutData			*mlut_data;
	guint				 mlut_data_size;
	gboolean			 adobe_gamma_workaround;
};

enum {
	PROP_0,
	PROP_COPYRIGHT,
	PROP_VENDOR,
	PROP_DESCRIPTION,
	PROP_TYPE,
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
 * gcm_prefs_get_tag_description:
 **/
static const gchar *
gcm_prefs_get_tag_description (guint tag)
{
	if (tag == GCM_TAG_ID_PROFILE_DESCRIPTION)
		return "profileDescription";
	if (tag == GCM_TAG_ID_VCGT)
		return "x-vcgt";
	if (tag == GCM_TAG_ID_MLUT)
		return "x-mlut";
	if (tag == GCM_TAG_ID_DEVICE_MFG_DESC)
		return "deviceMfgDesc";
	if (tag == GCM_TAG_ID_DEVICE_MODEL_DESC)
		return "deviceModelDesc";
	if (tag == GCM_TAG_ID_VIEWING_COND_DESC)
		return "viewingCondDesc";
	if (tag == GCM_TAG_ID_VIEWING_CONDITIONS)
		return "viewingConditions";
	if (tag == GCM_TAG_ID_LUMINANCE)
		return "luminance";
	if (tag == GCM_TAG_ID_MEASUREMENT)
		return "measurement";
	if (tag == GCM_TAG_ID_RED_MATRIX_COLUMN)
		return "redMatrixColumn";
	if (tag == GCM_TAG_ID_GREEN_MATRIX_COLUMN)
		return "greenMatrixColumn";
	if (tag == GCM_TAG_ID_BLUE_MATRIX_COLUMN)
		return "blueMatrixColumn";
	if (tag == GCM_TAG_ID_RED_TRC)
		return "redTRC";
	if (tag == GCM_TAG_ID_GREEN_TRC)
		return "greenTRC";
	if (tag == GCM_TAG_ID_BLUE_TRC)
		return "blueTRC";
	if (tag == GCM_TAG_ID_MEDIA_WHITE_POINT)
		return "mediaWhitePoint";
	if (tag == GCM_TAG_ID_MEDIA_BLACK_POINT)
		return "mediaBlackPoint";
	if (tag == GCM_TAG_ID_TECHNOLOGY)
		return "technology";
	if (tag == GCM_TAG_ID_COPYRIGHT)
		return "copyright";
	if (tag == GCM_TAG_ID_CALIBRATION_DATE_TIME)
		return "calibrationDateTime";
	if (tag == GCM_TAG_ID_COLORANT_TABLE)
		return "colorantTable";
	return NULL;
}

/**
 * gcm_parser_load_icc_mlut:
 **/
static gboolean
gcm_parser_load_icc_mlut (GcmProfile *profile, const gchar *data, gsize offset)
{
	gboolean ret = TRUE;
	guint i;
	GcmClutData *mlut_data;

	/* just load in data into a fixed size LUT */
	profile->priv->mlut_data = g_new0 (GcmClutData, 256);
	mlut_data = profile->priv->mlut_data;

	for (i=0; i<256; i++)
		mlut_data[i].red = gcm_parser_unencode_16 (data, offset + GCM_MLUT_RED + i*2);
	for (i=0; i<256; i++)
		mlut_data[i].green = gcm_parser_unencode_16 (data, offset + GCM_MLUT_GREEN + i*2);
	for (i=0; i<256; i++)
		mlut_data[i].blue = gcm_parser_unencode_16 (data, offset + GCM_MLUT_BLUE + i*2);

	/* save datatype */
	profile->priv->has_mlut = TRUE;
	return ret;
}

/**
 * gcm_parser_load_icc_vcgt_formula:
 **/
static gboolean
gcm_parser_load_icc_vcgt_formula (GcmProfile *profile, const gchar *data, gsize offset)
{
	gboolean ret = FALSE;
	GcmClutData *vcgt_data;

	egg_debug ("loading a formula encoded gamma table");

	/* just load in data into a temporary array */
	profile->priv->vcgt_data = g_new0 (GcmClutData, 4);
	vcgt_data = profile->priv->vcgt_data;

	/* read in block of data */
	vcgt_data[0].red = gcm_parser_unencode_32 (data, offset + GCM_VCGT_FORMULA_GAMMA_RED);
	vcgt_data[0].green = gcm_parser_unencode_32 (data, offset + GCM_VCGT_FORMULA_GAMMA_GREEN);
	vcgt_data[0].blue = gcm_parser_unencode_32 (data, offset + GCM_VCGT_FORMULA_GAMMA_BLUE);

	vcgt_data[1].red = gcm_parser_unencode_32 (data, offset + GCM_VCGT_FORMULA_MIN_RED);
	vcgt_data[1].green = gcm_parser_unencode_32 (data, offset + GCM_VCGT_FORMULA_MIN_GREEN);
	vcgt_data[1].blue = gcm_parser_unencode_32 (data, offset + GCM_VCGT_FORMULA_MIN_BLUE);

	vcgt_data[2].red = gcm_parser_unencode_32 (data, offset + GCM_VCGT_FORMULA_MAX_RED);
	vcgt_data[2].green = gcm_parser_unencode_32 (data, offset + GCM_VCGT_FORMULA_MAX_GREEN);
	vcgt_data[2].blue = gcm_parser_unencode_32 (data, offset + GCM_VCGT_FORMULA_MAX_BLUE);

	/* check if valid */
	if (vcgt_data[0].red / 65536.0 > 5.0 || vcgt_data[0].green / 65536.0 > 5.0 || vcgt_data[0].blue / 65536.0 > 5.0) {
		egg_warning ("Gamma values out of range: [R:%u G:%u B:%u]", vcgt_data[0].red, vcgt_data[0].green, vcgt_data[0].blue);
		goto out;
	}
	if (vcgt_data[1].red / 65536.0 >= 1.0 || vcgt_data[1].green / 65536.0 >= 1.0 || vcgt_data[1].blue / 65536.0 >= 1.0) {
		egg_warning ("Gamma min limit out of range: [R:%u G:%u B:%u]", vcgt_data[1].red, vcgt_data[1].green, vcgt_data[1].blue);
		goto out;
	}
	if (vcgt_data[2].red / 65536.0 > 1.0 || vcgt_data[2].green / 65536.0 > 1.0 || vcgt_data[2].blue / 65536.0 > 1.0) {
		egg_warning ("Gamma max limit out of range: [R:%u G:%u B:%u]", vcgt_data[2].red, vcgt_data[2].green, vcgt_data[2].blue);
		goto out;
	}

	/* save datatype */
	profile->priv->has_vcgt_formula = TRUE;
	profile->priv->vcgt_data_size = 3;
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
	GcmClutData *vcgt_data;

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
	profile->priv->vcgt_data = g_new0 (GcmClutData, num_entries+1);
	vcgt_data = profile->priv->vcgt_data;

	if (entry_size == 1) {
		for (i=0; i<num_entries; i++)
			vcgt_data[i].red = gcm_parser_unencode_8 (data, offset + GCM_VCGT_TABLE_NUM_DATA + (num_entries * 0) + i);
		for (i=0; i<num_entries; i++)
			vcgt_data[i].green = gcm_parser_unencode_8 (data, offset + GCM_VCGT_TABLE_NUM_DATA + (num_entries * 1) + i);
		for (i=0; i<num_entries; i++)
			vcgt_data[i].blue = gcm_parser_unencode_8 (data, offset + GCM_VCGT_TABLE_NUM_DATA + (num_entries * 2) + i);
	} else {
		for (i=0; i<num_entries; i++)
			vcgt_data[i].red = gcm_parser_unencode_16 (data, offset + GCM_VCGT_TABLE_NUM_DATA + (num_entries * 0) + (i*2));
		for (i=0; i<num_entries; i++)
			vcgt_data[i].green = gcm_parser_unencode_16 (data, offset + GCM_VCGT_TABLE_NUM_DATA + (num_entries * 2) + (i*2));
		for (i=0; i<num_entries; i++)
			vcgt_data[i].blue = gcm_parser_unencode_16 (data, offset + GCM_VCGT_TABLE_NUM_DATA + (num_entries * 4) + (i*2));
	}

	/* save datatype */
	profile->priv->has_vcgt_table = TRUE;
	profile->priv->vcgt_data_size = num_entries;
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
 * gcm_parser_load_icc_trc_curve:
 **/
static gboolean
gcm_parser_load_icc_trc_curve (GcmProfile *profile, const gchar *data, gsize offset, guint color)
{
	gboolean ret = TRUE;
	guint num_entries;
	guint i = 0;
	guint value;
	GcmClutData *trc_data;

	num_entries = gcm_parser_unencode_32 (data, offset+GCM_TRC_SIZE);

	/* create ramp */
	if (profile->priv->trc_data == NULL) {
		profile->priv->trc_data = g_new0 (GcmClutData, num_entries+1);
		profile->priv->trc_data_size = num_entries;
		egg_debug ("creating array of size %i", num_entries);
	}

	/* load data */
	trc_data = profile->priv->trc_data;

	/* save datatype */
	egg_debug ("curve size %i", num_entries);
	profile->priv->trc_data_size = num_entries;
	if (num_entries > 1) {

		/* load in data */
		for (i=0; i<num_entries; i++) {
			value = gcm_parser_unencode_16 (data, offset+GCM_TRC_DATA+(i*2));
			if (color == 0)
				trc_data[i].red = value;
			if (color == 1)
				trc_data[i].green = value;
			if (color == 2)
				trc_data[i].blue = value;
		}

		/* save datatype */
		profile->priv->has_curve_table = TRUE;
	} else {
		value = gcm_parser_unencode_8 (data, offset+GCM_TRC_DATA);
		if (color == 0)
			trc_data[i].red = value;
		if (color == 1)
			trc_data[i].green = value;
		if (color == 2)
			trc_data[i].blue = value;

		/* save datatype */
		profile->priv->has_curve_fixed = TRUE;
	}
	return ret;
}

/**
 * gcm_parser_load_icc_trc:
 **/
static gboolean
gcm_parser_load_icc_trc (GcmProfile *profile, const gchar *data, gsize offset, guint color)
{
	gboolean ret = FALSE;
	guint type;
	type = gcm_parser_unencode_32 (data, offset);

	if (type == GCM_TRC_TYPE_CURVE) {
		ret = gcm_parser_load_icc_trc_curve (profile, data, offset, color);
	} else if (type == GCM_TRC_TYPE_PARAMETRIC_CURVE) {
//		ret = gcm_parser_load_icc_trc_parametric_curve (profile, data, offset, color);
		egg_warning ("contains a parametric curve, FIXME");
		ret = TRUE;
	}
	return ret;
}

/**
 * gcm_profile_parse_data:
 **/
gboolean
gcm_profile_parse_data (GcmProfile *profile, const gchar *data, gsize length, GError **error)
{
	gboolean ret = FALSE;
	guint num_tags;
	guint i;
	guint tag_id;
	guint offset;
	guint tag_size;
	guint tag_offset;
	gchar *signature;
	guint32 profile_type;

	g_return_val_if_fail (GCM_IS_PROFILE (profile), FALSE);
	g_return_val_if_fail (data != NULL, FALSE);
	g_return_val_if_fail (profile->priv->loaded == FALSE, FALSE);

	profile->priv->loaded = TRUE;

	/* ensure we have the header */
	if (length < 0x84) {
		if (error != NULL)
			*error = g_error_new (1, 0, "profile was not valid (file size too small)");
		goto out;
	}

	/* ensure this is a icc file */
	if (data[GCM_SIGNATURE+0] != 'a' ||
	    data[GCM_SIGNATURE+1] != 'c' ||
	    data[GCM_SIGNATURE+2] != 's' ||
	    data[GCM_SIGNATURE+3] != 'p') {

		/* copy the 4 bytes of the invalid signature, with a '\0' byte */
		signature = g_new0 (gchar, 5);
		for (i=0; i<5; i++)
			signature[i] = data[GCM_SIGNATURE+i];
		if (error != NULL)
			*error = g_error_new (1, 0, "not an ICC profile, signature is '%s', expecting 'acsp'", signature);
		g_free (signature);
		goto out;
	}

	/* get the profile type */
	profile_type = gcm_parser_unencode_32 (data, GCM_TYPE);
	switch (profile_type) {
	case GCM_PROFILE_CLASS_INPUT_DEVICE:
		profile->priv->profile_type = GCM_PROFILE_TYPE_INPUT_DEVICE;
		break;
	case GCM_PROFILE_CLASS_DISPLAY_DEVICE:
		profile->priv->profile_type = GCM_PROFILE_TYPE_DISPLAY_DEVICE;
		break;
	case GCM_PROFILE_CLASS_OUTPUT_DEVICE:
		profile->priv->profile_type = GCM_PROFILE_TYPE_OUTPUT_DEVICE;
		break;
	case GCM_PROFILE_CLASS_DEVICELINK:
		profile->priv->profile_type = GCM_PROFILE_TYPE_DEVICELINK;
		break;
	case GCM_PROFILE_CLASS_COLORSPACE_CONVERSION:
		profile->priv->profile_type = GCM_PROFILE_TYPE_COLORSPACE_CONVERSION;
		break;
	case GCM_PROFILE_CLASS_ABSTRACT:
		profile->priv->profile_type = GCM_PROFILE_TYPE_ABSTRACT;
		break;
	case GCM_PROFILE_CLASS_NAMED_COLOUR:
		profile->priv->profile_type = GCM_PROFILE_TYPE_NAMED_COLOUR;
		break;
	default:
		profile->priv->profile_type = GCM_PROFILE_TYPE_UNKNOWN;
	}

	/* get the number of tags in the file */
	num_tags = gcm_parser_unencode_32 (data, GCM_NUMTAGS);
	egg_debug ("number of tags: %i", num_tags);

	for (i=0; i<num_tags; i++) {
		offset = GCM_TAG_WIDTH * i;
		tag_id = gcm_parser_unencode_32 (data, GCM_BODY + offset + GCM_TAG_ID);
		tag_offset = gcm_parser_unencode_32 (data, GCM_BODY + offset + GCM_TAG_OFFSET);
		tag_size = gcm_parser_unencode_32 (data, GCM_BODY + offset + GCM_TAG_SIZE);
		egg_debug ("tag %x (%s) is present at %u with size %u", tag_id, gcm_prefs_get_tag_description (tag_id), offset, tag_size);

		if (tag_id == GCM_TAG_ID_PROFILE_DESCRIPTION) {
			egg_debug ("found DESC: %s", data+tag_offset+12);
			profile->priv->description = g_strdup (data+tag_offset+12);
		}
		if (tag_id == GCM_TAG_ID_COPYRIGHT) {
			egg_debug ("found COPYRIGHT: %s", data+tag_offset+8);
			profile->priv->copyright = g_strdup (data+tag_offset+8);
		}
		if (tag_id == GCM_TAG_ID_DEVICE_MFG_DESC) {
			egg_debug ("found VENDOR: %s", data+tag_offset+12);
			profile->priv->vendor = g_strdup (data+tag_offset+12);
		}
		if (tag_id == GCM_TAG_ID_DEVICE_MODEL_DESC) {
			egg_debug ("found MODEL: %s", data+tag_offset+12);
//			profile->priv->model = g_strdup (data+tag_offset+12);
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
		if (tag_id == GCM_TAG_ID_RED_TRC) {
			egg_debug ("found TRC (red)");
			ret = gcm_parser_load_icc_trc (profile, data, tag_offset, 0);
			if (!ret) {
				*error = g_error_new (1, 0, "failed to load trc");
				goto out;
			}
		}
		if (tag_id == GCM_TAG_ID_GREEN_TRC) {
			egg_debug ("found TRC (green)");
			ret = gcm_parser_load_icc_trc (profile, data, tag_offset, 1);
			if (!ret) {
				*error = g_error_new (1, 0, "failed to load trc");
				goto out;
			}
		}
		if (tag_id == GCM_TAG_ID_BLUE_TRC) {
			egg_debug ("found TRC (blue)");
			ret = gcm_parser_load_icc_trc (profile, data, tag_offset, 2);
			if (!ret) {
				*error = g_error_new (1, 0, "failed to load trc");
				goto out;
			}
		}
	}

	/* success */
	ret = TRUE;

	egg_debug ("Has MLUT:         %s", profile->priv->has_mlut ? "YES" : "NO");
	egg_debug ("Has VCGT formula: %s", profile->priv->has_vcgt_formula ? "YES" : "NO");
	egg_debug ("Has VCGT table:   %s", profile->priv->has_vcgt_table ? "YES" : "NO");
	egg_debug ("Has curve table:  %s", profile->priv->has_curve_table ? "YES" : "NO");
	egg_debug ("Has fixed gamma:  %s", profile->priv->has_curve_fixed ? "YES" : "NO");
out:
	return ret;
}

/**
 * gcm_profile_parse:
 **/
gboolean
gcm_profile_parse (GcmProfile *profile, const gchar *filename, GError **error)
{
	gchar *data = NULL;
	gboolean ret;
	gsize length;
	GError *error_local = NULL;

	g_return_val_if_fail (GCM_IS_PROFILE (profile), FALSE);
	g_return_val_if_fail (filename != NULL, FALSE);
	g_return_val_if_fail (profile->priv->loaded == FALSE, FALSE);

	egg_debug ("loading '%s'", filename);

	/* load files */
	ret = g_file_get_contents (filename, &data, &length, &error_local);
	if (!ret) {
		if (error != NULL)
			*error = g_error_new (1, 0, "failed to load profile: %s", error_local->message);
		g_error_free (error_local);
		goto out;
	}

	/* parse the data */
	ret = gcm_profile_parse_data (profile, data, length, error);
	if (!ret)
		goto out;
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
	guint i, j;
	guint ratio;
	GcmClutData *gamma_data = NULL;
	GcmClutData *vcgt_data;
	GcmClutData *mlut_data;
	GcmClutData *trc_data;
	gfloat gamma_red, min_red, max_red;
	gfloat gamma_green, min_green, max_green;
	gfloat gamma_blue, min_blue, max_blue;
	guint num_entries;

	g_return_val_if_fail (GCM_IS_PROFILE (profile), NULL);
	g_return_val_if_fail (size != 0, FALSE);

	/* reduce dereferences */
	vcgt_data = profile->priv->vcgt_data;
	mlut_data = profile->priv->mlut_data;
	trc_data = profile->priv->trc_data;

	if (profile->priv->has_mlut) {

		/* create new output array */
		gamma_data = g_new0 (GcmClutData, size);

		/* roughly interpolate table */
		ratio = (guint) (256 / (size));
		for (i = 0; i<size; i++) {
			gamma_data[i].red = mlut_data[ratio*i].red;
			gamma_data[i].green = mlut_data[ratio*i].green;
			gamma_data[i].blue = mlut_data[ratio*i].blue;
		}
		goto out;
	}

	if (profile->priv->has_vcgt_formula) {

		/* create new output array */
		gamma_data = g_new0 (GcmClutData, size);

		/* create mapping of desired size */
		for (i = 0; i<size; i++) {
			gamma_red = (gfloat) vcgt_data[0].red / 65536.0;
			gamma_green = (gfloat) vcgt_data[0].green / 65536.0;
			gamma_blue = (gfloat) vcgt_data[0].blue / 65536.0;
			min_red = (gfloat) vcgt_data[1].red / 65536.0;
			min_green = (gfloat) vcgt_data[1].green / 65536.0;
			min_blue = (gfloat) vcgt_data[1].blue / 65536.0;
			max_red = (gfloat) vcgt_data[2].red / 65536.0;
			max_green = (gfloat) vcgt_data[2].green / 65536.0;
			max_blue = (gfloat) vcgt_data[2].blue / 65536.0;

			gamma_data[i].red = 65536.0 * ((gdouble) pow ((gdouble) i / (gdouble) size, gamma_red) * (max_red - min_red) + min_red);
			gamma_data[i].green = 65536.0 * ((gdouble) pow ((gdouble) i / (gdouble) size, gamma_green) * (max_green - min_green) + min_green);
			gamma_data[i].blue = 65536.0 * ((gdouble) pow ((gdouble) i / (gdouble) size, gamma_blue) * (max_blue - min_blue) + min_blue);
		}
	}

	if (profile->priv->has_vcgt_table) {

		/* create new output array */
		gamma_data = g_new0 (GcmClutData, size);

		/* simply subsample if the LUT is smaller than the number of entries in the file */
		num_entries = profile->priv->vcgt_data_size;
		if (num_entries >= size) {
			ratio = (guint) (num_entries / size);
			for (i=0; i<size; i++) {
				gamma_data[i].red = vcgt_data[ratio*i].red;
				gamma_data[i].green = vcgt_data[ratio*i].green;
				gamma_data[i].blue = vcgt_data[ratio*i].blue;
			}
			goto out;
		}

		/* add extrapolated upper limit to the arrays - handle overflow */
		ratio = (guint) (size / num_entries);
		vcgt_data[num_entries].red = (vcgt_data[num_entries-1].red + (vcgt_data[num_entries-1].red - vcgt_data[num_entries-2].red)) & 0xffff;
		if (vcgt_data[num_entries].red < 0x4000)
			vcgt_data[num_entries].red = 0xffff;

		vcgt_data[num_entries].green = (vcgt_data[num_entries-1].green + (vcgt_data[num_entries-1].green - vcgt_data[num_entries-2].green)) & 0xffff;
		if (vcgt_data[num_entries].green < 0x4000)
			vcgt_data[num_entries].green = 0xffff;

		vcgt_data[num_entries].blue = (vcgt_data[num_entries-1].blue + (vcgt_data[num_entries-1].blue - vcgt_data[num_entries-2].blue)) & 0xffff;
		if (vcgt_data[num_entries].blue < 0x4000)
			vcgt_data[num_entries].blue = 0xffff;

		/* interpolate */
		for (i=0; i<num_entries; i++) {
			for (j = 0; j<ratio; j++) {
				gamma_data[i*ratio+j].red = (vcgt_data[i].red * (ratio-j) + vcgt_data[i+1].red * j) / ratio;
				gamma_data[i*ratio+j].green = (vcgt_data[i].green * (ratio-j) + vcgt_data[i+1].green * j) / ratio;
				gamma_data[i*ratio+j].blue = (vcgt_data[i].blue * (ratio-j) + vcgt_data[i+1].blue * j) / ratio;
			}
		}
		goto out;
	}

	if (profile->priv->has_curve_table) {

		gfloat inverse_ratio;
		guint idx;
		gfloat frac;

		/* create new output array */
		gamma_data = g_new0 (GcmClutData, size);

		/* simply subsample if the LUT is smaller than the number of entries in the file */
		num_entries = profile->priv->trc_data_size;
		if (num_entries >= size) {
			ratio = (guint) (num_entries / size);
			for (i=0; i<size; i++) {
				gamma_data[i].red = trc_data[ratio*i].red;
				gamma_data[i].green = trc_data[ratio*i].green;
				gamma_data[i].blue = trc_data[ratio*i].blue;
			}
			goto out;
		}

		/* LUT is bigger than number of entries, so interpolate */
		inverse_ratio = (gfloat) num_entries / size;
		trc_data[num_entries].red = 0xffff;
		trc_data[num_entries].green = 0xffff;
		trc_data[num_entries].blue = 0xffff;

		/* interpolate */
		for (i=0; i<size; i++) {
			idx = floor(i*inverse_ratio);
			frac = (i*inverse_ratio) - idx;
			gamma_data[i].red = trc_data[idx].red * (1.0f-frac) + trc_data[idx+1].red * frac;
			gamma_data[i].green = trc_data[idx].green * (1.0f-frac) + trc_data[idx+1].green * frac;
			gamma_data[i].blue = trc_data[idx].blue * (1.0f-frac) + trc_data[idx+1].blue * frac;
		}
		goto out;
	}

	if (profile->priv->has_curve_fixed) {

		/* create new output array */
		gamma_data = g_new0 (GcmClutData, size);

		/* use a simple y=x^gamma formula */
		for (i=0; i<size; i++) {
			gfloat part;
			part = (gfloat) i / (gfloat) size;
			gamma_data[i].red = pow (part, (gfloat) trc_data[0].red / 256.0f) * 65000.f;
			gamma_data[i].green = pow (part, (gfloat) trc_data[0].green / 256.0f) * 65000.f;
			gamma_data[i].blue = pow (part, (gfloat) trc_data[0].blue / 256.0f) * 65000.f;
		}
		goto out;
	}

	/* bugger */
	egg_warning ("no LUT to generate");
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
	case PROP_VENDOR:
		g_value_set_string (value, priv->vendor);
		break;
	case PROP_DESCRIPTION:
		g_value_set_string (value, priv->description);
		break;
	case PROP_TYPE:
		g_value_set_uint (value, priv->profile_type);
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
	switch (prop_id) {
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
				     G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_COPYRIGHT, pspec);

	/**
	 * GcmProfile:vendor:
	 */
	pspec = g_param_spec_string ("vendor", NULL, NULL,
				     NULL,
				     G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_VENDOR, pspec);

	/**
	 * GcmProfile:description:
	 */
	pspec = g_param_spec_string ("description", NULL, NULL,
				     NULL,
				     G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_DESCRIPTION, pspec);

	/**
	 * GcmProfile:type:
	 */
	pspec = g_param_spec_uint ("type", NULL, NULL,
				   0, G_MAXUINT, 0,
				   G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_TYPE, pspec);

	g_type_class_add_private (klass, sizeof (GcmProfilePrivate));
}

/**
 * gcm_profile_init:
 **/
static void
gcm_profile_init (GcmProfile *profile)
{
	profile->priv = GCM_PROFILE_GET_PRIVATE (profile);
	profile->priv->vcgt_data = NULL;
	profile->priv->mlut_data = NULL;
	profile->priv->trc_data = NULL;
	profile->priv->adobe_gamma_workaround = FALSE;
	profile->priv->profile_type = GCM_PROFILE_TYPE_UNKNOWN;
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
	g_free (priv->vendor);
	g_free (priv->vcgt_data);
	g_free (priv->mlut_data);
	g_free (priv->trc_data);

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


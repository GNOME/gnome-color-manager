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
#include <glib/gi18n.h>
#include <math.h>
#include <lcms.h>

#include "egg-debug.h"

#include "gcm-profile.h"
#include "gcm-utils.h"
#include "gcm-xyz.h"

static void     gcm_profile_finalize	(GObject     *object);

#define GCM_PROFILE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GCM_TYPE_PROFILE, GcmProfilePrivate))

#define GCM_HEADER			0x00
#define GCM_TYPE			0x0c
#define GCM_COLORSPACE			0x10
#define GCM_CREATION_DATE_TIME		0x18
#define GCM_SIGNATURE			0x24
#define GCM_NUMTAGS			0x80
#define GCM_BODY			0x84

#define GCM_TAG_ID			0x00
#define GCM_TAG_OFFSET			0x04
#define GCM_TAG_SIZE			0x08
#define GCM_TAG_WIDTH			0x0c

#define icSigVideoCartGammaTableTag		0x76636774
#define icSigMachineLookUpTableTag		0x6d4c5554
#define	GCM_TAG_ID_COLORANT_TABLE		0x636c7274
#define GCM_TRC_TYPE_PARAMETRIC_CURVE		0x70617261

#define GCM_TRC_SIZE			0x08
#define GCM_TRC_DATA			0x0c

#define GCM_MLUT_RED			0x000
#define GCM_MLUT_GREEN			0x200
#define GCM_MLUT_BLUE			0x400

#define GCM_DESC_RECORD_SIZE		0x08
#define GCM_DESC_RECORD_TEXT		0x0c
#define GCM_TEXT_RECORD_TEXT		0x08

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
	guint				 colorspace;
	guint				 size;
	gchar				*description;
	gchar				*filename;
	gchar				*copyright;
	gchar				*manufacturer;
	gchar				*model;
	gchar				*datetime;
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
	GcmXyz				*white_point;
	GcmXyz				*black_point;
	GcmXyz				*luminance_red;
	GcmXyz				*luminance_green;
	GcmXyz				*luminance_blue;
};

enum {
	PROP_0,
	PROP_COPYRIGHT,
	PROP_MANUFACTURER,
	PROP_MODEL,
	PROP_DATETIME,
	PROP_DESCRIPTION,
	PROP_FILENAME,
	PROP_TYPE,
	PROP_COLORSPACE,
	PROP_SIZE,
	PROP_WHITE_POINT,
	PROP_LUMINANCE_RED,
	PROP_LUMINANCE_GREEN,
	PROP_LUMINANCE_BLUE,
	PROP_LAST
};

G_DEFINE_TYPE (GcmProfile, gcm_profile, G_TYPE_OBJECT)

/**
 * gcm_parser_decode_32:
 **/
static guint
gcm_parser_decode_32 (const guint8 *data)
{
	guint retval;
	retval = (*(data + 0) << 0) + (*(data + 1) << 8) + (*(data + 2) << 16) + (*(data + 3) << 24);
	return GUINT32_FROM_BE (retval);
}

/**
 * gcm_parser_decode_16:
 **/
static guint
gcm_parser_decode_16 (const guint8 *data)
{
	guint retval;
	retval = (*(data + 0) << 0) + (*(data + 1) << 8);
	return GUINT16_FROM_BE (retval);
}

/**
 * gcm_parser_decode_8:
 **/
static guint
gcm_parser_decode_8 (const guint8 *data)
{
	guint retval;
	retval = (*data << 0);
	return GUINT16_FROM_BE (retval);
}

/**
 * gcm_prefs_get_tag_description:
 **/
static const gchar *
gcm_prefs_get_tag_description (guint tag)
{
	if (tag == icSigProfileDescriptionTag)
		return "profileDescription";
	if (tag == icSigVideoCartGammaTableTag)
		return "videoCartGammaTable";
	if (tag == icSigMachineLookUpTableTag)
		return "massiveLookUpTable";
	if (tag == icSigDeviceMfgDescTag)
		return "deviceMfgDesc";
	if (tag == icSigDeviceModelDescTag)
		return "deviceModelDesc";
	if (tag == icSigViewingCondDescTag)
		return "viewingCondDesc";
	if (tag == icSigViewingConditionsTag)
		return "viewingConditions";
	if (tag == icSigLuminanceTag)
		return "luminance";
	if (tag == icSigMeasurementTag)
		return "measurement";
	if (tag == icSigRedColorantTag)
		return "redMatrixColumn";
	if (tag == icSigGreenColorantTag)
		return "greenMatrixColumn";
	if (tag == icSigBlueColorantTag)
		return "blueMatrixColumn";
	if (tag == icSigRedTRCTag)
		return "redTRC";
	if (tag == icSigGreenTRCTag)
		return "greenTRC";
	if (tag == icSigBlueTRCTag)
		return "blueTRC";
	if (tag == icSigMediaWhitePointTag)
		return "mediaWhitePoint";
	if (tag == icSigMediaBlackPointTag)
		return "mediaBlackPoint";
	if (tag == icSigTechnologyTag)
		return "technology";
	if (tag == icSigCopyrightTag)
		return "copyright";
	if (tag == icSigCalibrationDateTimeTag)
		return "calibrationDateTime";
	if (tag == GCM_TAG_ID_COLORANT_TABLE)
		return "colorantTable";
	if (tag == icSigBToA0Tag)
		return "BToA0";
	if (tag == icSigBToA1Tag)
		return "BToA1";
	if (tag == icSigBToA2Tag)
		return "BToA2";
	if (tag == icSigAToB0Tag)
		return "AToB0";
	if (tag == icSigAToB1Tag)
		return "AToB1";
	if (tag == icSigAToB2Tag)
		return "AToB2";
	return NULL;
}

/**
 * gcm_parser_load_icc_mlut:
 **/
static gboolean
gcm_parser_load_icc_mlut (GcmProfile *profile, const guint8 *data, guint size)
{
	gboolean ret = TRUE;
	guint i;
	GcmClutData *mlut_data;

	/* just load in data into a fixed size LUT */
	profile->priv->mlut_data = g_new0 (GcmClutData, 256);
	mlut_data = profile->priv->mlut_data;

	for (i=0; i<256; i++)
		mlut_data[i].red = gcm_parser_decode_16 (data + GCM_MLUT_RED + i*2);
	for (i=0; i<256; i++)
		mlut_data[i].green = gcm_parser_decode_16 (data + GCM_MLUT_GREEN + i*2);
	for (i=0; i<256; i++)
		mlut_data[i].blue = gcm_parser_decode_16 (data + GCM_MLUT_BLUE + i*2);

	/* save datatype */
	profile->priv->has_mlut = TRUE;
	return ret;
}

/**
 * gcm_parser_load_icc_vcgt_formula:
 **/
static gboolean
gcm_parser_load_icc_vcgt_formula (GcmProfile *profile, const guint8 *data, guint size)
{
	gboolean ret = FALSE;
	GcmClutData *vcgt_data;

	egg_debug ("loading a formula encoded gamma table");

	/* just load in data into a temporary array */
	profile->priv->vcgt_data = g_new0 (GcmClutData, 4);
	vcgt_data = profile->priv->vcgt_data;

	/* read in block of data */
	vcgt_data[0].red = gcm_parser_decode_32 (data + GCM_VCGT_FORMULA_GAMMA_RED);
	vcgt_data[0].green = gcm_parser_decode_32 (data + GCM_VCGT_FORMULA_GAMMA_GREEN);
	vcgt_data[0].blue = gcm_parser_decode_32 (data + GCM_VCGT_FORMULA_GAMMA_BLUE);

	vcgt_data[1].red = gcm_parser_decode_32 (data + GCM_VCGT_FORMULA_MIN_RED);
	vcgt_data[1].green = gcm_parser_decode_32 (data + GCM_VCGT_FORMULA_MIN_GREEN);
	vcgt_data[1].blue = gcm_parser_decode_32 (data + GCM_VCGT_FORMULA_MIN_BLUE);

	vcgt_data[2].red = gcm_parser_decode_32 (data + GCM_VCGT_FORMULA_MAX_RED);
	vcgt_data[2].green = gcm_parser_decode_32 (data + GCM_VCGT_FORMULA_MAX_GREEN);
	vcgt_data[2].blue = gcm_parser_decode_32 (data + GCM_VCGT_FORMULA_MAX_BLUE);

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
gcm_parser_load_icc_vcgt_table (GcmProfile *profile, const guint8 *data, guint size)
{
	gboolean ret = TRUE;
	guint num_channels = 0;
	guint num_entries = 0;
	guint entry_size = 0;
	guint i;
	GcmClutData *vcgt_data;

	egg_debug ("loading a table encoded gamma table");

	num_channels = gcm_parser_decode_16 (data + GCM_VCGT_TABLE_NUM_CHANNELS);
	num_entries = gcm_parser_decode_16 (data + GCM_VCGT_TABLE_NUM_ENTRIES);
	entry_size = gcm_parser_decode_16 (data + GCM_VCGT_TABLE_NUM_SIZE);

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
		ret = FALSE;
		goto out;
	}

	/* bigger than will fit in 16 bits? */
	if (entry_size > 2) {
		egg_warning ("cannot parse large entries");
		ret = FALSE;
		goto out;
	}

	/* allocate ramp, plus one entry for extrapolation */
	profile->priv->vcgt_data = g_new0 (GcmClutData, num_entries + 1);
	vcgt_data = profile->priv->vcgt_data;

	if (entry_size == 1) {
		for (i=0; i<num_entries; i++)
			vcgt_data[i].red = gcm_parser_decode_8 (data + GCM_VCGT_TABLE_NUM_DATA + (num_entries * 0) + i);
		for (i=0; i<num_entries; i++)
			vcgt_data[i].green = gcm_parser_decode_8 (data + GCM_VCGT_TABLE_NUM_DATA + (num_entries * 1) + i);
		for (i=0; i<num_entries; i++)
			vcgt_data[i].blue = gcm_parser_decode_8 (data + GCM_VCGT_TABLE_NUM_DATA + (num_entries * 2) + i);
	} else {
		for (i=0; i<num_entries; i++)
			vcgt_data[i].red = gcm_parser_decode_16 (data + GCM_VCGT_TABLE_NUM_DATA + (num_entries * 0) + (i*2));
		for (i=0; i<num_entries; i++)
			vcgt_data[i].green = gcm_parser_decode_16 (data + GCM_VCGT_TABLE_NUM_DATA + (num_entries * 2) + (i*2));
		for (i=0; i<num_entries; i++)
			vcgt_data[i].blue = gcm_parser_decode_16 (data + GCM_VCGT_TABLE_NUM_DATA + (num_entries * 4) + (i*2));
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
gcm_parser_load_icc_vcgt (GcmProfile *profile, const guint8 *data, guint size)
{
	gboolean ret = FALSE;
	guint tag_id;
	guint gamma_type;

	/* check we have a VCGT block */
	tag_id = gcm_parser_decode_32 (data);
	if (tag_id != icSigVideoCartGammaTableTag) {
		egg_warning ("invalid content of table vcgt, starting with %x", tag_id);
		goto out;
	}

	/* check what type of gamma encoding we have */
	gamma_type = gcm_parser_decode_32 (data + GCM_VCGT_GAMMA_TYPE);
	if (gamma_type == 0) {
		ret = gcm_parser_load_icc_vcgt_table (profile, data + GCM_VCGT_GAMMA_DATA, size);
		goto out;
	}
	if (gamma_type == 1) {
		ret = gcm_parser_load_icc_vcgt_formula (profile, data + GCM_VCGT_GAMMA_DATA, size);
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
gcm_parser_load_icc_trc_curve (GcmProfile *profile, const guint8 *data, guint size, guint color)
{
	gboolean ret = TRUE;
	guint num_entries;
	guint i = 0;
	guint value;
	GcmClutData *trc_data;

	num_entries = gcm_parser_decode_32 (data + GCM_TRC_SIZE);

	/* create ramp */
	if (profile->priv->trc_data == NULL) {
		profile->priv->trc_data = g_new0 (GcmClutData, num_entries + 1);
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
			value = gcm_parser_decode_16 (data + GCM_TRC_DATA + (i*2));
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
		value = gcm_parser_decode_8 (data + GCM_TRC_DATA);
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
gcm_parser_load_icc_trc (GcmProfile *profile, const guint8 *data, guint size, guint color)
{
	gboolean ret = FALSE;
	guint type;
	type = gcm_parser_decode_32 (data);

	if (type == icSigCurveType) {
		ret = gcm_parser_load_icc_trc_curve (profile, data, size, color);
	} else if (type == GCM_TRC_TYPE_PARAMETRIC_CURVE) {
//		ret = gcm_parser_load_icc_trc_parametric_curve (profile, data, offset, color);
		egg_warning ("contains a parametric curve, FIXME");
		ret = TRUE;
	}
	return ret;
}

/**
 * gcm_profile_ensure_printable:
 **/
static void
gcm_profile_ensure_printable (gchar *data)
{
	guint i;
	guint idx = 0;

	g_return_if_fail (data != NULL);

	for (i=0; data[i] != '\0'; i++) {
		if (g_ascii_isalnum (data[i]) ||
		    g_ascii_ispunct (data[i]) ||
		    data[i] == ' ')
			data[idx++] = data[i];
	}
	data[idx] = '\0';

	/* broken profiles have _ instead of spaces */
	g_strdelimit (data, "_", ' ');
}

/**
 * gcm_profile_utf16be_to_locale:
 *
 * Convert ICC encoded UTF-16BE into a string the user can understand
 **/
static gchar *
gcm_profile_utf16be_to_locale (const guint8 *text, guint size)
{
	gsize items_written;
	gchar *text_utf8 = NULL;
	gchar *text_locale = NULL;
	GError *error = NULL;

	/* convert from ICC text encoding to UTF-8 */
	text_utf8 = g_convert ((const gchar*)text, size, "UTF-8", "UTF-16BE", NULL, &items_written, &error);
	if (text_utf8 == NULL) {
		egg_warning ("failed to convert to UTF-8: %s", error->message);
		g_error_free (error);
		goto out;
	}

	/* convert from UTF-8 to the users locale*/
	text_locale = g_locale_from_utf8 (text_utf8, items_written, NULL, NULL, &error);
	if (text_locale == NULL) {
		egg_warning ("failed to convert to locale: %s", error->message);
		g_error_free (error);
		goto out;
	}
out:
	g_free (text_utf8);
	return text_locale;
}

/**
 * gcm_profile_parse_multi_localized_unicode:
 **/
static gchar *
gcm_profile_parse_multi_localized_unicode (GcmProfile *profile, const guint8 *data, guint size)
{
	guint i;
	gchar *text = NULL;
	guint record_size;
	guint names_size;
	guint len;
	guint offset_name;
	guint32 type;

	/* get type */
	type = gcm_parser_decode_32 (data);

	/* check we are not a localized tag */
	if (type == icSigTextDescriptionType) {
		record_size = gcm_parser_decode_32 (data + GCM_DESC_RECORD_SIZE);
		text = g_strndup ((const gchar*)&data[GCM_DESC_RECORD_TEXT], record_size);
		goto out;
	}

	/* check we are not a localized tag */
	if (type == icSigTextType) {
		text = g_strdup ((const gchar*)&data[GCM_TEXT_RECORD_TEXT]);
		goto out;
	}

	/* check we are not a localized tag */
	if (type == icSigMultiLocalizedUnicodeType) {
		names_size = gcm_parser_decode_32 (data + 8);
		if (names_size != 1) {
			/* there is more than one language encoded */
			egg_warning ("more than one item of data in MLUC (names size: %i), using first one", names_size);
		}
		record_size = gcm_parser_decode_32 (data + 12);
		len = gcm_parser_decode_32 (data + 20);
		offset_name = gcm_parser_decode_32 (data + 24);
		text = gcm_profile_utf16be_to_locale (data + offset_name, len);
		goto out;
	}

	/* an unrecognised tag */
	for (i=0x0; i<0x1c; i++) {
		egg_warning ("unrecognised text tag");
		if (data[i] >= 'A' && data[i] <= 'z')
			egg_debug ("%i\t%c (%i)", i, data[i], data[i]);
		else
			egg_debug ("%i\t  (%i)", i, data[i]);
	}
out:
	return text;
}

/**
 * gcm_parser_s15_fixed_16_number_to_float:
 **/
static gfloat
gcm_parser_s15_fixed_16_number_to_float (gint32 data)
{
	gfloat retval;

	/* s15Fixed16Number has 16 fractional bits */
	retval = (gfloat) data / 0x10000;
	return retval;
}

/**
 * gcm_parser_load_icc_xyz_type:
 **/
static gboolean
gcm_parser_load_icc_xyz_type (GcmProfile *profile, const guint8 *data, guint size, GcmXyz *xyz)
{
	gboolean ret;
	guint value;
	gfloat x;
	gfloat y;
	gfloat z;
	gchar *type;

	/* check we are not a localized tag */
	ret = (memcmp (data, "XYZ ", 4) == 0);
	if (!ret) {
		type = g_strndup ((const gchar*)data, 4);
		egg_warning ("not an XYZ type: '%s'", type);
		g_free (type);
		goto out;
	}

	/* just get the first entry in each matrix */
	value = gcm_parser_decode_32 (data + 8 + 0);
	x = gcm_parser_s15_fixed_16_number_to_float (value);
	value = gcm_parser_decode_32 (data + 8 + 4);
	y = gcm_parser_s15_fixed_16_number_to_float (value);
	value = gcm_parser_decode_32 (data + 8 + 8);
	z = gcm_parser_s15_fixed_16_number_to_float (value);

	/* set data */
	g_object_set (xyz,
		      "cie-x", x,
		      "cie-y", y,
		      "cie-z", z,
		      NULL);
out:
	return ret;
}

/**
 * gcm_parser_get_date_time:
 **/
static gchar *
gcm_parser_get_date_time (const guint8 *data)
{
	guint years;	/* 0..1 */
	guint months;	/* 2..3 */
	guint days;	/* 4..5 */
	guint hours;	/* 6..7 */
	guint minutes;	/* 8..9 */
	guint seconds;	/* 10..11 */

	years = gcm_parser_decode_16 (data + 0x00);
	months = gcm_parser_decode_16 (data + 0x02);
	days = gcm_parser_decode_16 (data + 0x04);
	hours = gcm_parser_decode_16 (data + 0x06);
	minutes = gcm_parser_decode_16 (data + 0x08);
	seconds = gcm_parser_decode_16 (data + 0x0a);

	/* invalid / unknown */
	if (years == 0)
		return NULL;

	/* localise */
	return gcm_utils_format_date_time (years, months, days, hours, minutes, seconds);
}

/**
 * gcm_profile_parse_data:
 **/
gboolean
gcm_profile_parse_data (GcmProfile *profile, const guint8 *data, gsize length, GError **error)
{
	gboolean ret = FALSE;
	guint num_tags;
	guint i;
	guint tag_id;
	guint offset;
	guint tag_size;
	guint tag_offset;
	gchar *signature;
	guint32 profile_class;
	guint32 color_space;
	GcmProfilePrivate *priv = profile->priv;

	g_return_val_if_fail (GCM_IS_PROFILE (profile), FALSE);
	g_return_val_if_fail (data != NULL, FALSE);
	g_return_val_if_fail (priv->loaded == FALSE, FALSE);

	priv->loaded = TRUE;

	/* ensure we have the header */
	if (length < 0x84) {
		if (error != NULL)
			*error = g_error_new (1, 0, "profile was not valid (file size too small)");
		goto out;
	}

	/* check we are not a localized tag */
	ret = (memcmp (&data[GCM_SIGNATURE], "acsp", 4) == 0);
	if (!ret) {
		/* copy the 4 bytes of the invalid signature, with a '\0' byte */
		signature = g_new0 (gchar, 5);
		for (i=0; i<5; i++)
			signature[i] = data[GCM_SIGNATURE + i];
		if (error != NULL)
			*error = g_error_new (1, 0, "not an ICC profile, signature is '%s', expecting 'acsp'", signature);
		g_free (signature);
		goto out;
	}

	/* get the profile type */
	profile_class = gcm_parser_decode_32 (data + GCM_TYPE);
	switch (profile_class) {
	case icSigInputClass:
		priv->profile_type = GCM_PROFILE_TYPE_INPUT_DEVICE;
		break;
	case icSigDisplayClass:
		priv->profile_type = GCM_PROFILE_TYPE_DISPLAY_DEVICE;
		break;
	case icSigOutputClass:
		priv->profile_type = GCM_PROFILE_TYPE_OUTPUT_DEVICE;
		break;
	case icSigLinkClass:
		priv->profile_type = GCM_PROFILE_TYPE_DEVICELINK;
		break;
	case icSigColorSpaceClass:
		priv->profile_type = GCM_PROFILE_TYPE_COLORSPACE_CONVERSION;
		break;
	case icSigAbstractClass:
		priv->profile_type = GCM_PROFILE_TYPE_ABSTRACT;
		break;
	case icSigNamedColorClass:
		priv->profile_type = GCM_PROFILE_TYPE_NAMED_COLOR;
		break;
	default:
		priv->profile_type = GCM_PROFILE_TYPE_UNKNOWN;
	}

	/* get colorspace */
	color_space = gcm_parser_decode_32 (data + GCM_COLORSPACE);
	switch (color_space) {
	case icSigXYZData:
		priv->colorspace = GCM_PROFILE_COLORSPACE_XYZ;
		break;
	case icSigLabData:
		priv->colorspace = GCM_PROFILE_COLORSPACE_LAB;
		break;
	case icSigLuvData:
		priv->colorspace = GCM_PROFILE_COLORSPACE_LUV;
		break;
	case icSigYCbCrData:
		priv->colorspace = GCM_PROFILE_COLORSPACE_YCBCR;
		break;
	case icSigYxyData:
		priv->colorspace = GCM_PROFILE_COLORSPACE_YXY;
		break;
	case icSigRgbData:
		priv->colorspace = GCM_PROFILE_COLORSPACE_RGB;
		break;
	case icSigGrayData:
		priv->colorspace = GCM_PROFILE_COLORSPACE_GRAY;
		break;
	case icSigHsvData:
		priv->colorspace = GCM_PROFILE_COLORSPACE_HSV;
		break;
	case icSigCmykData:
		priv->colorspace = GCM_PROFILE_COLORSPACE_CMYK;
		break;
	case icSigCmyData:
		priv->colorspace = GCM_PROFILE_COLORSPACE_CMY;
		break;
	default:
		priv->colorspace = GCM_PROFILE_COLORSPACE_UNKNOWN;
	}

	/* get the profile created time and date */
	priv->datetime = gcm_parser_get_date_time (data + GCM_CREATION_DATE_TIME);
	if (priv->datetime != NULL)
		egg_debug ("created: %s", priv->datetime);

	/* get the number of tags in the file */
	num_tags = gcm_parser_decode_32 (data + GCM_NUMTAGS);
	egg_debug ("number of tags: %i", num_tags);

	for (i=0; i<num_tags; i++) {
		const gchar *tag_description;
		offset = GCM_TAG_WIDTH * i;
		tag_id = gcm_parser_decode_32 (data + GCM_BODY + offset + GCM_TAG_ID);
		tag_offset = gcm_parser_decode_32 (data + GCM_BODY + offset + GCM_TAG_OFFSET);
		tag_size = gcm_parser_decode_32 (data + GCM_BODY + offset + GCM_TAG_SIZE);

		/* print description */
		tag_description = gcm_prefs_get_tag_description (tag_id);
		if (tag_description == NULL)
			egg_debug ("unknown tag %x is present at 0x%x with size %u", tag_id, tag_offset, tag_size);
		else
			egg_debug ("named tag %x [%s] is present at 0x%x with size %u", tag_id, tag_description, tag_offset, tag_size);

		if (tag_id == icSigProfileDescriptionTag) {
			priv->description = gcm_profile_parse_multi_localized_unicode (profile, data + tag_offset, tag_size);
			egg_debug ("found DESC: %s", priv->description);
		}
		if (tag_id == icSigCopyrightTag) {
			priv->copyright = gcm_profile_parse_multi_localized_unicode (profile, data + tag_offset, tag_size);
			egg_debug ("found COPYRIGHT: %s", priv->copyright);
		}
		if (tag_id == icSigDeviceMfgDescTag) {
			priv->manufacturer = gcm_profile_parse_multi_localized_unicode (profile, data + tag_offset, tag_size);
			egg_debug ("found MANUFACTURER: %s", priv->manufacturer);
		}
		if (tag_id == icSigDeviceModelDescTag) {
			priv->model = gcm_profile_parse_multi_localized_unicode (profile, data + tag_offset, tag_size);
			egg_debug ("found MODEL: %s", priv->model);
		}
		if (tag_id == icSigMachineLookUpTableTag) {
			egg_debug ("found MLUT which is a fixed size block");
			ret = gcm_parser_load_icc_mlut (profile, data + tag_offset, tag_size);
			if (!ret) {
				*error = g_error_new (1, 0, "failed to load mlut");
				goto out;
			}
		}
		if (tag_id == icSigVideoCartGammaTableTag) {
			egg_debug ("found VCGT");
			if (tag_size == 1584)
				priv->adobe_gamma_workaround = TRUE;
			ret = gcm_parser_load_icc_vcgt (profile, data + tag_offset, tag_size);
			if (!ret) {
				*error = g_error_new (1, 0, "failed to load vcgt");
				goto out;
			}
		}
		if (tag_id == icSigRedTRCTag) {
			egg_debug ("found TRC (red)");
			ret = gcm_parser_load_icc_trc (profile, data + tag_offset, tag_size, 0);
			if (!ret) {
				*error = g_error_new (1, 0, "failed to load trc");
				goto out;
			}
		}
		if (tag_id == icSigGreenTRCTag) {
			egg_debug ("found TRC (green)");
			ret = gcm_parser_load_icc_trc (profile, data + tag_offset, tag_size, 1);
			if (!ret) {
				*error = g_error_new (1, 0, "failed to load trc");
				goto out;
			}
		}
		if (tag_id == icSigBlueTRCTag) {
			egg_debug ("found TRC (blue)");
			ret = gcm_parser_load_icc_trc (profile, data + tag_offset, tag_size, 2);
			if (!ret) {
				*error = g_error_new (1, 0, "failed to load trc");
				goto out;
			}
		}
		if (tag_id == icSigMediaWhitePointTag) {
			egg_debug ("found media white point");
			ret = gcm_parser_load_icc_xyz_type (profile, data + tag_offset, tag_size, priv->white_point);
			if (!ret) {
				*error = g_error_new (1, 0, "failed to load white point");
				goto out;
			}
		}
		if (tag_id == icSigMediaBlackPointTag) {
			egg_debug ("found media black point");
			ret = gcm_parser_load_icc_xyz_type (profile, data + tag_offset, tag_size, priv->black_point);
			if (!ret) {
				*error = g_error_new (1, 0, "failed to load white point");
				goto out;
			}
		}
		if (tag_id == icSigRedColorantTag) {
			egg_debug ("found red matrix column");
			ret = gcm_parser_load_icc_xyz_type (profile, data + tag_offset, tag_size, priv->luminance_red);
			if (!ret) {
				*error = g_error_new (1, 0, "failed to load red matrix");
				goto out;
			}
		}
		if (tag_id == icSigGreenColorantTag) {
			egg_debug ("found green matrix column");
			ret = gcm_parser_load_icc_xyz_type (profile, data + tag_offset, tag_size, priv->luminance_green);
			if (!ret) {
				*error = g_error_new (1, 0, "failed to load green matrix");
				goto out;
			}
		}
		if (tag_id == icSigBlueColorantTag) {
			egg_debug ("found blue matrix column");
			ret = gcm_parser_load_icc_xyz_type (profile, data + tag_offset, tag_size, priv->luminance_blue);
			if (!ret) {
				*error = g_error_new (1, 0, "failed to load blue matrix");
				goto out;
			}
		}
	}

	/* there's nothing sensible to display */
	if (priv->description == NULL || priv->description[0] == '\0') {
		g_free (priv->description);
		if (priv->filename != NULL) {
			priv->description = g_path_get_basename (priv->filename);
		} else {
			/* TRANSLATORS: this is where the ICC profile has no description */
			priv->description = _("Missing description");
		}
	}

	/* Ensure this is displayable */
	if (priv->description != NULL)
		gcm_profile_ensure_printable (priv->description);
	if (priv->copyright != NULL)
		gcm_profile_ensure_printable (priv->copyright);
	if (priv->manufacturer != NULL)
		gcm_profile_ensure_printable (priv->manufacturer);
	if (priv->model != NULL)
		gcm_profile_ensure_printable (priv->model);

	/* save the length */
	priv->size = length;

	/* success */
	ret = TRUE;

	egg_debug ("Has MLUT:         %s", priv->has_mlut ? "YES" : "NO");
	egg_debug ("Has VCGT formula: %s", priv->has_vcgt_formula ? "YES" : "NO");
	egg_debug ("Has VCGT table:   %s", priv->has_vcgt_table ? "YES" : "NO");
	egg_debug ("Has curve table:  %s", priv->has_curve_table ? "YES" : "NO");
	egg_debug ("Has fixed gamma:  %s", priv->has_curve_fixed ? "YES" : "NO");
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
	guint length;
	GError *error_local = NULL;
	GcmProfilePrivate *priv = profile->priv;

	g_return_val_if_fail (GCM_IS_PROFILE (profile), FALSE);
	g_return_val_if_fail (filename != NULL, FALSE);
	g_return_val_if_fail (priv->loaded == FALSE, FALSE);

	egg_debug ("loading '%s'", filename);

	/* save */
	g_free (priv->filename);
	priv->filename = g_strdup (filename);

	/* load files */
	ret = g_file_get_contents (filename, &data, (gsize *) &length, &error_local);
	if (!ret) {
		if (error != NULL)
			*error = g_error_new (1, 0, "failed to load profile: %s", error_local->message);
		g_error_free (error_local);
		goto out;
	}

	/* parse the data */
	ret = gcm_profile_parse_data (profile, (const guint8*)data, length, error);
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
GcmClut *
gcm_profile_generate (GcmProfile *profile, guint size)
{
	guint i;
	guint ratio;
	GcmClutData *tmp;
	GcmClutData *vcgt_data;
	GcmClutData *mlut_data;
	GcmClutData *trc_data;
	gfloat gamma_red, min_red, max_red;
	gfloat gamma_green, min_green, max_green;
	gfloat gamma_blue, min_blue, max_blue;
	guint num_entries;
	GcmClut *clut;
	GPtrArray *array;
	gfloat inverse_ratio;
	guint idx;
	gfloat frac;

	g_return_val_if_fail (GCM_IS_PROFILE (profile), NULL);
	g_return_val_if_fail (size != 0, FALSE);

	/* reduce dereferences */
	vcgt_data = profile->priv->vcgt_data;
	mlut_data = profile->priv->mlut_data;
	trc_data = profile->priv->trc_data;

	/* create new output array */
	clut = gcm_clut_new ();
	array = g_ptr_array_new_with_free_func (g_free);

	if (profile->priv->has_vcgt_table) {

		/* simply subsample if the LUT is smaller than the number of entries in the file */
		num_entries = profile->priv->vcgt_data_size;
		if (num_entries >= size) {
			ratio = (guint) (num_entries / size);
			for (i=0; i<size; i++) {
				/* add a point */
				tmp = g_new0 (GcmClutData, 1);
				tmp->red = vcgt_data[ratio*i].red;
				tmp->green = vcgt_data[ratio*i].green;
				tmp->blue = vcgt_data[ratio*i].blue;
				g_ptr_array_add (array, tmp);
			}
			goto out;
		}

		/* LUT is bigger than number of entries, so interpolate */
		inverse_ratio = (gfloat) num_entries / size;
		vcgt_data[num_entries].red = 0xffff;
		vcgt_data[num_entries].green = 0xffff;
		vcgt_data[num_entries].blue = 0xffff;

		/* interpolate */
		for (i=0; i<size; i++) {
			idx = floor(i*inverse_ratio);
			frac = (i*inverse_ratio) - idx;
			tmp = g_new0 (GcmClutData, 1);
			tmp->red = vcgt_data[idx].red * (1.0f-frac) + vcgt_data[idx + 1].red * frac;
			tmp->green = vcgt_data[idx].green * (1.0f-frac) + vcgt_data[idx + 1].green * frac;
			tmp->blue = vcgt_data[idx].blue * (1.0f-frac) + vcgt_data[idx + 1].blue * frac;
			g_ptr_array_add (array, tmp);
		}
		goto out;
	}

	if (profile->priv->has_vcgt_formula) {

		gamma_red = (gfloat) vcgt_data[0].red / 65536.0;
		gamma_green = (gfloat) vcgt_data[0].green / 65536.0;
		gamma_blue = (gfloat) vcgt_data[0].blue / 65536.0;
		min_red = (gfloat) vcgt_data[1].red / 65536.0;
		min_green = (gfloat) vcgt_data[1].green / 65536.0;
		min_blue = (gfloat) vcgt_data[1].blue / 65536.0;
		max_red = (gfloat) vcgt_data[2].red / 65536.0;
		max_green = (gfloat) vcgt_data[2].green / 65536.0;
		max_blue = (gfloat) vcgt_data[2].blue / 65536.0;

		/* create mapping of desired size */
		for (i=0; i<size; i++) {
			/* add a point */
			tmp = g_new0 (GcmClutData, 1);
			tmp->red = 65536.0 * ((gdouble) pow ((gdouble) i / (gdouble) size, gamma_red) * (max_red - min_red) + min_red);
			tmp->green = 65536.0 * ((gdouble) pow ((gdouble) i / (gdouble) size, gamma_green) * (max_green - min_green) + min_green);
			tmp->blue = 65536.0 * ((gdouble) pow ((gdouble) i / (gdouble) size, gamma_blue) * (max_blue - min_blue) + min_blue);
			g_ptr_array_add (array, tmp);
		}
		goto out;
	}

	if (profile->priv->has_mlut) {

		/* roughly interpolate table */
		ratio = (guint) (256 / (size));
		for (i=0; i<size; i++) {
			/* add a point */
			tmp = g_new0 (GcmClutData, 1);
			tmp->red = mlut_data[ratio*i].red;
			tmp->green = mlut_data[ratio*i].green;
			tmp->blue = mlut_data[ratio*i].blue;
			g_ptr_array_add (array, tmp);
		}
		goto out;
	}

	if (profile->priv->has_curve_table) {

		/* simply subsample if the LUT is smaller than the number of entries in the file */
		num_entries = profile->priv->trc_data_size;
		if (num_entries >= size) {
			ratio = (guint) (num_entries / size);
			for (i=0; i<size; i++) {
				/* add a point */
				tmp = g_new0 (GcmClutData, 1);
				tmp->red = trc_data[ratio*i].red;
				tmp->green = trc_data[ratio*i].green;
				tmp->blue = trc_data[ratio*i].blue;
				g_ptr_array_add (array, tmp);
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
			tmp = g_new0 (GcmClutData, 1);
			tmp->red = trc_data[idx].red * (1.0f-frac) + trc_data[idx + 1].red * frac;
			tmp->green = trc_data[idx].green * (1.0f-frac) + trc_data[idx + 1].green * frac;
			tmp->blue = trc_data[idx].blue * (1.0f-frac) + trc_data[idx + 1].blue * frac;
			g_ptr_array_add (array, tmp);
		}
		goto out;
	}

	if (profile->priv->has_curve_fixed) {

		/* use a simple y=x^gamma formula */
		for (i=0; i<size; i++) {
			gfloat part;
			part = (gfloat) i / (gfloat) size;
			/* add a point */
			tmp = g_new0 (GcmClutData, 1);
			tmp->red = pow (part, (gfloat) trc_data[0].red / 256.0f) * 65000.f;
			tmp->green = pow (part, (gfloat) trc_data[0].green / 256.0f) * 65000.f;
			tmp->blue = pow (part, (gfloat) trc_data[0].blue / 256.0f) * 65000.f;
			g_ptr_array_add (array, tmp);
		}
		goto out;
	}

	/* bugger */
	egg_debug ("no LUT to generate");
out:
	gcm_clut_set_source_array (clut, array);
	g_ptr_array_unref (array);
	return clut;
}

/**
 * gcm_profile_type_to_text:
 **/
const gchar *
gcm_profile_type_to_text (GcmProfileType type)
{
	if (type == GCM_PROFILE_TYPE_INPUT_DEVICE)
		return "input-device";
	if (type == GCM_PROFILE_TYPE_DISPLAY_DEVICE)
		return "display-device";
	if (type == GCM_PROFILE_TYPE_OUTPUT_DEVICE)
		return "output-device";
	if (type == GCM_PROFILE_TYPE_DEVICELINK)
		return "devicelink";
	if (type == GCM_PROFILE_TYPE_COLORSPACE_CONVERSION)
		return "colorspace-conversion";
	if (type == GCM_PROFILE_TYPE_ABSTRACT)
		return "abstract";
	if (type == GCM_PROFILE_TYPE_NAMED_COLOR)
		return "named-color";
	return "unknown";
}

/**
 * gcm_profile_colorspace_to_text:
 **/
const gchar *
gcm_profile_colorspace_to_text (GcmProfileColorspace type)
{
	if (type == GCM_PROFILE_COLORSPACE_XYZ)
		return "xyz";
	if (type == GCM_PROFILE_COLORSPACE_LAB)
		return "lab";
	if (type == GCM_PROFILE_COLORSPACE_LUV)
		return "luv";
	if (type == GCM_PROFILE_COLORSPACE_YCBCR)
		return "ycbcr";
	if (type == GCM_PROFILE_COLORSPACE_YXY)
		return "yxy";
	if (type == GCM_PROFILE_COLORSPACE_RGB)
		return "rgb";
	if (type == GCM_PROFILE_COLORSPACE_GRAY)
		return "gray";
	if (type == GCM_PROFILE_COLORSPACE_HSV)
		return "hsv";
	if (type == GCM_PROFILE_COLORSPACE_CMYK)
		return "cmyk";
	if (type == GCM_PROFILE_COLORSPACE_CMY)
		return "cmy";
	return "unknown";
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
	case PROP_MANUFACTURER:
		g_value_set_string (value, priv->manufacturer);
		break;
	case PROP_MODEL:
		g_value_set_string (value, priv->model);
		break;
	case PROP_DATETIME:
		g_value_set_string (value, priv->datetime);
		break;
	case PROP_DESCRIPTION:
		g_value_set_string (value, priv->description);
		break;
	case PROP_FILENAME:
		g_value_set_string (value, priv->filename);
		break;
	case PROP_TYPE:
		g_value_set_uint (value, priv->profile_type);
		break;
	case PROP_COLORSPACE:
		g_value_set_uint (value, priv->colorspace);
		break;
	case PROP_SIZE:
		g_value_set_uint (value, priv->size);
		break;
	case PROP_WHITE_POINT:
		g_value_set_object (value, priv->white_point);
		break;
	case PROP_LUMINANCE_RED:
		g_value_set_object (value, priv->luminance_red);
		break;
	case PROP_LUMINANCE_GREEN:
		g_value_set_object (value, priv->luminance_green);
		break;
	case PROP_LUMINANCE_BLUE:
		g_value_set_object (value, priv->luminance_blue);
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
	 * GcmProfile:manufacturer:
	 */
	pspec = g_param_spec_string ("manufacturer", NULL, NULL,
				     NULL,
				     G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_MANUFACTURER, pspec);

	/**
	 * GcmProfile:model:
	 */
	pspec = g_param_spec_string ("model", NULL, NULL,
				     NULL,
				     G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_MODEL, pspec);

	/**
	 * GcmProfile:datetime:
	 */
	pspec = g_param_spec_string ("datetime", NULL, NULL,
				     NULL,
				     G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_DATETIME, pspec);

	/**
	 * GcmProfile:description:
	 */
	pspec = g_param_spec_string ("description", NULL, NULL,
				     NULL,
				     G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_DESCRIPTION, pspec);

	/**
	 * GcmProfile:filename:
	 */
	pspec = g_param_spec_string ("filename", NULL, NULL,
				     NULL,
				     G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_FILENAME, pspec);

	/**
	 * GcmProfile:type:
	 */
	pspec = g_param_spec_uint ("type", NULL, NULL,
				   0, G_MAXUINT, 0,
				   G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_TYPE, pspec);

	/**
	 * GcmProfile:colorspace:
	 */
	pspec = g_param_spec_uint ("colorspace", NULL, NULL,
				   0, G_MAXUINT, 0,
				   G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_COLORSPACE, pspec);

	/**
	 * GcmProfile:size:
	 */
	pspec = g_param_spec_uint ("size", NULL, NULL,
				   0, G_MAXUINT, 0,
				   G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_SIZE, pspec);

	/**
	 * GcmProfile:white-point:
	 */
	pspec = g_param_spec_object ("white-point", NULL, NULL,
				     GCM_TYPE_XYZ,
				     G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_WHITE_POINT, pspec);

	/**
	 * GcmProfile:luminance-red:
	 */
	pspec = g_param_spec_object ("luminance-red", NULL, NULL,
				     GCM_TYPE_XYZ,
				     G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_LUMINANCE_RED, pspec);

	/**
	 * GcmProfile:luminance-green:
	 */
	pspec = g_param_spec_object ("luminance-green", NULL, NULL,
				     GCM_TYPE_XYZ,
				     G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_LUMINANCE_GREEN, pspec);

	/**
	 * GcmProfile:luminance-blue:
	 */
	pspec = g_param_spec_object ("luminance-blue", NULL, NULL,
				     GCM_TYPE_XYZ,
				     G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_LUMINANCE_BLUE, pspec);

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
	profile->priv->colorspace = GCM_PROFILE_COLORSPACE_UNKNOWN;
	profile->priv->white_point = gcm_xyz_new ();
	profile->priv->black_point = gcm_xyz_new ();
	profile->priv->luminance_red = gcm_xyz_new ();
	profile->priv->luminance_green = gcm_xyz_new ();
	profile->priv->luminance_blue = gcm_xyz_new ();
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
	g_free (priv->description);
	g_free (priv->filename);
	g_free (priv->manufacturer);
	g_free (priv->model);
	g_free (priv->datetime);
	g_free (priv->vcgt_data);
	g_free (priv->mlut_data);
	g_free (priv->trc_data);
	g_object_unref (priv->white_point);
	g_object_unref (priv->black_point);
	g_object_unref (priv->luminance_red);
	g_object_unref (priv->luminance_green);
	g_object_unref (priv->luminance_blue);

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

/***************************************************************************
 ***                          MAKE CHECK TESTS                           ***
 ***************************************************************************/
#ifdef EGG_TEST
#include "egg-test.h"

typedef struct {
	const gchar *copyright;
	const gchar *manufacturer;
	const gchar *model;
	const gchar *datetime;
	const gchar *description;
	GcmProfileType type;
	GcmProfileColorspace colorspace;
	gfloat luminance;
} GcmProfileTestData;

void
gcm_profile_test_parse_file (EggTest *test, const guint8 *datafile, GcmProfileTestData *test_data)
{
	gchar *filename;
	gchar *filename_tmp;
	gchar *copyright;
	gchar *manufacturer;
	gchar *model;
	gchar *datetime;
	gchar *description;
	gchar *ascii_string;
	gchar *pnp_id;
	gchar *data;
	guint width;
	guint type;
	guint colorspace;
	gfloat gamma;
	gboolean ret;
	GError *error = NULL;
	GcmProfile *profile;
	GcmXyz *xyz;
	gfloat luminance;

	/************************************************************/
	egg_test_title (test, "get a profile object");
	profile = gcm_profile_new ();
	egg_test_assert (test, profile != NULL);

	/************************************************************/
	egg_test_title (test, "get filename of data file");
	filename = egg_test_get_data_file (datafile);
	egg_test_assert (test, (filename != NULL));

	/************************************************************/
	egg_test_title (test, "load ICC file");
	ret = gcm_profile_parse (profile, filename, &error);
	if (ret)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "failed to parse: %s", error->message);

	/* get some properties */
	g_object_get (profile,
		      "copyright", &copyright,
		      "manufacturer", &manufacturer,
		      "model", &model,
		      "datetime", &datetime,
		      "description", &description,
		      "filename", &filename_tmp,
		      "type", &type,
		      "colorspace", &colorspace,
		      NULL);

	/************************************************************/
	egg_test_title (test, "check filename for %s", datafile);
	if (g_strcmp0 (filename, filename_tmp) == 0)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "invalid value: %s, expecting: %s", filename, filename_tmp);

	/************************************************************/
	egg_test_title (test, "check copyright for %s", datafile);
	if (g_strcmp0 (copyright, test_data->copyright) == 0)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "invalid value: %s, expecting: %s", copyright, test_data->copyright);

	/************************************************************/
	egg_test_title (test, "check manufacturer for %s", datafile);
	if (g_strcmp0 (manufacturer, test_data->manufacturer) == 0)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "invalid value: %s, expecting: %s", manufacturer, test_data->manufacturer);

	/************************************************************/
	egg_test_title (test, "check model for %s", datafile);
	if (g_strcmp0 (model, test_data->model) == 0)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "invalid value: %s, expecting: %s", model, test_data->model);

	/************************************************************/
	egg_test_title (test, "check datetime for %s", datafile);
	if (g_strcmp0 (datetime, test_data->datetime) == 0)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "invalid value: %s, expecting: %s", datetime, test_data->datetime);

	/************************************************************/
	egg_test_title (test, "check description for %s", datafile);
	if (g_strcmp0 (description, test_data->description) == 0)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "invalid value: %s, expecting: %s", description, test_data->description);

	/************************************************************/
	egg_test_title (test, "check type for %s", datafile);
	if (type == test_data->type)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "invalid value: %i, expecting: %i", type, test_data->type);

	/************************************************************/
	egg_test_title (test, "check colorspace for %s", datafile);
	if (colorspace == test_data->colorspace)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "invalid value: %i, expecting: %i", colorspace, test_data->colorspace);

	/************************************************************/
	egg_test_title (test, "check luminance red %s", datafile);
	g_object_get (profile,
		      "luminance-red", &xyz,
		      NULL);
	luminance = gcm_xyz_get_x (xyz);
	if (fabs (luminance - test_data->luminance) < 0.001)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "invalid value: %f, expecting: %f", luminance, test_data->luminance);

	g_object_unref (xyz);
	g_object_unref (profile);
	g_free (copyright);
	g_free (manufacturer);
	g_free (model);
	g_free (datetime);
	g_free (description);
	g_free (data);
	g_free (filename);
	g_free (filename_tmp);
}

void
gcm_profile_test (EggTest *test)
{
	GcmProfileTestData test_data;
	gfloat fp;
	gfloat expected;
	gboolean ret;
	const gchar temp[5] = {0x6d, 0x42, 0x41, 0x20, 0x00};

	if (!egg_test_start (test, "GcmProfile"))
		return;

	/************************************************************/
	egg_test_title (test, "convert s15 fixed 16 number [1]");
	fp = gcm_parser_s15_fixed_16_number_to_float (0x80000000);
	expected = -32768.0;
	if (fp == expected)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "invalid value: %f, expecting: %f", fp, expected);

	/************************************************************/
	egg_test_title (test, "convert s15 fixed 16 number [2]");
	fp = gcm_parser_s15_fixed_16_number_to_float (0x0);
	expected = 0.0;
	if (fp == expected)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "invalid value: %f, expecting: %f", fp, expected);

	/************************************************************/
	egg_test_title (test, "convert s15 fixed 16 number [3]");
	fp = gcm_parser_s15_fixed_16_number_to_float (0x10000);
	expected = 1.0;
	if (fp == expected)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "invalid value: %f, expecting: %f", fp, expected);

	/* bluish test */
	test_data.copyright = "Copyright (c) 1998 Hewlett-Packard Company";
	test_data.manufacturer = "IEC http://www.iec.ch";
	test_data.model = "IEC 61966-2.1 Default RGB colour space - sRGB";
	test_data.description = "Blueish Test";
	test_data.type = GCM_PROFILE_TYPE_DISPLAY_DEVICE;
	test_data.colorspace = GCM_PROFILE_COLORSPACE_RGB;
	test_data.luminance = 0.648454;
	test_data.datetime = "9 February 1998, 06:49:00";
	gcm_profile_test_parse_file (test, "bluish.icc", &test_data);

	/* Adobe test */
	test_data.copyright = "Copyright (c) 1998 Hewlett-Packard Company Modified using Adobe Gamma";
	test_data.manufacturer = "IEC http://www.iec.ch";
	test_data.model = "IEC 61966-2.1 Default RGB colour space - sRGB";
	test_data.description = "ADOBEGAMMA-Test";
	test_data.type = GCM_PROFILE_TYPE_DISPLAY_DEVICE;
	test_data.colorspace = GCM_PROFILE_COLORSPACE_RGB;
	test_data.luminance = 0.648446;
	test_data.datetime = "16 August 2005, 21:49:54";
	gcm_profile_test_parse_file (test, "AdobeGammaTest.icm", &test_data);

	egg_test_end (test);
}
#endif


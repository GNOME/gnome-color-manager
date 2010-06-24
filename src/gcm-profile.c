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
 * SECTION:gcm-profile
 * @short_description: A parser object that understands the ICC profile data format.
 *
 * This object is a simple parser for the ICC binary profile data. If only understands
 * a subset of the ICC profile, just enought to get some metadata and the LUT.
 */

#include "config.h"

#include <glib-object.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <lcms.h>

#include "egg-debug.h"

#include "gcm-profile.h"
#include "gcm-utils.h"
#include "gcm-xyz.h"

static void     gcm_profile_finalize	(GObject     *object);

#define GCM_PROFILE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GCM_TYPE_PROFILE, GcmProfilePrivate))

#define GCM_NUMTAGS			0x80
#define GCM_BODY			0x84

#define GCM_TAG_ID			0x00
#define GCM_TAG_OFFSET			0x04
#define GCM_TAG_SIZE			0x08
#define GCM_TAG_WIDTH			0x0c

#define icSigVideoCartGammaTableTag	0x76636774
#define icSigMachineLookUpTableTag	0x6d4c5554

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
	GcmProfileKind		 kind;
	GcmColorspace		 colorspace;
	guint			 size;
	gboolean		 has_vcgt;
	gboolean		 can_delete;
	gchar			*description;
	gchar			*filename;
	gchar			*copyright;
	gchar			*manufacturer;
	gchar			*model;
	gchar			*datetime;
	gchar			*checksum;
	GcmXyz			*white;
	GcmXyz			*black;
	GcmXyz			*red;
	GcmXyz			*green;
	GcmXyz			*blue;
	GFileMonitor		*monitor;

	gboolean			 loaded;
	gboolean			 has_mlut;
	gboolean			 has_vcgt_formula;
	gboolean			 has_vcgt_table;
	cmsHPROFILE			 lcms_profile;
	GcmClutData			*vcgt_data;
	guint				 vcgt_data_size;
	GcmClutData			*mlut_data;
	guint				 mlut_data_size;
	gboolean			 adobe_gamma_workaround;
};

enum {
	PROP_0,
	PROP_COPYRIGHT,
	PROP_MANUFACTURER,
	PROP_MODEL,
	PROP_DATETIME,
	PROP_CHECKSUM,
	PROP_DESCRIPTION,
	PROP_FILENAME,
	PROP_KIND,
	PROP_COLORSPACE,
	PROP_SIZE,
	PROP_HAS_VCGT,
	PROP_CAN_DELETE,
	PROP_WHITE,
	PROP_BLACK,
	PROP_RED,
	PROP_GREEN,
	PROP_BLUE,
	PROP_LAST
};

G_DEFINE_TYPE (GcmProfile, gcm_profile, G_TYPE_OBJECT)

static void gcm_profile_file_monitor_changed_cb (GFileMonitor *monitor, GFile *file, GFile *other_file, GFileMonitorEvent event_type, GcmProfile *profile);

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

	num_channels = gcm_parser_decode_16 (data + GCM_VCGT_TABLE_NUM_CHANNELS);
	num_entries = gcm_parser_decode_16 (data + GCM_VCGT_TABLE_NUM_ENTRIES);
	entry_size = gcm_parser_decode_16 (data + GCM_VCGT_TABLE_NUM_SIZE);

	/* work-around for AdobeGamma-ProfileLcms1s (taken from xcalib) */
	if (profile->priv->adobe_gamma_workaround) {
		egg_debug ("Working around AdobeGamma profile");
		entry_size = 2;
		num_entries = 256;
		num_channels = 3;
	}

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
	egg_warning ("gamma type encoding not recognized");
out:
	return ret;
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
		len = gcm_parser_decode_32 (data + 20);
		offset_name = gcm_parser_decode_32 (data + 24);
		text = gcm_profile_utf16be_to_locale (data + offset_name, len);
		goto out;
	}

	/* an unrecognized tag */
	for (i=0x0; i<0x1c; i++) {
		egg_warning ("unrecognized text tag");
		if (data[i] >= 'A' && data[i] <= 'z')
			egg_debug ("%i\t%c (%i)", i, data[i], data[i]);
		else
			egg_debug ("%i\t  (%i)", i, data[i]);
	}
out:
	return text;
}

/**
 * gcm_profile_get_description:
 **/
const gchar *
gcm_profile_get_description (GcmProfile *profile)
{
	g_return_val_if_fail (GCM_IS_PROFILE (profile), NULL);
	return profile->priv->description;
}

/**
 * gcm_profile_set_description:
 **/
void
gcm_profile_set_description (GcmProfile *profile, const gchar *description)
{
	GcmProfilePrivate *priv = profile->priv;
	g_return_if_fail (GCM_IS_PROFILE (profile));

	g_free (priv->description);
	priv->description = g_strdup (description);

	if (priv->description != NULL)
		gcm_utils_ensure_printable (priv->description);

	/* there's nothing sensible to display */
	if (priv->description == NULL || priv->description[0] == '\0') {
		g_free (priv->description);
		if (priv->filename != NULL) {
			priv->description = g_path_get_basename (priv->filename);
		} else {
			/* TRANSLATORS: this is where the ICC profile has no description */
			priv->description = g_strdup (_("Missing description"));
		}
	}
	g_object_notify (G_OBJECT (profile), "description");
}

/**
 * gcm_profile_get_filename:
 **/
const gchar *
gcm_profile_get_filename (GcmProfile *profile)
{
	g_return_val_if_fail (GCM_IS_PROFILE (profile), NULL);
	return profile->priv->filename;
}

/**
 * gcm_profile_has_colorspace_description:
 *
 * Return value: if the description mentions the profile colorspace explicity,
 * e.g. "Adobe RGB" for %GCM_COLORSPACE_RGB.
 **/
gboolean
gcm_profile_has_colorspace_description (GcmProfile *profile)
{
	GcmProfilePrivate *priv = profile->priv;
	g_return_val_if_fail (GCM_IS_PROFILE (profile), FALSE);

	/* for each profile type */
	if (priv->colorspace == GCM_COLORSPACE_RGB)
		return (g_strstr_len (priv->description, -1, "RGB") != NULL);
	if (priv->colorspace == GCM_COLORSPACE_CMYK)
		return (g_strstr_len (priv->description, -1, "CMYK") != NULL);

	/* nothing */
	return FALSE;
}

/**
 * gcm_profile_set_filename:
 **/
void
gcm_profile_set_filename (GcmProfile *profile, const gchar *filename)
{
	GcmProfilePrivate *priv = profile->priv;
	GFile *file;

	g_return_if_fail (GCM_IS_PROFILE (profile));

	g_free (priv->filename);
	priv->filename = g_strdup (filename);

	/* unref old instance */
	if (priv->monitor != NULL) {
		g_object_unref (priv->monitor);
		priv->monitor = NULL;
	}

	/* setup watch on new profile */
	if (priv->filename != NULL) {
		file = g_file_new_for_path (priv->filename);
		priv->monitor = g_file_monitor_file (file, G_FILE_MONITOR_NONE, NULL, NULL);
		if (priv->monitor != NULL)
			g_signal_connect (priv->monitor, "changed", G_CALLBACK(gcm_profile_file_monitor_changed_cb), profile);
		g_object_unref (file);
	}
	g_object_notify (G_OBJECT (profile), "filename");
}

/**
 * gcm_profile_get_copyright:
 **/
const gchar *
gcm_profile_get_copyright (GcmProfile *profile)
{
	g_return_val_if_fail (GCM_IS_PROFILE (profile), NULL);
	return profile->priv->copyright;
}

/**
 * gcm_profile_set_copyright:
 **/
void
gcm_profile_set_copyright (GcmProfile *profile, const gchar *copyright)
{
	GcmProfilePrivate *priv = profile->priv;

	g_return_if_fail (GCM_IS_PROFILE (profile));

	g_free (priv->copyright);
	priv->copyright = g_strdup (copyright);
	if (priv->copyright != NULL)
		gcm_utils_ensure_printable (priv->copyright);
	g_object_notify (G_OBJECT (profile), "copyright");
}

/**
 * gcm_profile_get_model:
 **/
const gchar *
gcm_profile_get_model (GcmProfile *profile)
{
	g_return_val_if_fail (GCM_IS_PROFILE (profile), NULL);
	return profile->priv->model;
}

/**
 * gcm_profile_set_model:
 **/
void
gcm_profile_set_model (GcmProfile *profile, const gchar *model)
{
	GcmProfilePrivate *priv = profile->priv;

	g_return_if_fail (GCM_IS_PROFILE (profile));

	g_free (priv->model);
	priv->model = g_strdup (model);
	if (priv->model != NULL)
		gcm_utils_ensure_printable (priv->model);
	g_object_notify (G_OBJECT (profile), "model");
}

/**
 * gcm_profile_get_manufacturer:
 **/
const gchar *
gcm_profile_get_manufacturer (GcmProfile *profile)
{
	g_return_val_if_fail (GCM_IS_PROFILE (profile), NULL);
	return profile->priv->manufacturer;
}

/**
 * gcm_profile_set_manufacturer:
 **/
void
gcm_profile_set_manufacturer (GcmProfile *profile, const gchar *manufacturer)
{
	GcmProfilePrivate *priv = profile->priv;

	g_return_if_fail (GCM_IS_PROFILE (profile));

	g_free (priv->manufacturer);
	priv->manufacturer = g_strdup (manufacturer);
	if (priv->manufacturer != NULL)
		gcm_utils_ensure_printable (priv->manufacturer);
	g_object_notify (G_OBJECT (profile), "manufacturer");
}

/**
 * gcm_profile_get_datetime:
 **/
const gchar *
gcm_profile_get_datetime (GcmProfile *profile)
{
	g_return_val_if_fail (GCM_IS_PROFILE (profile), NULL);
	return profile->priv->datetime;
}

/**
 * gcm_profile_set_datetime:
 **/
void
gcm_profile_set_datetime (GcmProfile *profile, const gchar *datetime)
{
	GcmProfilePrivate *priv = profile->priv;

	g_return_if_fail (GCM_IS_PROFILE (profile));

	g_free (priv->datetime);
	priv->datetime = g_strdup (datetime);
	g_object_notify (G_OBJECT (profile), "datetime");
}

/**
 * gcm_profile_get_checksum:
 **/
const gchar *
gcm_profile_get_checksum (GcmProfile *profile)
{
	g_return_val_if_fail (GCM_IS_PROFILE (profile), NULL);
	return profile->priv->checksum;
}

/**
 * gcm_profile_set_checksum:
 **/
static void
gcm_profile_set_checksum (GcmProfile *profile, const gchar *checksum)
{
	GcmProfilePrivate *priv = profile->priv;

	g_return_if_fail (GCM_IS_PROFILE (profile));

	g_free (priv->checksum);
	priv->checksum = g_strdup (checksum);
	g_object_notify (G_OBJECT (profile), "checksum");
}

/**
 * gcm_profile_get_size:
 **/
guint
gcm_profile_get_size (GcmProfile *profile)
{
	g_return_val_if_fail (GCM_IS_PROFILE (profile), 0);
	return profile->priv->size;
}

/**
 * gcm_profile_set_size:
 **/
void
gcm_profile_set_size (GcmProfile *profile, guint size)
{
	g_return_if_fail (GCM_IS_PROFILE (profile));
	profile->priv->size = size;
	g_object_notify (G_OBJECT (profile), "size");
}

/**
 * gcm_profile_get_kind:
 **/
GcmProfileKind
gcm_profile_get_kind (GcmProfile *profile)
{
	g_return_val_if_fail (GCM_IS_PROFILE (profile), GCM_PROFILE_KIND_UNKNOWN);
	return profile->priv->kind;
}

/**
 * gcm_profile_set_kind:
 **/
void
gcm_profile_set_kind (GcmProfile *profile, GcmProfileKind kind)
{
	g_return_if_fail (GCM_IS_PROFILE (profile));
	profile->priv->kind = kind;
	g_object_notify (G_OBJECT (profile), "kind");
}

/**
 * gcm_profile_get_colorspace:
 **/
GcmColorspace
gcm_profile_get_colorspace (GcmProfile *profile)
{
	g_return_val_if_fail (GCM_IS_PROFILE (profile), GCM_COLORSPACE_UNKNOWN);
	return profile->priv->colorspace;
}

/**
 * gcm_profile_set_colorspace:
 **/
void
gcm_profile_set_colorspace (GcmProfile *profile, GcmColorspace colorspace)
{
	g_return_if_fail (GCM_IS_PROFILE (profile));
	profile->priv->colorspace = colorspace;
	g_object_notify (G_OBJECT (profile), "colorspace");
}

/**
 * gcm_profile_get_has_vcgt:
 **/
gboolean
gcm_profile_get_has_vcgt (GcmProfile *profile)
{
	g_return_val_if_fail (GCM_IS_PROFILE (profile), FALSE);
	return profile->priv->has_vcgt;
}

/**
 * gcm_profile_set_has_vcgt:
 **/
void
gcm_profile_set_has_vcgt (GcmProfile *profile, gboolean has_vcgt)
{
	g_return_if_fail (GCM_IS_PROFILE (profile));
	profile->priv->has_vcgt = has_vcgt;
	g_object_notify (G_OBJECT (profile), "has_vcgt");
}

/**
 * gcm_profile_get_can_delete:
 **/
gboolean
gcm_profile_get_can_delete (GcmProfile *profile)
{
	g_return_val_if_fail (GCM_IS_PROFILE (profile), FALSE);
	return profile->priv->can_delete;
}

/**
 * gcm_profile_parse_data:
 **/
gboolean
gcm_profile_parse_data (GcmProfile *profile, const guint8 *data, gsize length, GError **error)
{
	gboolean ret = FALSE;
	gchar *checksum = NULL;
	guint num_tags;
	guint i;
	guint tag_id;
	guint offset;
	guint tag_size;
	guint tag_offset;
	icProfileClassSignature profile_class;
	icColorSpaceSignature color_space;
	GcmColorspace colorspace;
	GcmProfileKind profile_kind;
	cmsCIEXYZ cie_xyz;
	cmsCIEXYZTRIPLE cie_illum;
	struct tm created;
	cmsHPROFILE xyz_profile;
	cmsHTRANSFORM transform;
	gchar *text;
	GcmXyz *xyz;
	GcmProfilePrivate *priv = profile->priv;

	g_return_val_if_fail (GCM_IS_PROFILE (profile), FALSE);
	g_return_val_if_fail (data != NULL, FALSE);
	g_return_val_if_fail (priv->loaded == FALSE, FALSE);

	/* save the length */
	priv->size = length;
	priv->loaded = TRUE;

	/* load profile into lcms */
	priv->lcms_profile = cmsOpenProfileFromMem ((LPVOID)data, length);
	if (priv->lcms_profile == NULL) {
		g_set_error_literal (error, 1, 0, "failed to load: not an ICC profile");
		goto out;
	}

	/* get white point */
	ret = cmsTakeMediaWhitePoint (&cie_xyz, priv->lcms_profile);
	if (ret) {
		xyz = gcm_xyz_new ();
		g_object_set (xyz,
			      "cie-x", cie_xyz.X,
			      "cie-y", cie_xyz.Y,
			      "cie-z", cie_xyz.Z,
			      NULL);
		g_object_set (profile,
			      "white", xyz,
			      NULL);
		g_object_unref (xyz);
	} else {
		egg_warning ("failed to get white point");
	}

	/* get black point */
	ret = cmsTakeMediaBlackPoint (&cie_xyz, priv->lcms_profile);
	if (ret) {
		xyz = gcm_xyz_new ();
		g_object_set (xyz,
			      "cie-x", cie_xyz.X,
			      "cie-y", cie_xyz.Y,
			      "cie-z", cie_xyz.Z,
			      NULL);
		g_object_set (profile,
			      "black", xyz,
			      NULL);
		g_object_unref (xyz);
	} else {
		egg_warning ("failed to get black point");
	}

	/* get the profile kind */
	profile_class = cmsGetDeviceClass (priv->lcms_profile);
	switch (profile_class) {
	case icSigInputClass:
		profile_kind = GCM_PROFILE_KIND_INPUT_DEVICE;
		break;
	case icSigDisplayClass:
		profile_kind = GCM_PROFILE_KIND_DISPLAY_DEVICE;
		break;
	case icSigOutputClass:
		profile_kind = GCM_PROFILE_KIND_OUTPUT_DEVICE;
		break;
	case icSigLinkClass:
		profile_kind = GCM_PROFILE_KIND_DEVICELINK;
		break;
	case icSigColorSpaceClass:
		profile_kind = GCM_PROFILE_KIND_COLORSPACE_CONVERSION;
		break;
	case icSigAbstractClass:
		profile_kind = GCM_PROFILE_KIND_ABSTRACT;
		break;
	case icSigNamedColorClass:
		profile_kind = GCM_PROFILE_KIND_NAMED_COLOR;
		break;
	default:
		profile_kind = GCM_PROFILE_KIND_UNKNOWN;
	}
	gcm_profile_set_kind (profile, profile_kind);

	/* get colorspace */
	color_space = cmsGetColorSpace (priv->lcms_profile);
	switch (color_space) {
	case icSigXYZData:
		colorspace = GCM_COLORSPACE_XYZ;
		break;
	case icSigLabData:
		colorspace = GCM_COLORSPACE_LAB;
		break;
	case icSigLuvData:
		colorspace = GCM_COLORSPACE_LUV;
		break;
	case icSigYCbCrData:
		colorspace = GCM_COLORSPACE_YCBCR;
		break;
	case icSigYxyData:
		colorspace = GCM_COLORSPACE_YXY;
		break;
	case icSigRgbData:
		colorspace = GCM_COLORSPACE_RGB;
		break;
	case icSigGrayData:
		colorspace = GCM_COLORSPACE_GRAY;
		break;
	case icSigHsvData:
		colorspace = GCM_COLORSPACE_HSV;
		break;
	case icSigCmykData:
		colorspace = GCM_COLORSPACE_CMYK;
		break;
	case icSigCmyData:
		colorspace = GCM_COLORSPACE_CMY;
		break;
	default:
		colorspace = GCM_COLORSPACE_UNKNOWN;
	}
	gcm_profile_set_colorspace (profile, colorspace);

	/* get primary illuminants */
	ret = cmsTakeColorants (&cie_illum, priv->lcms_profile);

	/* geting the illuminants failed, try running it through the profile */
	if (!ret && color_space == icSigRgbData) {
		gdouble rgb_values[3];

		/* create a transform from profile to XYZ */
		xyz_profile = cmsCreateXYZProfile ();
		transform = cmsCreateTransform (priv->lcms_profile, TYPE_RGB_DBL, xyz_profile, TYPE_XYZ_DBL, INTENT_PERCEPTUAL, 0);
		if (transform != NULL) {

			/* red */
			rgb_values[0] = 1.0;
			rgb_values[1] = 0.0;
			rgb_values[2] = 0.0;
			cmsDoTransform (transform, rgb_values, &cie_illum.Red, 1);

			/* green */
			rgb_values[0] = 0.0;
			rgb_values[1] = 1.0;
			rgb_values[2] = 0.0;
			cmsDoTransform (transform, rgb_values, &cie_illum.Green, 1);

			/* blue */
			rgb_values[0] = 0.0;
			rgb_values[1] = 0.0;
			rgb_values[2] = 1.0;
			cmsDoTransform (transform, rgb_values, &cie_illum.Blue, 1);

			/* we're done */
			cmsDeleteTransform (transform);
			ret = TRUE;
		}

		/* no more need for the output profile */
		cmsCloseProfile (xyz_profile);
	}

	/* we've got valid values */
	if (ret) {
		/* red */
		xyz = gcm_xyz_new ();
		g_object_set (xyz,
			      "cie-x", cie_illum.Red.X,
			      "cie-y", cie_illum.Red.Y,
			      "cie-z", cie_illum.Red.Z,
			      NULL);
		g_object_set (profile,
			      "red", xyz,
			      NULL);
		g_object_unref (xyz);

		/* green */
		xyz = gcm_xyz_new ();
		g_object_set (xyz,
			      "cie-x", cie_illum.Green.X,
			      "cie-y", cie_illum.Green.Y,
			      "cie-z", cie_illum.Green.Z,
			      NULL);
		g_object_set (profile,
			      "green", xyz,
			      NULL);
		g_object_unref (xyz);

		/* blue */
		xyz = gcm_xyz_new ();
		g_object_set (xyz,
			      "cie-x", cie_illum.Blue.X,
			      "cie-y", cie_illum.Blue.Y,
			      "cie-z", cie_illum.Blue.Z,
			      NULL);
		g_object_set (profile,
			      "blue", xyz,
			      NULL);
		g_object_unref (xyz);
	} else {
		egg_debug ("failed to get luminance values");
	}

	/* get the profile created time and date */
	ret = cmsTakeCreationDateTime (&created, priv->lcms_profile);
	if (ret) {
		text = gcm_utils_format_date_time (&created);
		gcm_profile_set_datetime (profile, text);
		g_free (text);
	}

	/* get the number of tags in the file */
	num_tags = gcm_parser_decode_32 (data + GCM_NUMTAGS);
	for (i=0; i<num_tags; i++) {
		offset = GCM_TAG_WIDTH * i;
		tag_id = gcm_parser_decode_32 (data + GCM_BODY + offset + GCM_TAG_ID);
		tag_offset = gcm_parser_decode_32 (data + GCM_BODY + offset + GCM_TAG_OFFSET);
		tag_size = gcm_parser_decode_32 (data + GCM_BODY + offset + GCM_TAG_SIZE);

		/* print tag */
//		egg_debug ("tag %x is present at 0x%x with size %u", tag_id, tag_offset, tag_size);

		if (tag_id == icSigProfileDescriptionTag) {
			text = gcm_profile_parse_multi_localized_unicode (profile, data + tag_offset, tag_size);
			gcm_profile_set_description (profile, text);
			g_free (text);
		}
		if (tag_id == icSigCopyrightTag) {
			text = gcm_profile_parse_multi_localized_unicode (profile, data + tag_offset, tag_size);
			gcm_profile_set_copyright (profile, text);
			g_free (text);
		}
		if (tag_id == icSigDeviceMfgDescTag) {
			text = gcm_profile_parse_multi_localized_unicode (profile, data + tag_offset, tag_size);
			gcm_profile_set_manufacturer (profile, text);
			g_free (text);
		}
		if (tag_id == icSigDeviceModelDescTag) {
			text = gcm_profile_parse_multi_localized_unicode (profile, data + tag_offset, tag_size);
			gcm_profile_set_model (profile, text);
			g_free (text);
		}
		if (tag_id == icSigMachineLookUpTableTag) {
			ret = gcm_parser_load_icc_mlut (profile, data + tag_offset, tag_size);
			if (!ret) {
				g_set_error_literal (error, 1, 0, "failed to load mlut");
				goto out;
			}
		}
		if (tag_id == icSigVideoCartGammaTableTag) {
			if (tag_size == 1584)
				priv->adobe_gamma_workaround = TRUE;
			ret = gcm_parser_load_icc_vcgt (profile, data + tag_offset, tag_size);
			if (!ret) {
				g_set_error_literal (error, 1, 0, "failed to load vcgt");
				goto out;
			}
		}
	}

	/* success */
	ret = TRUE;

	/* set properties */
	gcm_profile_set_has_vcgt (profile, priv->has_vcgt_formula || priv->has_vcgt_table);

	/* generate and set checksum */
	checksum = g_compute_checksum_for_data (G_CHECKSUM_MD5, (const guchar *) data, length);
	gcm_profile_set_checksum (profile, checksum);
out:
	g_free (checksum);
	return ret;
}

/**
 * gcm_profile_parse:
 **/
gboolean
gcm_profile_parse (GcmProfile *profile, GFile *file, GError **error)
{
	gchar *data = NULL;
	gboolean ret = FALSE;
	gsize length;
	gchar *filename = NULL;
	GError *error_local = NULL;
	GFileInfo *info;

	g_return_val_if_fail (GCM_IS_PROFILE (profile), FALSE);
	g_return_val_if_fail (file != NULL, FALSE);

	/* find out if the user could delete this profile */
	info = g_file_query_info (file, G_FILE_ATTRIBUTE_ACCESS_CAN_DELETE,
				  G_FILE_QUERY_INFO_NONE, NULL, error);
	if (info == NULL)
		goto out;
	profile->priv->can_delete = g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_DELETE);

	/* load files */
	ret = g_file_load_contents (file, NULL, &data, &length, NULL, &error_local);
	if (!ret) {
		g_set_error (error, 1, 0, "failed to load profile: %s", error_local->message);
		g_error_free (error_local);
		goto out;
	}

	/* parse the data */
	ret = gcm_profile_parse_data (profile, (const guint8*)data, length, error);
	if (!ret)
		goto out;

	/* save */
	filename = g_file_get_path (file);
	gcm_profile_set_filename (profile, filename);
out:
	if (info != NULL)
		g_object_unref (info);
	g_free (filename);
	g_free (data);
	return ret;
}

/**
 * gcm_profile_save:
 **/
gboolean
gcm_profile_save (GcmProfile *profile, const gchar *filename, GError **error)
{
	gboolean ret = FALSE;
	GcmProfilePrivate *priv = profile->priv;

	/* not loaded */
	if (priv->size == 0) {
		g_set_error_literal (error, 1, 0, "not loaded");
		goto out;
	}

	/* save, TODO: get error */
	_cmsSaveProfile (priv->lcms_profile, filename);
	ret = TRUE;
out:
	return ret;
}

/**
 * gcm_profile_generate_vcgt:
 *
 * Free with g_object_unref();
 **/
GcmClut *
gcm_profile_generate_vcgt (GcmProfile *profile, guint size)
{
	/* proxy */
	guint i;
	guint ratio;
	GcmClutData *tmp;
	GcmClutData *vcgt_data;
	GcmClutData *mlut_data;
	gfloat gamma_red, min_red, max_red;
	gfloat gamma_green, min_green, max_green;
	gfloat gamma_blue, min_blue, max_blue;
	guint num_entries;
	GcmClut *clut = NULL;
	GPtrArray *array = NULL;
	gfloat inverse_ratio;
	guint idx;
	gfloat frac;

	g_return_val_if_fail (GCM_IS_PROFILE (profile), NULL);
	g_return_val_if_fail (size != 0, FALSE);

	/* reduce dereferences */
	vcgt_data = profile->priv->vcgt_data;
	mlut_data = profile->priv->mlut_data;

	if (profile->priv->has_vcgt_table) {

		/* create array */
		array = g_ptr_array_new_with_free_func (g_free);

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

		/* create array */
		array = g_ptr_array_new_with_free_func (g_free);

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

		/* create array */
		array = g_ptr_array_new_with_free_func (g_free);

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

	/* bugger */
	egg_debug ("no LUT to generate");
out:
	if (array != NULL) {
		/* create new output array */
		clut = gcm_clut_new ();
		gcm_clut_set_source_array (clut, array);
		g_ptr_array_unref (array);
	}
	return clut;
}

/**
 * gcm_profile_generate_curve:
 *
 * Free with g_object_unref();
 **/
GcmClut *
gcm_profile_generate_curve (GcmProfile *profile, guint size)
{
	GcmClut *clut = NULL;
	gdouble *values_in = NULL;
	gdouble *values_out = NULL;
	guint i;
	GcmClutData *data;
	GPtrArray *array = NULL;
	gfloat divamount;
	gfloat divadd;
	guint component_width;
	cmsHPROFILE srgb_profile = NULL;
	cmsHTRANSFORM transform = NULL;
	guint type;
	GcmColorspace colorspace;
	GcmProfilePrivate *priv = profile->priv;

	/* run through the profile */
	colorspace = gcm_profile_get_colorspace (profile);
	if (colorspace == GCM_COLORSPACE_RGB) {

		/* RGB */
		component_width = 3;
		type = TYPE_RGB_DBL;

		/* create input array */
		values_in = g_new0 (gdouble, size * 3 * component_width);
		divamount = 1.0f / (gfloat) (size - 1);
		for (i=0; i<size; i++) {
			divadd = divamount * (gfloat) i;

			/* red component */
			values_in[(i * 3 * component_width)+0] = divadd;
			values_in[(i * 3 * component_width)+1] = 0.0f;
			values_in[(i * 3 * component_width)+2] = 0.0f;

			/* green component */
			values_in[(i * 3 * component_width)+3] = 0.0f;
			values_in[(i * 3 * component_width)+4] = divadd;
			values_in[(i * 3 * component_width)+5] = 0.0f;

			/* blue component */
			values_in[(i * 3 * component_width)+6] = 0.0f;
			values_in[(i * 3 * component_width)+7] = 0.0f;
			values_in[(i * 3 * component_width)+8] = divadd;
		}
	}

	/* do each transform */
	if (values_in != NULL) {
		/* create output array */
		values_out = g_new0 (gdouble, size * 3 * component_width);

		/* create a transform from profile to sRGB */
		srgb_profile = cmsCreate_sRGBProfile ();
		transform = cmsCreateTransform (priv->lcms_profile, type, srgb_profile, TYPE_RGB_DBL, INTENT_PERCEPTUAL, 0);
		if (transform == NULL)
			goto out;

		/* do transform */
		cmsDoTransform (transform, values_in, values_out, size * 3);

		/* create output array */
		array = g_ptr_array_new_with_free_func (g_free);

		for (i=0; i<size; i++) {
			data = g_new0 (GcmClutData, 1);

			data->red = values_out[(i * 3 * component_width)+0] * (gfloat) 0xffff;
			data->green = values_out[(i * 3 * component_width)+4] * (gfloat) 0xffff;
			data->blue = values_out[(i * 3 * component_width)+8] * (gfloat) 0xffff;
			g_ptr_array_add (array, data);
		}
		clut = gcm_clut_new ();
		gcm_clut_set_source_array (clut, array);
	}

out:
	g_free (values_in);
	g_free (values_out);
	if (array != NULL)
		g_ptr_array_unref (array);
	if (transform != NULL)
		cmsDeleteTransform (transform);
	if (srgb_profile != NULL)
		cmsCloseProfile (srgb_profile);
	return clut;
}

/**
 * gcm_profile_file_monitor_changed_cb:
 **/
static void
gcm_profile_file_monitor_changed_cb (GFileMonitor *monitor, GFile *file, GFile *other_file, GFileMonitorEvent event_type, GcmProfile *profile)
{
	GcmProfilePrivate *priv = profile->priv;

	/* ony care about deleted events */
	if (event_type != G_FILE_MONITOR_EVENT_DELETED)
		goto out;

	/* just rescan everything */
	egg_debug ("%s deleted, clearing filename", priv->filename);
	gcm_profile_set_filename (profile, NULL);
out:
	return;
}


/**
 * gcm_profile_lcms_error_cb:
 **/
static int
gcm_profile_lcms_error_cb (int ErrorCode, const char *ErrorText)
{
	egg_warning ("LCMS error %i: %s", ErrorCode, ErrorText);
	return LCMS_ERRC_WARNING;
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
	case PROP_CHECKSUM:
		g_value_set_string (value, priv->checksum);
		break;
	case PROP_DESCRIPTION:
		g_value_set_string (value, priv->description);
		break;
	case PROP_FILENAME:
		g_value_set_string (value, priv->filename);
		break;
	case PROP_KIND:
		g_value_set_uint (value, priv->kind);
		break;
	case PROP_COLORSPACE:
		g_value_set_uint (value, priv->colorspace);
		break;
	case PROP_SIZE:
		g_value_set_uint (value, priv->size);
		break;
	case PROP_HAS_VCGT:
		g_value_set_boolean (value, priv->has_vcgt);
		break;
	case PROP_CAN_DELETE:
		g_value_set_boolean (value, priv->can_delete);
		break;
	case PROP_WHITE:
		g_value_set_object (value, priv->white);
		break;
	case PROP_BLACK:
		g_value_set_object (value, priv->black);
		break;
	case PROP_RED:
		g_value_set_object (value, priv->red);
		break;
	case PROP_GREEN:
		g_value_set_object (value, priv->green);
		break;
	case PROP_BLUE:
		g_value_set_object (value, priv->blue);
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
		gcm_profile_set_copyright (profile, g_value_get_string (value));
		break;
	case PROP_MANUFACTURER:
		gcm_profile_set_manufacturer (profile, g_value_get_string (value));
		break;
	case PROP_MODEL:
		gcm_profile_set_model (profile, g_value_get_string (value));
		break;
	case PROP_DATETIME:
		gcm_profile_set_datetime (profile, g_value_get_string (value));
		break;
	case PROP_DESCRIPTION:
		gcm_profile_set_description (profile, g_value_get_string (value));
		break;
	case PROP_FILENAME:
		gcm_profile_set_filename (profile, g_value_get_string (value));
		break;
	case PROP_KIND:
		gcm_profile_set_kind (profile, g_value_get_uint (value));
		break;
	case PROP_COLORSPACE:
		gcm_profile_set_colorspace (profile, g_value_get_uint (value));
		break;
	case PROP_SIZE:
		gcm_profile_set_size (profile, g_value_get_uint (value));
		break;
	case PROP_HAS_VCGT:
		gcm_profile_set_has_vcgt (profile, g_value_get_boolean (value));
		break;
	case PROP_WHITE:
		priv->white = g_value_dup_object (value);
		break;
	case PROP_BLACK:
		priv->black = g_value_dup_object (value);
		break;
	case PROP_RED:
		priv->red = g_value_dup_object (value);
		break;
	case PROP_GREEN:
		priv->green = g_value_dup_object (value);
		break;
	case PROP_BLUE:
		priv->blue = g_value_dup_object (value);
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
	 * GcmProfile:manufacturer:
	 */
	pspec = g_param_spec_string ("manufacturer", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_MANUFACTURER, pspec);

	/**
	 * GcmProfile:model:
	 */
	pspec = g_param_spec_string ("model", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_MODEL, pspec);

	/**
	 * GcmProfile:datetime:
	 */
	pspec = g_param_spec_string ("datetime", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_DATETIME, pspec);

	/**
	 * GcmProfile:checksum:
	 */
	pspec = g_param_spec_string ("checksum", NULL, NULL,
				     NULL,
				     G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_CHECKSUM, pspec);

	/**
	 * GcmProfile:description:
	 */
	pspec = g_param_spec_string ("description", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_DESCRIPTION, pspec);

	/**
	 * GcmProfile:filename:
	 */
	pspec = g_param_spec_string ("filename", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_FILENAME, pspec);

	/**
	 * GcmProfile:kind:
	 */
	pspec = g_param_spec_uint ("kind", NULL, NULL,
				   0, G_MAXUINT, 0,
				   G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_KIND, pspec);

	/**
	 * GcmProfile:colorspace:
	 */
	pspec = g_param_spec_uint ("colorspace", NULL, NULL,
				   0, G_MAXUINT, 0,
				   G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_COLORSPACE, pspec);

	/**
	 * GcmProfile:size:
	 */
	pspec = g_param_spec_uint ("size", NULL, NULL,
				   0, G_MAXUINT, 0,
				   G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_SIZE, pspec);

	/**
	 * GcmProfile:has-vcgt:
	 */
	pspec = g_param_spec_boolean ("has-vcgt", NULL, NULL,
				      FALSE,
				      G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_HAS_VCGT, pspec);

	/**
	 * GcmProfile:can-delete:
	 */
	pspec = g_param_spec_boolean ("can-delete", NULL, NULL,
				      FALSE,
				      G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_CAN_DELETE, pspec);

	/**
	 * GcmProfile:white:
	 */
	pspec = g_param_spec_object ("white", NULL, NULL,
				     GCM_TYPE_XYZ,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_WHITE, pspec);

	/**
	 * GcmProfile:black:
	 */
	pspec = g_param_spec_object ("black", NULL, NULL,
				     GCM_TYPE_XYZ,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_BLACK, pspec);

	/**
	 * GcmProfile:red:
	 */
	pspec = g_param_spec_object ("red", NULL, NULL,
				     GCM_TYPE_XYZ,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_RED, pspec);

	/**
	 * GcmProfile:green:
	 */
	pspec = g_param_spec_object ("green", NULL, NULL,
				     GCM_TYPE_XYZ,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_GREEN, pspec);

	/**
	 * GcmProfile:blue:
	 */
	pspec = g_param_spec_object ("blue", NULL, NULL,
				     GCM_TYPE_XYZ,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_BLUE, pspec);

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
	profile->priv->adobe_gamma_workaround = FALSE;
	profile->priv->can_delete = FALSE;
	profile->priv->monitor = NULL;
	profile->priv->kind = GCM_PROFILE_KIND_UNKNOWN;
	profile->priv->colorspace = GCM_COLORSPACE_UNKNOWN;
	profile->priv->white = gcm_xyz_new ();
	profile->priv->black = gcm_xyz_new ();
	profile->priv->red = gcm_xyz_new ();
	profile->priv->green = gcm_xyz_new ();
	profile->priv->blue = gcm_xyz_new ();

	/* setup LCMS */
	cmsSetErrorHandler (gcm_profile_lcms_error_cb);
	cmsErrorAction (LCMS_ERROR_SHOW);
	cmsSetLanguage ("en", "US");
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
	g_free (priv->checksum);
	g_free (priv->vcgt_data);
	g_free (priv->mlut_data);
	g_object_unref (priv->white);
	g_object_unref (priv->black);
	g_object_unref (priv->red);
	g_object_unref (priv->green);
	g_object_unref (priv->blue);
	if (priv->monitor != NULL)
		g_object_unref (priv->monitor);

	if (priv->lcms_profile != NULL)
		cmsCloseProfile (priv->lcms_profile);

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

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
#include <lcms2.h>

#include "egg-debug.h"

#include "gcm-profile.h"
#include "gcm-color.h"

static void     gcm_profile_finalize	(GObject     *object);

#define GCM_PROFILE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GCM_TYPE_PROFILE, GcmProfilePrivate))

/**
 * GcmProfilePrivate:
 *
 * Private #GcmProfile data
 **/
struct _GcmProfilePrivate
{
	gboolean		 loaded;
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
	guint			 temperature;
	GcmColorXYZ		*white;
	GcmColorXYZ		*black;
	GcmColorXYZ		*red;
	GcmColorXYZ		*green;
	GcmColorXYZ		*blue;
	GFile			*file;
	GFileMonitor		*monitor;
	gboolean		 has_mlut;
	cmsHPROFILE		 lcms_profile;
};

enum {
	PROP_0,
	PROP_COPYRIGHT,
	PROP_MANUFACTURER,
	PROP_MODEL,
	PROP_DATETIME,
	PROP_CHECKSUM,
	PROP_DESCRIPTION,
	PROP_FILE,
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
	PROP_TEMPERATURE,
	PROP_LAST
};

G_DEFINE_TYPE (GcmProfile, gcm_profile, G_TYPE_OBJECT)

static void gcm_profile_file_monitor_changed_cb (GFileMonitor *monitor, GFile *file, GFile *other_file, GFileMonitorEvent event_type, GcmProfile *profile);

/**
 * gcm_profile_get_description:
 * @profile: A valid #GcmProfile
 *
 * Gets the profile description.
 *
 * Return value: The profile description as a string.
 *
 * Since: 0.0.1
 **/
const gchar *
gcm_profile_get_description (GcmProfile *profile)
{
	g_return_val_if_fail (GCM_IS_PROFILE (profile), NULL);
	return profile->priv->description;
}


/**
 * gcm_profile_ensure_printable:
 **/
static void
gcm_profile_ensure_printable (gchar *text)
{
	guint i;
	guint idx = 0;

	g_return_if_fail (text != NULL);

	for (i=0; text[i] != '\0'; i++) {
		if (g_ascii_isalnum (text[i]) ||
		    g_ascii_ispunct (text[i]) ||
		    text[i] == ' ')
			text[idx++] = text[i];
	}
	text[idx] = '\0';

	/* broken profiles have _ instead of spaces */
	g_strdelimit (text, "_", ' ');
}

/**
 * gcm_profile_set_description:
 * @profile: A valid #GcmProfile
 * @description: the data location to read into
 *
 * Sets the description of the profile.
 *
 * Since: 0.0.1
 **/
void
gcm_profile_set_description (GcmProfile *profile, const gchar *description)
{
	GcmProfilePrivate *priv = profile->priv;
	g_return_if_fail (GCM_IS_PROFILE (profile));

	g_free (priv->description);
	priv->description = g_strdup (description);

	if (priv->description != NULL)
		gcm_profile_ensure_printable (priv->description);

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
 * gcm_profile_has_colorspace_description:
 * @profile: A valid #GcmProfile
 *
 * Finds out if the profile contains a colorspace description.
 *
 * Return value: %TRUE if the description mentions the profile colorspace explicity,
 * e.g. "Adobe RGB" for %GCM_COLORSPACE_RGB.
 *
 * Since: 0.0.1
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
 * gcm_profile_get_temperature:
 * @profile: A valid #GcmProfile
 *
 * Gets the profile color temperature, rounded to the nearest 100K.
 *
 * Return value: The color temperature in Kelvins, or 0 for error.
 *
 * Since: 0.0.1
 **/
guint
gcm_profile_get_temperature (GcmProfile *profile)
{
	g_return_val_if_fail (GCM_IS_PROFILE (profile), 0);
	return profile->priv->temperature;
}

/**
 * gcm_profile_get_file:
 * @profile: A valid #GcmProfile
 *
 * Gets the file attached to this profile.
 *
 * Return value: A #GFile, or %NULL. Do not free.
 *
 * Since: 0.0.1
 **/
GFile *
gcm_profile_get_file (GcmProfile *profile)
{
	g_return_val_if_fail (GCM_IS_PROFILE (profile), NULL);
	return profile->priv->file;
}

/**
 * gcm_profile_set_file:
 * @profile: A valid #GcmProfile
 * @file: A #GFile to read
 *
 * Sets the file to be used when reading the profile.
 *
 * Since: 0.0.1
 **/
void
gcm_profile_set_file (GcmProfile *profile, GFile *file)
{
	GcmProfilePrivate *priv = profile->priv;

	g_return_if_fail (GCM_IS_PROFILE (profile));
	g_return_if_fail (G_IS_FILE (file));

	/* unref old instance */
	if (priv->file != NULL)
		g_object_unref (priv->file);
	if (priv->monitor != NULL)
		g_object_unref (priv->monitor);
	g_free (priv->filename);

	/* setup watch on new profile */
	priv->file = g_object_ref (file);
	priv->filename = g_file_get_path (file);
	priv->monitor = g_file_monitor_file (file, G_FILE_MONITOR_NONE, NULL, NULL);
	if (priv->monitor != NULL)
		g_signal_connect (priv->monitor, "changed", G_CALLBACK(gcm_profile_file_monitor_changed_cb), profile);

	/* notify we've changed */
	g_object_notify (G_OBJECT (profile), "file");
}

/**
 * gcm_profile_get_filename:
 * @profile: A valid #GcmProfile
 *
 * Gets the filename of the profile data, if one exists.
 *
 * Return value: A filename, or %NULL
 *
 * Since: 0.0.1
 **/
const gchar *
gcm_profile_get_filename (GcmProfile *profile)
{
	GcmProfilePrivate *priv = profile->priv;

	g_return_val_if_fail (GCM_IS_PROFILE (profile), NULL);

	/* this returns a const, so we have to track it internally */
	if (priv->filename == NULL && priv->file != NULL)
		priv->filename = g_file_get_path (priv->file);
	return priv->filename;
}

/**
 * gcm_profile_get_copyright:
 * @profile: A valid #GcmProfile
 *
 * Gets the copyright string for this profile.
 *
 * Return value: A string. Do not free.
 *
 * Since: 0.0.1
 **/
const gchar *
gcm_profile_get_copyright (GcmProfile *profile)
{
	g_return_val_if_fail (GCM_IS_PROFILE (profile), NULL);
	return profile->priv->copyright;
}

/**
 * gcm_profile_set_copyright:
 * @profile: A valid #GcmProfile
 * @copyright: the copyright string
 *
 * Sets the copyright string.
 *
 * Since: 0.0.1
 **/
void
gcm_profile_set_copyright (GcmProfile *profile, const gchar *copyright)
{
	GcmProfilePrivate *priv = profile->priv;

	g_return_if_fail (GCM_IS_PROFILE (profile));

	g_free (priv->copyright);
	priv->copyright = g_strdup (copyright);
	if (priv->copyright != NULL)
		gcm_profile_ensure_printable (priv->copyright);
	g_object_notify (G_OBJECT (profile), "copyright");
}

/**
 * gcm_profile_get_model:
 * @profile: A valid #GcmProfile
 *
 * Gets the device model name.
 *
 * Return value: A string. Do not free.
 *
 * Since: 0.0.1
 **/
const gchar *
gcm_profile_get_model (GcmProfile *profile)
{
	g_return_val_if_fail (GCM_IS_PROFILE (profile), NULL);
	return profile->priv->model;
}

/**
 * gcm_profile_set_model:
 * @profile: A valid #GcmProfile
 * @model: the profile model.
 *
 * Sets the device model name.
 *
 * Since: 0.0.1
 **/
void
gcm_profile_set_model (GcmProfile *profile, const gchar *model)
{
	GcmProfilePrivate *priv = profile->priv;

	g_return_if_fail (GCM_IS_PROFILE (profile));

	g_free (priv->model);
	priv->model = g_strdup (model);
	if (priv->model != NULL)
		gcm_profile_ensure_printable (priv->model);
	g_object_notify (G_OBJECT (profile), "model");
}

/**
 * gcm_profile_get_manufacturer:
 * @profile: A valid #GcmProfile
 *
 * Gets the device manufacturer name.
 *
 * Return value: A string. Do not free.
 *
 * Since: 0.0.1
 **/
const gchar *
gcm_profile_get_manufacturer (GcmProfile *profile)
{
	g_return_val_if_fail (GCM_IS_PROFILE (profile), NULL);
	return profile->priv->manufacturer;
}

/**
 * gcm_profile_set_manufacturer:
 * @profile: A valid #GcmProfile
 * @manufacturer: the profile manufacturer.
 *
 * Sets the device manufacturer name.
 *
 * Since: 0.0.1
 **/
void
gcm_profile_set_manufacturer (GcmProfile *profile, const gchar *manufacturer)
{
	GcmProfilePrivate *priv = profile->priv;

	g_return_if_fail (GCM_IS_PROFILE (profile));

	g_free (priv->manufacturer);
	priv->manufacturer = g_strdup (manufacturer);
	if (priv->manufacturer != NULL)
		gcm_profile_ensure_printable (priv->manufacturer);
	g_object_notify (G_OBJECT (profile), "manufacturer");
}

/**
 * gcm_profile_get_datetime:
 * @profile: A valid #GcmProfile
 *
 * Gets the profile date and time.
 *
 * Return value: A string. Do not free.
 *
 * Since: 0.0.1
 **/
const gchar *
gcm_profile_get_datetime (GcmProfile *profile)
{
	g_return_val_if_fail (GCM_IS_PROFILE (profile), NULL);
	return profile->priv->datetime;
}

/**
 * gcm_profile_set_datetime:
 * @profile: A valid #GcmProfile
 * @datetime: the profile date time.
 *
 * Sets the profile date and time.
 *
 * Since: 0.0.1
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
static void
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
static void
gcm_profile_set_has_vcgt (GcmProfile *profile, gboolean has_vcgt)
{
	g_return_if_fail (GCM_IS_PROFILE (profile));
	profile->priv->has_vcgt = has_vcgt;
	g_object_notify (G_OBJECT (profile), "has_vcgt");
}

/**
 * gcm_profile_get_handle:
 *
 * Return value: Do not call cmsCloseProfile() on this value!
 **/
gpointer
gcm_profile_get_handle (GcmProfile *profile)
{
	g_return_val_if_fail (GCM_IS_PROFILE (profile), NULL);
	return profile->priv->lcms_profile;
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
 * gcm_utils_format_date_time:
 **/
static gchar *
gcm_utils_format_date_time (const struct tm *created)
{
	gchar buffer[256];

	/* TRANSLATORS: this is the profile creation date strftime format */
	strftime (buffer, sizeof(buffer), _("%B %e %Y, %I:%M:%S %p"), created);

	return g_strdup (g_strchug (buffer));
}

/**
 * gcm_profile_parse_data:
 * @profile: A valid #GcmProfile
 * @data: the data to parse
 * @length: the length of @data
 * @error: A #GError, or %NULL
 *
 * Parses profile data, filling in all the details possible.
 *
 * Return value: %TRUE for success
 *
 * Since: 0.0.1
 **/
gboolean
gcm_profile_parse_data (GcmProfile *profile, const guint8 *data, gsize length, GError **error)
{
	gboolean ret = FALSE;
	cmsProfileClassSignature profile_class;
	cmsColorSpaceSignature color_space;
	GcmColorspace colorspace;
	GcmProfileKind profile_kind;
	cmsCIEXYZ *cie_xyz;
	cmsCIEXYZTRIPLE cie_illum;
	struct tm created;
	cmsHPROFILE xyz_profile;
	cmsHTRANSFORM transform;
	gchar *text = NULL;
	gchar *checksum = NULL;
	GcmProfilePrivate *priv = profile->priv;

	g_return_val_if_fail (GCM_IS_PROFILE (profile), FALSE);
	g_return_val_if_fail (data != NULL, FALSE);
	g_return_val_if_fail (priv->loaded == FALSE, FALSE);

	/* save the length */
	priv->size = length;
	priv->loaded = TRUE;

	/* ensure we have the header */
	if (length < 0x84) {
		g_set_error (error, 1, 0, "profile was not valid (file size too small)");
		goto out;
	}

	/* load profile into lcms */
	priv->lcms_profile = cmsOpenProfileFromMem (data, length);
	if (priv->lcms_profile == NULL) {
		g_set_error_literal (error, 1, 0, "failed to load: not an ICC profile");
		goto out;
	}

	/* get white point */
	cie_xyz = cmsReadTag (priv->lcms_profile, cmsSigMediaWhitePointTag);
	if (cie_xyz != NULL) {
		GcmColorYxy yxy;
		cmsCIExyY xyY;
		gdouble temp_float;
		gcm_color_set_XYZ (priv->white,
				   cie_xyz->X, cie_xyz->Y, cie_xyz->Z);

		/* convert to lcms xyY values */
		gcm_color_convert_XYZ_to_Yxy (priv->white, &yxy);
		xyY.x = yxy.x;
		xyY.y = yxy.y;
		xyY.Y = yxy.Y;
		cmsTempFromWhitePoint (&temp_float, &xyY);

		/* round to nearest 100K */
		priv->temperature = (((guint) temp_float) / 100) * 100;

	} else {
		gcm_color_clear_XYZ (priv->white);
		egg_warning ("failed to get white point");
	}

	/* get black point */
	cie_xyz = cmsReadTag (priv->lcms_profile, cmsSigMediaBlackPointTag);
	if (cie_xyz != NULL) {
		gcm_color_set_XYZ (priv->black,
				   cie_xyz->X, cie_xyz->Y, cie_xyz->Z);
	} else {
		gcm_color_clear_XYZ (priv->black);
		egg_warning ("failed to get black point");
	}

	/* get the profile kind */
	profile_class = cmsGetDeviceClass (priv->lcms_profile);
	switch (profile_class) {
	case cmsSigInputClass:
		profile_kind = GCM_PROFILE_KIND_INPUT_DEVICE;
		break;
	case cmsSigDisplayClass:
		profile_kind = GCM_PROFILE_KIND_DISPLAY_DEVICE;
		break;
	case cmsSigOutputClass:
		profile_kind = GCM_PROFILE_KIND_OUTPUT_DEVICE;
		break;
	case cmsSigLinkClass:
		profile_kind = GCM_PROFILE_KIND_DEVICELINK;
		break;
	case cmsSigColorSpaceClass:
		profile_kind = GCM_PROFILE_KIND_COLORSPACE_CONVERSION;
		break;
	case cmsSigAbstractClass:
		profile_kind = GCM_PROFILE_KIND_ABSTRACT;
		break;
	case cmsSigNamedColorClass:
		profile_kind = GCM_PROFILE_KIND_NAMED_COLOR;
		break;
	default:
		profile_kind = GCM_PROFILE_KIND_UNKNOWN;
	}
	gcm_profile_set_kind (profile, profile_kind);

	/* get colorspace */
	color_space = cmsGetColorSpace (priv->lcms_profile);
	switch (color_space) {
	case cmsSigXYZData:
		colorspace = GCM_COLORSPACE_XYZ;
		break;
	case cmsSigLabData:
		colorspace = GCM_COLORSPACE_LAB;
		break;
	case cmsSigLuvData:
		colorspace = GCM_COLORSPACE_LUV;
		break;
	case cmsSigYCbCrData:
		colorspace = GCM_COLORSPACE_YCBCR;
		break;
	case cmsSigYxyData:
		colorspace = GCM_COLORSPACE_YXY;
		break;
	case cmsSigRgbData:
		colorspace = GCM_COLORSPACE_RGB;
		break;
	case cmsSigGrayData:
		colorspace = GCM_COLORSPACE_GRAY;
		break;
	case cmsSigHsvData:
		colorspace = GCM_COLORSPACE_HSV;
		break;
	case cmsSigCmykData:
		colorspace = GCM_COLORSPACE_CMYK;
		break;
	case cmsSigCmyData:
		colorspace = GCM_COLORSPACE_CMY;
		break;
	default:
		colorspace = GCM_COLORSPACE_UNKNOWN;
	}
	gcm_profile_set_colorspace (profile, colorspace);

	/* get the illuminants by running it through the profile */
	if (color_space == cmsSigRgbData) {
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
		gcm_color_set_XYZ (priv->red,
				   cie_illum.Red.X, cie_illum.Red.Y, cie_illum.Red.Z);
		gcm_color_set_XYZ (priv->green,
				   cie_illum.Green.X, cie_illum.Green.Y, cie_illum.Green.Z);
		gcm_color_set_XYZ (priv->blue,
				   cie_illum.Blue.X, cie_illum.Blue.Y, cie_illum.Blue.Z);
	} else {
		egg_debug ("failed to get luminance values");
	}

	/* get the profile created time and date */
	ret = cmsGetHeaderCreationDateTime (priv->lcms_profile, &created);
	if (ret) {
		text = gcm_utils_format_date_time (&created);
		gcm_profile_set_datetime (profile, text);
		g_free (text);
	}

	/* do we have vcgt */
	ret = cmsIsTag (priv->lcms_profile, cmsSigVcgtTag);
	gcm_profile_set_has_vcgt (profile, ret);

	/* allocate temporary buffer */
	text = g_new0 (gchar, 1024);

	/* get description */
	ret = cmsGetProfileInfoASCII (priv->lcms_profile, cmsInfoDescription, "en", "US", text, 1024);
	if (ret)
		gcm_profile_set_description (profile, text);

	/* get copyright */
	ret = cmsGetProfileInfoASCII (priv->lcms_profile, cmsInfoCopyright, "en", "US", text, 1024);
	if (ret)
		gcm_profile_set_copyright (profile, text);

	/* get description */
	ret = cmsGetProfileInfoASCII (priv->lcms_profile, cmsInfoManufacturer, "en", "US", text, 1024);
	if (ret)
		gcm_profile_set_manufacturer (profile, text);

	/* get description */
	ret = cmsGetProfileInfoASCII (priv->lcms_profile, cmsInfoModel, "en", "US", text, 1024);
	if (ret)
		gcm_profile_set_model (profile, text);

	/* success */
	ret = TRUE;

	/* generate and set checksum */
	checksum = g_compute_checksum_for_data (G_CHECKSUM_MD5, (const guchar *) data, length);
	gcm_profile_set_checksum (profile, checksum);
out:
	g_free (text);
	g_free (checksum);
	return ret;
}

/**
 * gcm_profile_parse:
 * @profile: A valid #GcmProfile
 * @file: A GFile pointing to a profile
 * @error: A #GError, or %NULL
 *
 * Parses a profile filename, filling in all the details possible.
 *
 * Return value: %TRUE for success
 *
 * Since: 0.0.1
 **/
gboolean
gcm_profile_parse (GcmProfile *profile, GFile *file, GError **error)
{
	gchar *data = NULL;
	gboolean ret = FALSE;
	gsize length;
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
	gcm_profile_set_file (profile, file);
out:
	if (info != NULL)
		g_object_unref (info);
	g_free (data);
	return ret;
}

/*
 * _cmsWriteTagTextAscii:
 */
static cmsBool
_cmsWriteTagTextAscii (cmsHPROFILE lcms_profile, cmsTagSignature sig, const gchar *text)
{
	cmsBool ret;
	cmsMLU *mlu = cmsMLUalloc (0, 1);
	cmsMLUsetASCII (mlu, "EN", "us", text);
	ret = cmsWriteTag (lcms_profile, sig, mlu);
	cmsMLUfree (mlu);
	return ret;
}

/**
 * gcm_profile_save:
 * @profile: A valid #GcmProfile
 * @filename: the data to parse
 * @error: A #GError, or %NULL
 *
 * Saves the profile data to a file.
 *
 * Return value: %TRUE for success
 *
 * Since: 0.0.1
 **/
gboolean
gcm_profile_save (GcmProfile *profile, const gchar *filename, GError **error)
{
	GFile *file = NULL;
	gboolean ret = FALSE;
	GcmProfilePrivate *priv = profile->priv;

	/* not loaded */
	if (priv->lcms_profile == NULL) {
		g_set_error_literal (error, 1, 0, "not loaded or generated");
		goto out;
	}

	/* this is all we support writing */
	if (priv->colorspace == GCM_COLORSPACE_RGB) {
		cmsSetColorSpace (priv->lcms_profile, cmsSigRgbData);
		cmsSetPCS (priv->lcms_profile, cmsSigLabData);
	}
	if (priv->kind == GCM_PROFILE_KIND_DISPLAY_DEVICE)
		cmsSetDeviceClass (priv->lcms_profile, cmsSigDisplayClass);

	/* write text data */
	if (priv->description != NULL) {
		ret = _cmsWriteTagTextAscii (priv->lcms_profile,
					     cmsSigProfileDescriptionTag,
					     priv->description);
		if (!ret) {
			g_set_error_literal (error, 1, 0, "failed to write description");
			goto out;
		}
	}
	if (priv->copyright != NULL) {
		ret = _cmsWriteTagTextAscii (priv->lcms_profile,
					     cmsSigCopyrightTag,
					     priv->copyright);
		if (!ret) {
			g_set_error_literal (error, 1, 0, "failed to write copyright");
			goto out;
		}
	}
	if (priv->model != NULL) {
		ret = _cmsWriteTagTextAscii (priv->lcms_profile,
					     cmsSigDeviceModelDescTag,
					     priv->model);
		if (!ret) {
			g_set_error_literal (error, 1, 0, "failed to write model");
			goto out;
		}
	}
	if (priv->manufacturer != NULL) {
		ret = _cmsWriteTagTextAscii (priv->lcms_profile,
					     cmsSigDeviceMfgDescTag,
					     priv->manufacturer);
		if (!ret) {
			g_set_error_literal (error, 1, 0, "failed to write manufacturer");
			goto out;
		}
	}

	/* save, TODO: get error */
	cmsSaveProfileToFile (priv->lcms_profile, filename);
	ret = TRUE;

	/* the profile now tracks the saved file */
	if (priv->filename == NULL) {
		egg_debug ("assuming %s, so re-parse", filename);
		file = g_file_new_for_path (filename);
		ret = gcm_profile_parse (profile, file, error);
		if (!ret)
			goto out;
	}

out:
	if (file != NULL)
		g_object_unref (file);
	return ret;
}

/**
 * gcm_profile_create_from_chroma:
 * @profile: A valid #GcmProfile
 * @filename: the data to parse
 * @error: A #GError, or %NULL
 *
 * Saves the profile data to a file.
 *
 * Return value: %TRUE for success
 *
 * Since: 0.0.1
 **/
gboolean
gcm_profile_create_from_chroma (GcmProfile *profile, gdouble gamma,
				const GcmColorYxy *red,
				const GcmColorYxy *green,
				const GcmColorYxy *blue,
				const GcmColorYxy *white,
				GError **error)
{
	gboolean ret = FALSE;
	cmsCIExyYTRIPLE chroma;
	cmsToneCurve *transfer_curve[3];
	cmsCIExyY white_point;
	GcmProfilePrivate *priv = profile->priv;

	/* not loaded */
	if (priv->lcms_profile != NULL) {
		g_set_error_literal (error, 1, 0, "already loaded or generated");
		goto out;
	}

	/* copy data from our structures (which are the wrong packing
	 * size for lcms2) */
	chroma.Red.x = red->x;
	chroma.Red.y = red->y;
	chroma.Green.x = green->x;
	chroma.Green.y = green->y;
	chroma.Blue.x = blue->x;
	chroma.Blue.y = blue->y;
	white_point.x = white->x;
	white_point.y = white->y;
	white_point.Y = 1.0;

	/* estimate the transfer function for the gamma */
	transfer_curve[0] = transfer_curve[1] = transfer_curve[2] = cmsBuildGamma (NULL, gamma);

	/* create our generated profile */
	priv->lcms_profile = cmsCreateRGBProfile (&white_point, &chroma, transfer_curve);
	cmsSetEncodedICCversion (priv->lcms_profile, 2);
	cmsFreeToneCurve (*transfer_curve);

	/* success */
	ret = TRUE;
out:
	return ret;
}

/**
 * gcm_profile_guess_and_add_vcgt:
 * @profile: A valid #GcmProfile
 * @error: A #GError, or %NULL
 *
 * Runs a grey image through the profile, to guess semi-correct VCGT curves
 *
 * Return value: %TRUE for success
 *
 * Since: 0.0.1
 **/
gboolean
gcm_profile_guess_and_add_vcgt (GcmProfile *profile, GError **error)
{
	cmsHPROFILE srgb_profile = NULL;
	cmsHTRANSFORM transform = NULL;
	cmsToneCurve *transfer_curve[3];
	cmsUInt16Number rawdata[3][256];
	const guint size = 256;
	gboolean ret = FALSE;
	gdouble *values_in = NULL;
	gdouble *values_out = NULL;
	gfloat divadd;
	gfloat divamount;
	guint i;
	GcmProfilePrivate *priv = profile->priv;

	/* not loaded */
	if (priv->lcms_profile == NULL) {
		g_set_error_literal (error, 1, 0, "not already loaded or generated");
		goto out;
	}

	/* create arrays */
	values_in = g_new0 (gdouble, size * 3);
	values_out = g_new0 (gdouble, size * 3);

	/* populate with data */
	divamount = 1.0f / (gfloat) (size - 1);
	for (i=0; i<size; i++) {
		divadd = divamount * (gfloat) i;

		/* grey component */
		values_in[(i * 3)+0] = divadd;
		values_in[(i * 3)+1] = divadd;
		values_in[(i * 3)+2] = divadd;
	}

	/* create a transform from sRGB to profile */
	srgb_profile = cmsCreate_sRGBProfile ();
	transform = cmsCreateTransform (priv->lcms_profile, TYPE_RGB_DBL,
					srgb_profile, TYPE_RGB_DBL,
					INTENT_ABSOLUTE_COLORIMETRIC, 0);
	if (transform == NULL) {
		g_set_error_literal (error, 1, 0, "failed to generate transform");
		goto out;
	}

	/* do transform */
	cmsDoTransform (transform, values_in, values_out, size);

	/* unroll the data */
	for (i=0; i<size; i++) {
		rawdata[0][i] = values_out[(i * 3)+0] * 0xffff;
		rawdata[1][i] = values_out[(i * 3)+1] * 0xffff;
		rawdata[2][i] = values_out[(i * 3)+2] * 0xffff;
	}

	/* build tone curves */
	for (i=0; i<3; i++)
		transfer_curve[i] = cmsBuildTabulatedToneCurve16 (NULL, 256, rawdata[i]);

	/* write to VCGT */
	ret = cmsWriteTag (priv->lcms_profile, cmsSigVcgtType, transfer_curve);
	cmsFreeToneCurveTriple (transfer_curve);
out:
	g_free (values_in);
	g_free (values_out);
	if (transform != NULL)
		cmsDeleteTransform (transform);
	if (srgb_profile != NULL)
		cmsCloseProfile (srgb_profile);
	return ret;
}

/**
 * gcm_profile_generate_vcgt:
 * @profile: A valid #GcmProfile
 * @size: the size of the table to generate
 *
 * Generates a VCGT table of a specified size.
 *
 * Return value: A #GcmClut object, or %NULL. Free with g_object_unref()
 *
 * Since: 0.0.1
 **/
GcmClut *
gcm_profile_generate_vcgt (GcmProfile *profile, guint size)
{
	GcmClut *clut = NULL;
	GcmClutData *tmp;
	GPtrArray *array = NULL;
	GcmProfilePrivate *priv = profile->priv;
	const cmsToneCurve **vcgt;
	cmsFloat32Number in;
	guint i;

	/* get tone curves from profile */
	vcgt = cmsReadTag (priv->lcms_profile, cmsSigVcgtType);
	if (vcgt == NULL || vcgt[0] == NULL) {
		egg_debug ("profile does not have any VCGT data");
		goto out;
	}

	/* create array */
	array = g_ptr_array_new_with_free_func (g_free);
	for (i=0; i<size; i++) {
		in = (gdouble) i / (gdouble) (size - 1);
		tmp = g_new0 (GcmClutData, 1);
		tmp->red = cmsEvalToneCurveFloat(vcgt[0], in) * (gdouble) 0xffff;
		tmp->green = cmsEvalToneCurveFloat(vcgt[1], in) * (gdouble) 0xffff;
		tmp->blue = cmsEvalToneCurveFloat(vcgt[2], in) * (gdouble) 0xffff;
		g_ptr_array_add (array, tmp);
	}

	/* create new scaled CLUT */
	clut = gcm_clut_new ();
	gcm_clut_set_source_array (clut, array);
out:
	if (array != NULL)
		g_ptr_array_unref (array);
	return clut;
}

/**
 * gcm_profile_set_vcgt_from_data:
 * @profile: A valid #GcmProfile
 * @red: red color data
 * @green: green color data
 * @blue: blue color data
 * @size: the size of the color curves.
 *
 * Sets a VCGT curve of a specified size.
 *
 * Return value: %TRUE for success
 *
 * Since: 0.0.1
 **/
gboolean
gcm_profile_set_vcgt_from_data (GcmProfile *profile, guint16 *red, guint16 *green, guint16 *blue, guint size, GError **error)
{
	guint i;
	gboolean ret = FALSE;
	cmsToneCurve *vcgt_curve[3];
	GcmProfilePrivate *priv = profile->priv;

	/* not loaded or created */
	if (priv->lcms_profile == NULL)
		priv->lcms_profile = cmsCreateProfilePlaceholder (NULL);

	/* print what we've got */
	for (i=0; i<size; i++)
		egg_debug ("VCGT%i = %i,%i,%i", i, red[i], green[i], blue[i]);

	/* build tone curve */
	vcgt_curve[0] = cmsBuildTabulatedToneCurve16 (NULL, size, red);
	vcgt_curve[1] = cmsBuildTabulatedToneCurve16 (NULL, size, green);
	vcgt_curve[2] = cmsBuildTabulatedToneCurve16 (NULL, size, blue);

	/* write the tag */
	ret = cmsWriteTag (priv->lcms_profile, cmsSigVcgtType, vcgt_curve);
	for (i=0; i<3; i++)
		cmsFreeToneCurve (vcgt_curve[i]);
	if (!ret) {
		g_set_error_literal (error, 1, 0, "Failed to set vcgt");
		goto out;
	}
out:
	return ret;
}

/**
 * gcm_profile_generate_curve:
 * @profile: A valid #GcmProfile
 * @size: the size of the curve to generate
 *
 * Generates a curve of a specified size.
 *
 * Return value: A #GcmClut object, or %NULL. Free with g_object_unref()
 *
 * Since: 0.0.1
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
	if (priv->file != NULL)
		g_object_unref (priv->file);
	priv->file = NULL;
out:
	return;
}


/**
 * gcm_profile_error_cb:
 **/
static void
gcm_profile_error_cb (cmsContext ContextID, cmsUInt32Number errorcode, const char *text)
{
	egg_warning ("LCMS error %i: %s", errorcode, text);
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
	case PROP_FILE:
		g_value_set_object (value, priv->file);
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
		g_value_set_boxed (value, g_boxed_copy (GCM_TYPE_COLOR_XYZ, priv->white));
		break;
	case PROP_BLACK:
		g_value_set_boxed (value, g_boxed_copy (GCM_TYPE_COLOR_XYZ, priv->black));
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
	case PROP_TEMPERATURE:
		g_value_set_uint (value, priv->temperature);
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
	case PROP_FILE:
		gcm_profile_set_file (profile, g_value_get_object (value));
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
		gcm_color_copy_XYZ (g_value_get_boxed (value), priv->white);
		break;
	case PROP_BLACK:
		gcm_color_copy_XYZ (g_value_get_boxed (value), priv->black);
		break;
	case PROP_RED:
		gcm_color_copy_XYZ (g_value_get_boxed (value), priv->red);
		break;
	case PROP_GREEN:
		gcm_color_copy_XYZ (g_value_get_boxed (value), priv->green);
		break;
	case PROP_BLUE:
		gcm_color_copy_XYZ (g_value_get_boxed (value), priv->blue);
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
	 * GcmProfile:file:
	 */
	pspec = g_param_spec_object ("file", NULL, NULL,
				     G_TYPE_FILE,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_FILE, pspec);

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
	pspec = g_param_spec_boxed ("white", NULL, NULL,
				    GCM_TYPE_COLOR_XYZ,
				    G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_WHITE, pspec);

	/**
	 * GcmProfile:black:
	 */
	pspec = g_param_spec_boxed ("black", NULL, NULL,
				    GCM_TYPE_COLOR_XYZ,
				    G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_BLACK, pspec);

	/**
	 * GcmProfile:red:
	 */
	pspec = g_param_spec_boxed ("red", NULL, NULL,
				    GCM_TYPE_COLOR_XYZ,
				    G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_RED, pspec);

	/**
	 * GcmProfile:green:
	 */
	pspec = g_param_spec_boxed ("green", NULL, NULL,
				    GCM_TYPE_COLOR_XYZ,
				    G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_GREEN, pspec);

	/**
	 * GcmProfile:blue:
	 */
	pspec = g_param_spec_boxed ("blue", NULL, NULL,
				    GCM_TYPE_COLOR_XYZ,
				    G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_BLUE, pspec);

	/**
	 * GcmProfile:temperature:
	 */
	pspec = g_param_spec_uint ("temperature", NULL, NULL,
				   0, G_MAXUINT, 0,
				   G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_TEMPERATURE, pspec);

	g_type_class_add_private (klass, sizeof (GcmProfilePrivate));
}

/**
 * gcm_profile_init:
 **/
static void
gcm_profile_init (GcmProfile *profile)
{
	profile->priv = GCM_PROFILE_GET_PRIVATE (profile);
	profile->priv->can_delete = FALSE;
	profile->priv->monitor = NULL;
	profile->priv->kind = GCM_PROFILE_KIND_UNKNOWN;
	profile->priv->colorspace = GCM_COLORSPACE_UNKNOWN;
	profile->priv->white = gcm_color_new_XYZ ();
	profile->priv->black = gcm_color_new_XYZ ();
	profile->priv->red = gcm_color_new_XYZ ();
	profile->priv->green = gcm_color_new_XYZ ();
	profile->priv->blue = gcm_color_new_XYZ ();

	/* setup LCMS */
	cmsSetLogErrorHandler (gcm_profile_error_cb);
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
	gcm_color_free_XYZ (priv->white);
	gcm_color_free_XYZ (priv->black);
	gcm_color_free_XYZ (priv->red);
	gcm_color_free_XYZ (priv->green);
	gcm_color_free_XYZ (priv->blue);
	if (priv->file != NULL)
		g_object_unref (priv->file);
	if (priv->monitor != NULL)
		g_object_unref (priv->monitor);

	if (priv->lcms_profile != NULL)
		cmsCloseProfile (priv->lcms_profile);

	G_OBJECT_CLASS (gcm_profile_parent_class)->finalize (object);
}

/**
 * gcm_profile_new:
 *
 * Return value: a new #GcmProfile object.
 *
 * Since: 0.0.1
 **/
GcmProfile *
gcm_profile_new (void)
{
	GcmProfile *profile;
	profile = g_object_new (GCM_TYPE_PROFILE, NULL);
	return GCM_PROFILE (profile);
}

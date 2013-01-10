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

#include "config.h"

#include <glib-object.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <lcms2.h>
#include <colord.h>
#include <math.h>

#include "gcm-profile.h"
#include "gcm-hull.h"
#include "gcm-named-color.h"

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
	CdProfileKind		 kind;
	CdColorspace		 colorspace;
	guint			 size;
	gboolean		 can_delete;
	gchar			*description;
	gchar			*filename;
	gchar			*version;
	gchar			*copyright;
	gchar			*manufacturer;
	gchar			*model;
	gchar			*datetime;
	gchar			*checksum;
	guint			 temperature;
	GHashTable		*dict;
	CdColorXYZ		*white;
	CdColorXYZ		*red;
	CdColorXYZ		*green;
	CdColorXYZ		*blue;
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
	PROP_CAN_DELETE,
	PROP_WHITE,
	PROP_RED,
	PROP_GREEN,
	PROP_BLUE,
	PROP_TEMPERATURE,
	PROP_LAST
};

G_DEFINE_TYPE (GcmProfile, gcm_profile, G_TYPE_OBJECT)

#define HYP(a,b)		(sqrt((a)*(a) + (b)*(b)))

static void gcm_profile_file_monitor_changed_cb (GFileMonitor *monitor, GFile *file, GFile *other_file, GFileMonitorEvent event_type, GcmProfile *profile);

/**
 * gcm_profile_get_description:
 * @profile: A valid #GcmProfile
 *
 * Gets the profile description.
 *
 * Return value: The profile description as a string.
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
 * gcm_profile_get_temperature:
 * @profile: A valid #GcmProfile
 *
 * Gets the profile color temperature, rounded to the nearest 100K.
 *
 * Return value: The color temperature in Kelvins, or 0 for error.
 **/
guint
gcm_profile_get_temperature (GcmProfile *profile)
{
	g_return_val_if_fail (GCM_IS_PROFILE (profile), 0);
	return profile->priv->temperature;
}

/**
 * gcm_profile_get_red:
 * @profile: a valid #GcmProfile instance
 *
 * Gets the monitor red chromaticity value.
 *
 * Return value: the #CdColorXYZ value
 **/
const CdColorXYZ *
gcm_profile_get_red (GcmProfile *profile)
{
	g_return_val_if_fail (GCM_IS_PROFILE (profile), NULL);
	return profile->priv->red;
}

/**
 * gcm_profile_get_green:
 * @profile: a valid #GcmProfile instance
 *
 * Gets the monitor green chromaticity value.
 *
 * Return value: the #CdColorXYZ value
 **/
const CdColorXYZ *
gcm_profile_get_green (GcmProfile *profile)
{
	g_return_val_if_fail (GCM_IS_PROFILE (profile), NULL);
	return profile->priv->green;
}

/**
 * gcm_profile_get_blue:
 * @profile: a valid #GcmProfile instance
 *
 * Gets the monitor red chromaticity value.
 *
 * Return value: the #CdColorXYZ value
 **/
const CdColorXYZ *
gcm_profile_get_blue (GcmProfile *profile)
{
	g_return_val_if_fail (GCM_IS_PROFILE (profile), NULL);
	return profile->priv->blue;
}

/**
 * gcm_profile_get_white:
 * @profile: a valid #GcmProfile instance
 *
 * Gets the monitor white chromaticity value.
 *
 * Return value: the #CdColorXYZ value
 **/
const CdColorXYZ *
gcm_profile_get_white (GcmProfile *profile)
{
	g_return_val_if_fail (GCM_IS_PROFILE (profile), NULL);
	return profile->priv->white;
}

/**
 * gcm_profile_get_file:
 * @profile: A valid #GcmProfile
 *
 * Gets the file attached to this profile.
 *
 * Return value: A #GFile, or %NULL. Do not free.
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
 * gcm_profile_get_version:
 **/
const gchar *
gcm_profile_get_version (GcmProfile *profile)
{
	g_return_val_if_fail (GCM_IS_PROFILE (profile), NULL);
	return profile->priv->version;
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
CdProfileKind
gcm_profile_get_kind (GcmProfile *profile)
{
	g_return_val_if_fail (GCM_IS_PROFILE (profile), CD_PROFILE_KIND_UNKNOWN);
	return profile->priv->kind;
}

/**
 * gcm_profile_set_kind:
 **/
void
gcm_profile_set_kind (GcmProfile *profile, CdProfileKind kind)
{
	g_return_if_fail (GCM_IS_PROFILE (profile));
	profile->priv->kind = kind;
	g_object_notify (G_OBJECT (profile), "kind");
}

/**
 * gcm_profile_get_colorspace:
 **/
CdColorspace
gcm_profile_get_colorspace (GcmProfile *profile)
{
	g_return_val_if_fail (GCM_IS_PROFILE (profile), CD_COLORSPACE_UNKNOWN);
	return profile->priv->colorspace;
}

/**
 * gcm_profile_set_colorspace:
 **/
void
gcm_profile_set_colorspace (GcmProfile *profile, CdColorspace colorspace)
{
	g_return_if_fail (GCM_IS_PROFILE (profile));
	profile->priv->colorspace = colorspace;
	g_object_notify (G_OBJECT (profile), "colorspace");
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
 * gcm_profile_get_precooked_md5:
 **/
static gchar *
gcm_profile_get_precooked_md5 (cmsHPROFILE lcms_profile)
{
	cmsUInt8Number profile_id[16];
	gboolean md5_precooked = FALSE;
	guint i;
	gchar *md5 = NULL;

	/* check to see if we have a pre-cooked MD5 */
	cmsGetHeaderProfileID (lcms_profile, profile_id);
	for (i = 0; i < 16; i++) {
		if (profile_id[i] != 0) {
			md5_precooked = TRUE;
			break;
		}
	}
	if (!md5_precooked)
		goto out;

	/* convert to a hex string */
	md5 = g_new0 (gchar, 32 + 1);
	for (i = 0; i < 16; i++)
		g_snprintf (md5 + i*2, 3, "%02x", profile_id[i]);
out:
	return md5;
}

/**
 * gcm_profile_calc_whitepoint:
 **/
static gboolean
gcm_profile_calc_whitepoint (GcmProfile *profile)
{
	cmsBool bpc[2] = { FALSE, FALSE };
	cmsCIExyY tmp;
	cmsCIEXYZ whitepoint;
	cmsFloat64Number adaption[2] = { 0, 0 };
	cmsHPROFILE profiles[2];
	cmsHTRANSFORM transform;
	cmsUInt32Number intents[2] = { INTENT_ABSOLUTE_COLORIMETRIC,
				       INTENT_ABSOLUTE_COLORIMETRIC };
	gboolean ret;
	gdouble temp_float;
	guint8 data[3] = { 255, 255, 255 };
	GcmProfilePrivate *priv = profile->priv;

	/* do Lab to RGB transform to get primaries */
	profiles[0] = priv->lcms_profile;
	profiles[1] = cmsCreateXYZProfile ();
	transform = cmsCreateExtendedTransform (NULL,
						2,
						profiles,
						bpc,
						intents,
						adaption,
						NULL, 0, /* gamut profile */
						TYPE_RGB_8,
						TYPE_XYZ_DBL,
						cmsFLAGS_NOOPTIMIZE);
	if (transform == NULL) {
		ret = FALSE;
		g_warning ("failed to setup RGB -> XYZ transform");
		goto out;
	}

	/* Run white through the transform */
	cmsDoTransform (transform, data, &whitepoint, 1);

	/* convert to lcms xyY values */
	cd_color_xyz_set (priv->white,
			  whitepoint.X, whitepoint.Y, whitepoint.Z);
	cmsXYZ2xyY (&tmp, &whitepoint);
	g_debug ("whitepoint = %f,%f [%f]", tmp.x, tmp.y, tmp.Y);

	/* get temperature */
	ret = cmsTempFromWhitePoint (&temp_float, &tmp);
	if (ret) {
		/* round to nearest 100K */
		priv->temperature = (((guint) temp_float) / 100) * 100;
		g_debug ("color temperature = %i", priv->temperature);
	} else {
		priv->temperature = 0;
	}
out:
	if (profiles[1] != NULL)
		cmsCloseProfile (profiles[1]);
	if (transform != NULL)
		cmsDeleteTransform (transform);
	return ret;
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
 **/
gboolean
gcm_profile_parse_data (GcmProfile *profile, const guint8 *data, gsize length, GError **error)
{
	gboolean ret = FALSE;
	cmsProfileClassSignature profile_class;
	cmsColorSpaceSignature color_space;
	CdColorspace colorspace;
	CdProfileKind profile_kind;
	cmsCIEXYZ *cie_xyz;
	cmsCIEXYZTRIPLE cie_illum;
	cmsFloat64Number profile_version;
	struct tm created;
	cmsHPROFILE xyz_profile;
	cmsHTRANSFORM transform;
	gchar *text = NULL;
	gchar *checksum = NULL;
	gboolean got_illuminants = FALSE;
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
	ret = gcm_profile_calc_whitepoint (profile);
	if (!ret)
		cd_color_xyz_clear (priv->white);

	/* get the profile kind */
	profile_class = cmsGetDeviceClass (priv->lcms_profile);
	switch (profile_class) {
	case cmsSigInputClass:
		profile_kind = CD_PROFILE_KIND_INPUT_DEVICE;
		break;
	case cmsSigDisplayClass:
		profile_kind = CD_PROFILE_KIND_DISPLAY_DEVICE;
		break;
	case cmsSigOutputClass:
		profile_kind = CD_PROFILE_KIND_OUTPUT_DEVICE;
		break;
	case cmsSigLinkClass:
		profile_kind = CD_PROFILE_KIND_DEVICELINK;
		break;
	case cmsSigColorSpaceClass:
		profile_kind = CD_PROFILE_KIND_COLORSPACE_CONVERSION;
		break;
	case cmsSigAbstractClass:
		profile_kind = CD_PROFILE_KIND_ABSTRACT;
		break;
	case cmsSigNamedColorClass:
		profile_kind = CD_PROFILE_KIND_NAMED_COLOR;
		break;
	default:
		profile_kind = CD_PROFILE_KIND_UNKNOWN;
	}
	gcm_profile_set_kind (profile, profile_kind);

	/* get colorspace */
	color_space = cmsGetColorSpace (priv->lcms_profile);
	switch (color_space) {
	case cmsSigXYZData:
		colorspace = CD_COLORSPACE_XYZ;
		break;
	case cmsSigLabData:
		colorspace = CD_COLORSPACE_LAB;
		break;
	case cmsSigLuvData:
		colorspace = CD_COLORSPACE_LUV;
		break;
	case cmsSigYCbCrData:
		colorspace = CD_COLORSPACE_YCBCR;
		break;
	case cmsSigYxyData:
		colorspace = CD_COLORSPACE_YXY;
		break;
	case cmsSigRgbData:
		colorspace = CD_COLORSPACE_RGB;
		break;
	case cmsSigGrayData:
		colorspace = CD_COLORSPACE_GRAY;
		break;
	case cmsSigHsvData:
		colorspace = CD_COLORSPACE_HSV;
		break;
	case cmsSigCmykData:
		colorspace = CD_COLORSPACE_CMYK;
		break;
	case cmsSigCmyData:
		colorspace = CD_COLORSPACE_CMY;
		break;
	default:
		colorspace = CD_COLORSPACE_UNKNOWN;
	}
	gcm_profile_set_colorspace (profile, colorspace);

	/* get the illuminants from the primaries */
	if (color_space == cmsSigRgbData) {
		cie_xyz = cmsReadTag (priv->lcms_profile, cmsSigRedMatrixColumnTag);
		if (cie_xyz != NULL) {
			/* assume that if red is present, the green and blue are too */
			cd_color_xyz_copy ((CdColorXYZ *) cie_xyz, (CdColorXYZ *) &cie_illum.Red);
			cie_xyz = cmsReadTag (priv->lcms_profile, cmsSigGreenMatrixColumnTag);
			cd_color_xyz_copy ((CdColorXYZ *) cie_xyz, (CdColorXYZ *) &cie_illum.Green);
			cie_xyz = cmsReadTag (priv->lcms_profile, cmsSigBlueMatrixColumnTag);
			cd_color_xyz_copy ((CdColorXYZ *) cie_xyz, (CdColorXYZ *) &cie_illum.Blue);
			got_illuminants = TRUE;
		} else {
			g_debug ("failed to get illuminants");
		}
	}

	/* get the illuminants by running it through the profile */
	if (!got_illuminants && color_space == cmsSigRgbData) {
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
			got_illuminants = TRUE;
		} else {
			g_debug ("failed to run through profile");
		}

		/* no more need for the output profile */
		cmsCloseProfile (xyz_profile);
	}

	/* we've got valid values */
	if (got_illuminants) {
		cd_color_xyz_set (priv->red,
				   cie_illum.Red.X, cie_illum.Red.Y, cie_illum.Red.Z);
		cd_color_xyz_set (priv->green,
				   cie_illum.Green.X, cie_illum.Green.Y, cie_illum.Green.Z);
		cd_color_xyz_set (priv->blue,
				   cie_illum.Blue.X, cie_illum.Blue.Y, cie_illum.Blue.Z);
	} else {
		g_debug ("failed to get luminance values");
		cd_color_xyz_clear (priv->red);
		cd_color_xyz_clear (priv->green);
		cd_color_xyz_clear (priv->blue);
	}

	/* get the profile created time and date */
	ret = cmsGetHeaderCreationDateTime (priv->lcms_profile, &created);
	if (ret) {
		text = gcm_utils_format_date_time (&created);
		gcm_profile_set_datetime (profile, text);
		g_free (text);
	}

	/* get profile header version */
	profile_version = cmsGetProfileVersion (priv->lcms_profile);
	priv->version = g_strdup_printf ("%.2lf", profile_version);

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

	/* get precooked, or generate and set checksum */
	checksum = gcm_profile_get_precooked_md5 (priv->lcms_profile);
	if (checksum == NULL) {
		checksum = g_compute_checksum_for_data (G_CHECKSUM_MD5,
							(const guchar *) data,
							length);
	}
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
 * Parses a profile file, filling in all the details possible.
 *
 * Return value: %TRUE for success
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
 * gcm_profile_set_whitepoint:
 * @profile: A valid #GcmProfile
 * @whitepoint: the whitepoint
 * @error: A #GError, or %NULL
 *
 * Saves the whitepoint data to a file.
 *
 * Return value: %TRUE for success
 **/
gboolean
gcm_profile_set_whitepoint (GcmProfile *profile, const CdColorXYZ *whitepoint, GError **error)
{
	gboolean ret;
	GcmProfilePrivate *priv = profile->priv;

	/* not loaded */
	if (priv->lcms_profile == NULL)
		priv->lcms_profile = cmsCreateProfilePlaceholder (NULL);

	/* copy */
	cd_color_xyz_copy (whitepoint, priv->white);

	/* write tag */
	ret = cmsWriteTag (priv->lcms_profile, cmsSigMediaWhitePointTag, priv->white);
	if (!ret) {
		g_set_error_literal (error, 1, 0, "failed to set write cmsSigMediaWhitePointTag");
		goto out;
	}
out:
	return ret;
}

/**
 * gcm_profile_set_primaries:
 * @profile: A valid #GcmProfile
 * @red: the red primary
 * @green: the green primary
 * @blue: the blue primary
 * @error: A #GError, or %NULL
 *
 * Saves the primaries data to a file.
 *
 * Return value: %TRUE for success
 **/
gboolean
gcm_profile_set_primaries (GcmProfile *profile,
			   const CdColorXYZ *red,
			   const CdColorXYZ *green,
			   const CdColorXYZ *blue,
			   GError **error)
{
	gboolean ret;
	GcmProfilePrivate *priv = profile->priv;

	/* not loaded */
	if (priv->lcms_profile == NULL)
		priv->lcms_profile = cmsCreateProfilePlaceholder (NULL);

	/* copy */
	cd_color_xyz_copy (red, priv->red);
	cd_color_xyz_copy (green, priv->green);
	cd_color_xyz_copy (blue, priv->blue);

	/* write tags */
	ret = cmsWriteTag (priv->lcms_profile, cmsSigRedMatrixColumnTag, priv->red);
	if (!ret) {
		g_set_error_literal (error, 1, 0, "failed to set write cmsSigRedMatrixColumnTag");
		goto out;
	}
	ret = cmsWriteTag (priv->lcms_profile, cmsSigGreenMatrixColumnTag, priv->green);
	if (!ret) {
		g_set_error_literal (error, 1, 0, "failed to set write cmsSigGreenMatrixColumnTag");
		goto out;
	}
	ret = cmsWriteTag (priv->lcms_profile, cmsSigBlueMatrixColumnTag, priv->blue);
	if (!ret) {
		g_set_error_literal (error, 1, 0, "failed to set write cmsSigBlueMatrixColumnTag");
		goto out;
	}
out:
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
	if (priv->colorspace == CD_COLORSPACE_RGB) {
		cmsSetColorSpace (priv->lcms_profile, cmsSigRgbData);
		cmsSetPCS (priv->lcms_profile, cmsSigXYZData);
		cmsSetHeaderRenderingIntent (priv->lcms_profile,
					     INTENT_RELATIVE_COLORIMETRIC);
	}
	if (priv->kind == CD_PROFILE_KIND_DISPLAY_DEVICE)
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

	/* write profile id */
	ret = cmsMD5computeID (priv->lcms_profile);
	if (!ret) {
		g_set_error_literal (error, 1, 0, "failed to write profile id");
		goto out;
	}

	/* save, TODO: get error */
	cmsSaveProfileToFile (priv->lcms_profile, filename);
	ret = TRUE;

	/* the profile now tracks the saved file */
	if (priv->filename == NULL) {
		g_debug ("assuming %s, so re-parse", filename);
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
 * gcm_profile_get_data:
 * @profile: A valid #GcmProfile
 * @key: the dictionary key
 *
 * Gets an item of data from the profile dictionary.
 *
 * Return value: The dictinary data, or %NULL if the key does not exist.
 *
 * Since: 2.91.2
 **/
const gchar *
gcm_profile_get_data (GcmProfile *profile, const gchar *key)
{
	g_warning ("lcms2 support for dict missing: Cannot get %s", key);
	return (const gchar *) g_hash_table_lookup (profile->priv->dict, key);
}

/**
 * gcm_profile_get_data:
 * @profile: A valid #GcmProfile
 * @key: the dictionary key
 * @data: the dictionary data
 *
 * Sets an item of data from the profile dictionary, overwriting it if
 * it already exists.
 *
 * Since: 2.91.2
 **/
void
gcm_profile_set_data (GcmProfile *profile, const gchar *key, const gchar *data)
{
	g_warning ("lcms2 support for dict missing: Cannot set %s to %s", key, data);
	g_hash_table_insert (profile->priv->dict, g_strdup (key), g_strdup (data));
}

/**
 * gcm_profile_create_from_chroma:
 * @profile: A valid #GcmProfile
 * @red: primary color data
 * @green: primary color data
 * @blue: primary color data
 * @white: whitepoint data
 * @error: A #GError, or %NULL
 *
 * Saves the profile data to a file.
 *
 * Return value: %TRUE for success
 **/
gboolean
gcm_profile_create_from_chroma (GcmProfile *profile,
				gdouble localgamma,
				const CdColorYxy *red,
				const CdColorYxy *green,
				const CdColorYxy *blue,
				const CdColorYxy *white,
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
	transfer_curve[0] = transfer_curve[1] = transfer_curve[2] = cmsBuildGamma (NULL, localgamma);

	/* create our generated profile */
	priv->lcms_profile = cmsCreateRGBProfile (&white_point, &chroma, transfer_curve);
	if (priv->lcms_profile == NULL) {
		g_set_error (error, 1, 0, "failed to create profile");
		goto out;
	}
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
	for (i = 0; i < size; i++) {
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
	for (i = 0; i < size; i++) {
		rawdata[0][i] = values_out[(i * 3)+0] * 0xffff;
		rawdata[1][i] = values_out[(i * 3)+1] * 0xffff;
		rawdata[2][i] = values_out[(i * 3)+2] * 0xffff;
	}

	/* build tone curves */
	for (i = 0; i < 3; i++)
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

	/* not an actual profile */
	if (priv->lcms_profile == NULL)
		goto out;

	/* get tone curves from profile */
	vcgt = cmsReadTag (priv->lcms_profile, cmsSigVcgtType);
	if (vcgt == NULL || vcgt[0] == NULL) {
		g_debug ("profile does not have any VCGT data");
		goto out;
	}

	/* create array */
	array = g_ptr_array_new_with_free_func (g_free);
	for (i = 0; i < size; i++) {
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

	/* check monotonic */
	for (i = 0; i < size-1; i++) {
		if (red[i] > red[i+1] ||
		    green[i] > green[i+1] ||
		    blue[i] > blue[i+1]) {
			g_set_error_literal (error, 1, 0, "CVGT data not monotonic");
			goto out;
		}
	}

	/* build tone curve */
	vcgt_curve[0] = cmsBuildTabulatedToneCurve16 (NULL, size, red);
	vcgt_curve[1] = cmsBuildTabulatedToneCurve16 (NULL, size, green);
	vcgt_curve[2] = cmsBuildTabulatedToneCurve16 (NULL, size, blue);

	/* smooth it */
	for (i = 0; i < 3; i++)
		cmsSmoothToneCurve (vcgt_curve[i], 5);

	/* write the tag */
	ret = cmsWriteTag (priv->lcms_profile, cmsSigVcgtType, vcgt_curve);
	for (i = 0; i < 3; i++)
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
	CdColorspace colorspace;
	gdouble tmp;
	GcmProfilePrivate *priv = profile->priv;

	/* run through the profile */
	colorspace = gcm_profile_get_colorspace (profile);
	if (colorspace == CD_COLORSPACE_RGB) {

		/* RGB */
		component_width = 3;
		type = TYPE_RGB_DBL;

		/* create input array */
		values_in = g_new0 (gdouble, size * 3 * component_width);
		divamount = 1.0f / (gfloat) (size - 1);
		for (i = 0; i < size; i++) {
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

		for (i = 0; i < size; i++) {
			data = g_new0 (GcmClutData, 1);

			/* default values */
			data->red = 0;
			data->green = 0;
			data->blue = 0;

			/* only save curve data if it is positive */
			tmp = values_out[(i * 3 * component_width)+0] * (gfloat) 0xffff;
			if (tmp > 0.0f)
				data->red = tmp;
			tmp = values_out[(i * 3 * component_width)+4] * (gfloat) 0xffff;
			if (tmp > 0.0f)
				data->green = tmp;
			tmp = values_out[(i * 3 * component_width)+8] * (gfloat) 0xffff;
			if (tmp > 0.0f)
				data->blue = tmp;
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
 * gcm_profile_create_lab_cube:
 *
 * The original code was taken from icc_examin,
 *  Copyright 2004-2009 Kai-Uwe Behrmann <ku.b@gmx.de>
 **/
static gdouble *
gcm_profile_create_lab_cube (guint res)
{
	gdouble *lab = NULL;
	gdouble max = 0.99;
	gdouble min = 0.01;
	gint area;
	gint channels_n = 3;
	gint pos;
	gsize size;
	guint x, y;

	size = 4 * res * (res+1) + 2 * (res-1) * (res-1);
	lab = g_new0 (gdouble, size * channels_n);
	if (lab == NULL)
		goto out;

	g_debug ("created 2*%ix%i array", (guint)size, (guint)channels_n);

	/* side squares */
	for (y = 0; y <= res; ++y) {
		for (x = 0; x < 4 * res; ++x) {
			area = 0;
			pos = (y * 4 * res + x) * channels_n;

			lab[pos + 0] = pow(0.9999 - (gdouble)y / (gdouble)res, 2.0) + 0.0001;
			if (area * res <= x && x < ++area * res) {
				lab[pos + 1] = min + (x - (area - 1) * res) / (gdouble)res * (max-min);
				lab[pos + 2] = min;
			} else if (area * res <= x && x < ++area * res) {
				lab[pos + 1] = max;
				lab[pos + 2] = min + (x - (area - 1) * res) / (gdouble)res * (max-min);
			} else if (area * res <= x && x < ++area * res) {
				lab[pos + 1] = max - (x - (area - 1) * res) / (gdouble)res * (max-min);
				lab[pos + 2] = max;
			} else if (area * res <= x && x < ++area * res) {
				lab[pos + 1] = min;
				lab[pos + 2] = max - (x - (area - 1) * res) / (double)res * (max-min);
			}
		}
	}

	/* bottom and top square */
	for (y = 0; y < (res - 1); ++y) {
		for (x = 0; x < 2 * (res - 1); ++x) {
			gint x_pos;
			gint y_pos;
			gdouble val;

			pos = (4 * res * (res + 1) + y * 2 * (res - 1) + x) * channels_n;
			area = 1;
			x_pos = x + 1;
			y_pos = y + 1;
			val = (gdouble)y_pos/(gdouble)res * (max-min);

			if (/*0 <= x &&*/ x < res - 1) {
				lab[pos + 0] = 1.0;
				lab[pos + 1] = min + (x_pos - (area - 1) * (res - 1)) / (gdouble)res * (max-min);
				lab[pos + 2] = min + val;
			} else if (res - 1 <= x && x < 2 * res - 2) {
				++area;
				lab[pos + 1] = min + (x_pos - (area - 1) * (res - 1)) / (gdouble)res * (max-min);
				lab[pos + 2] = min + val;
				lab[pos + 0] = HYP (lab[pos + 1] - 0.5, lab[pos + 2] - 0.5)/100.; /* 0.0 */
			}
		}
	}
out:
	return lab;
}

/**
 * gcm_profile_create_hull_for_data:
 *
 * The original code was taken from icc_examin,
 *  Copyright 2004-2009 Kai-Uwe Behrmann <ku.b@gmx.de>
 **/
static GcmHull *
gcm_profile_create_hull_for_data (guint res, gdouble *lab, gdouble *rgb)
{
	CdColorRGB color;
	CdColorXYZ xyz;
	GcmHull *hull = NULL;
	gint channels_n = 3;
	gint off;
	gsize i;
	gsize size;
	guint face[3];
	guint x, y;

	size = 4 * res * (res+1) + 2 * (res-1) * (res-1);

	hull = gcm_hull_new ();

	/* collect colour points */
	for (i = 0; i < size; ++i) {
		xyz.X = lab[i*channels_n+0];
		xyz.Y = lab[i*channels_n+1];
		xyz.Z = lab[i*channels_n+2];
		color.R = rgb[i*channels_n+0];
		color.G = rgb[i*channels_n+1];
		color.B = rgb[i*channels_n+2];
		gcm_hull_add_vertex (hull, &xyz, &color);
	}

	for (y = 0; y < res; ++y) {
		for (x = 0; x < 4 * res; ++x) {
			gint x_ = x;
			if (x == 4 * res - 1)
				x_ = -1;
			face[0] = y * 4*res+x;
			face[1] = y * 4*res+x_+1;
			face[2] = (y+1)*4*res+x;
			gcm_hull_add_face (hull, face, 3);

			face[0] = y * 4*res+x_+1;
			face[1] = (y+1)*4*res+x_+1;
			face[2] = (y+1)*4*res+x;
			gcm_hull_add_face (hull, face, 3);
		}
	}

	off = 4 * res * (res + 1);

	/* 1 0 0 (L res b) */
	face[0] = 4*res-1;
	face[1] = off;
	face[2] = 0;
	gcm_hull_add_face (hull, face, 3);

	face[0] = off;
	face[2] = 0;
	face[1] = 1;
	gcm_hull_add_face (hull, face, 3);

	/* 0 0 0 */
	face[1] = off-1;
	face[0] = off+res-1;
	face[2] = off-4*res;
	gcm_hull_add_face (hull, face, 3);

	face[1] = off+res-1;
	face[2] = off-4*res;
	face[0] = off - 4*res+1;
	gcm_hull_add_face (hull, face, 3);

	/* 0 0 1 */
	face[2] = off-res;
	face[1] = off-res-1;
	face[0] = off+2*(res-1)*(res-1)-res+1;
	gcm_hull_add_face (hull, face, 3);

	face[0] = off-res;
	face[1] = off-res+1;
	face[2] = off+2*(res-1)*(res-1)-res+1;
	gcm_hull_add_face (hull, face, 3);

	/* 0 1 1 */
	face[0] = off-2*res+1;
	face[2] = off-2*res;
	face[1] = off+2*(res-1)*(res-1)-1;
	gcm_hull_add_face (hull, face, 3);

	face[1] = off-2*res;
	face[2] = off+2*(res-1)*(res-1)-1;
	face[0] = off-2*res-1;
	gcm_hull_add_face (hull, face, 3);

	/* 1 1 1 */
	face[0] = 2*res-1;
	face[2] = 2*res;
	face[1] = off+2*(res-1)*(res-1)-res;
	gcm_hull_add_face (hull, face, 3);

	face[1] = 2*res;
	face[2] = off+2*(res-1)*(res-1)-res;
	face[0] = 2*res+1;
	gcm_hull_add_face (hull, face, 3);

	/* 1 0 1 */
	face[2] = 3*res;
	face[0] = 3*res-1;
	face[1] = off+2*(res-1)*(res-1)-2*res+2;
	gcm_hull_add_face (hull, face, 3);

	face[2] = 3*res;
	face[1] = 3*res+1;
	face[0] = off+2*(res-1)*(res-1)-2*res+2;
	gcm_hull_add_face (hull, face, 3);

	/* 1 1 0 */
	face[0] = off+res-2;
	face[1] = res + 1;
	face[2] = res - 1;
	gcm_hull_add_face (hull, face, 3);

	face[0] = res + 1;
	face[2] = res - 1;
	face[1] = res;
	gcm_hull_add_face (hull, face, 3);

	/* 0 1 0 */
	face[0] = off+2*(res-1)-1;
	face[1] = off-3*res-1;
	face[2] = off-3*res;
	gcm_hull_add_face (hull, face, 3);

	face[1] = off+2*(res-1)-1;
	face[0] = off-3*res+1;
	face[2] = off-3*res+0;
	gcm_hull_add_face (hull, face, 3);

	for (y = 0; y < res; ++y) {
		if (0 < y && y < res - 1) {
			/* 0 0 . */
			face[2] = off-y;
			face[0] = off+(y+1)*2*(res-1)-res+1;
			face[1] = off-y-1;
			gcm_hull_add_face (hull, face, 3);

			face[0] = off+(y+0)*2*(res-1)-res+1;
			face[2] = off-y;
			face[1] = off+(y+1)*2*(res-1)-res+1;
			gcm_hull_add_face (hull, face, 3);

			/* 0 1 . */
			face[1] = off+(y+1)*2*(res-1)-1;
			face[0] = off-3*res+y+1;
			face[2] = off+(y)*2*(res-1)-1;
			gcm_hull_add_face (hull, face, 3);

			face[1] = off-3*res+y+1;
			face[2] = off+(y)*2*(res-1)-1;
			face[0] = off-3*res+y;
			gcm_hull_add_face (hull, face, 3);

			/* 1 0 . */
			face[0] = off+2*(res-1)*(res-1)-(y+1)*2*(res-1);
			face[1] = 3*res+y+1;
			face[2] = off+2*(res-1)*(res-1)-y*2*(res-1);
			gcm_hull_add_face (hull, face, 3);

			face[0] = 3*res+y+1;
			face[2] = off+2*(res-1)*(res-1)-y*2*(res-1);
			face[1] = 3*res+y;
			gcm_hull_add_face (hull, face, 3);

			/* 1 1 . */
			face[0] = off+2*(res-1)*(res-1)-(y+1)*2*(res-1)+res-2;
			face[1] = off+2*(res-1)*(res-1)-(y+0)*2*(res-1)+res-2;
			face[2] = 2*res-y;
			gcm_hull_add_face (hull, face, 3);

			face[0] = 2*res-y-1;
			face[1] = off+2*(res-1)*(res-1)-(y+1)*2*(res-1)+res-2;
			face[2] = 2*res-y;
			gcm_hull_add_face (hull, face, 3);
		}

		for (x = 0; x < 2 * res; ++x) {
			gint x_ = x + off;

			/* lower border */
			if ( y == 0 ) {
				if (x == 0) {
				} else if (x == res - 1) {
				} else if (x < res - 1) {
					/* 1 . 0 */
					face[0] = off + x - 1;
					face[1] = off + x;
					face[2] = x;
					gcm_hull_add_face (hull, face, 3);

					face[0] = off + x;
					face[2] = x;
					face[1] = x + 1;
					gcm_hull_add_face (hull, face, 3);

					/* 0 . 1 */
					face[0] = off-res-x;
					face[2] = off-res-x-1;
					face[1] = off+2*(res-1)*(res-1)-res+x;
					gcm_hull_add_face (hull, face, 3);

					face[2] = off-res-x-1;
					face[0] = off+2*(res-1)*(res-1)-res+x;
					face[1] = off+2*(res-1)*(res-1)-res+x+1;
					gcm_hull_add_face (hull, face, 3);

					/* 1 . 1 */
					face[0] = 3*res - x;
					face[1] = 3*res - x-1;
					face[2] = off+2*(res-1)*(res-1)-2*(res-1)+x-1;
					gcm_hull_add_face (hull, face, 3);

					face[0] = 3*res - x-1;
					face[2] = off+2*(res-1)*(res-1)-2*(res-1)+x-1;
					face[1] = off+2*(res-1)*(res-1)-2*(res-1)+x;
					gcm_hull_add_face (hull, face, 3);

				} else if (x > res + 1) {
					/* 0 . 0 */
					face[0] = off+x-3;
					face[2] = off+x-3+1;
					face[1] = 4*res*(res+1)-4*res + x-res-1;
					gcm_hull_add_face (hull, face, 3);

					face[1] = off+x-3+1;
					face[2] = 4*res*(res+1)-4*res + x-res-1;
					face[0] = 4*res*(res+1)-4*res + x-res;
					gcm_hull_add_face (hull, face, 3);
				}

			/* upper border */
			} else if ( y == res - 1 ) {
				if (x == 0) {
				}
			} else if (/*0 <= x &&*/ x < res - 1 - 1) {

				/* upper middle field (*L=0.0) */
				face[0] = (y-1) * 2*(res-1)+x_;
				face[2] = (y-1)*2*(res-1)+x_+1;
				face[1] = (y+0)*2*(res-1)+x_;
				gcm_hull_add_face (hull, face, 3);

				face[2] = (y-1)*2*(res-1)+x_+1;
				face[0] = (y+0)*2*(res-1)+x_;
				face[1] = (y+0)*2*(res-1)+x_+1;
				gcm_hull_add_face (hull, face, 3);

			} else if (res - 1 <= x && x < 2 * res - 2 - 1) {

				/* lower middle field (*L=1.0) */
				face[0] = (y-1) * 2*(res-1)+x_;
				face[1] = (y-1)*2*(res-1)+x_+1;
				face[2] = (y+0)*2*(res-1)+x_;
				gcm_hull_add_face (hull, face, 3);

				face[0] = (y-1)*2*(res-1)+x_+1;
				face[2] = (y+0)*2*(res-1)+x_;
				face[1] = (y+0)*2*(res-1)+x_+1;
				gcm_hull_add_face (hull, face, 3);
			}
		}
	}

	return hull;
}

/**
 * gcm_profile_generate_gamut_hull:
 * @profile: a #GcmProfile
 * @res: The resolution. 10 is quick, 20 is more precise. 12 is a good default.
 *
 * A cube from six squares with the range of the Lab cube will be
 * transformed to a profile colour space and then converted to a
 * mesh.
 *
 * The original code was taken from icc_examin,
 *  Copyright 2004-2009 Kai-Uwe Behrmann <ku.b@gmx.de>
 **/
GcmHull *
gcm_profile_generate_gamut_hull (GcmProfile *profile, guint res)
{
	cmsHPROFILE lab_profile = NULL;
	cmsHPROFILE srgb_profile = NULL;
	cmsHTRANSFORM lab_transform = NULL;
	cmsHTRANSFORM srgb_transform = NULL;
	GcmHull *hull = NULL;
	GcmProfilePrivate *priv = profile->priv;
	gdouble *lab = NULL;
	gdouble *rgb = NULL;
	gint channels_n = 3;
	gsize size = 4 * res * (res+1) + 2 * (res-1) * (res-1);

	/* create data array */
	lab = gcm_profile_create_lab_cube (res);
	rgb = g_new0 (gdouble, size * channels_n);
	if (rgb == NULL)
		goto out;

	/* run the cube through the Lab profile */
	lab_profile = cmsCreateLab4Profile (cmsD50_xyY ());
	lab_transform = cmsCreateTransform (priv->lcms_profile, TYPE_RGB_DBL,
					    lab_profile, TYPE_Lab_DBL,
					    INTENT_ABSOLUTE_COLORIMETRIC, 0);
	if (lab_transform == NULL) {
		g_warning ("failed to create Lab transform");
		goto out;
	}
	cmsDoTransform (lab_transform, lab, lab, size);

	/* run the cube through the sRGB profile */
	srgb_profile = cmsCreate_sRGBProfile ();
	srgb_transform = cmsCreateTransform (lab_profile, TYPE_Lab_DBL,
					     srgb_profile, TYPE_RGB_DBL,
					     INTENT_ABSOLUTE_COLORIMETRIC, 0);
	if (srgb_transform == NULL) {
		g_warning ("failed to create sRGB transform");
		goto out;
	}
	cmsDoTransform (srgb_transform, lab, rgb, size);

	/* create gamut hull */
	hull = gcm_profile_create_hull_for_data (res, lab, rgb);
out:
	g_free (rgb);
	g_free (lab);
	if (lab_profile != NULL)
		cmsCloseProfile (lab_profile);
	if (srgb_profile != NULL)
		cmsCloseProfile (srgb_profile);
	if (lab_transform != NULL)
		cmsDeleteTransform (lab_transform);
	if (srgb_transform != NULL)
		cmsDeleteTransform (srgb_transform);
	return hull;
}

/**
 * gcm_profile_get_named_colors:
 **/
GPtrArray *
gcm_profile_get_named_colors (GcmProfile *profile, GError **error)
{
	CdColorLab lab;
	CdColorXYZ xyz;
	cmsHPROFILE profile_lab = NULL;
	cmsHPROFILE profile_xyz = NULL;
	cmsHTRANSFORM xform = NULL;
	cmsNAMEDCOLORLIST *nc2 = NULL;
	cmsUInt16Number pcs[3];
	cmsUInt32Number count;
	gboolean ret;
	gchar name[cmsMAX_PATH];
	gchar prefix[33];
	gchar suffix[33];
	GcmNamedColor *nc;
	GcmProfilePrivate *priv = profile->priv;
	GPtrArray *array = NULL;
	GString *string;
	guchar tmp;
	guint i, j;

	/* setup a dummy transform so we can get all the named colors */
	profile_lab = cmsCreateLab2Profile (NULL);
	profile_xyz = cmsCreateXYZProfile ();
	xform = cmsCreateTransform (profile_lab, TYPE_Lab_DBL,
				    profile_xyz, TYPE_XYZ_DBL,
				    INTENT_ABSOLUTE_COLORIMETRIC, 0);
	if (xform == NULL) {
		g_set_error_literal (error, 1, 0, "no transform");
		goto out;
	}

	/* retrieve named color list from transform */
	nc2 = cmsReadTag (priv->lcms_profile, cmsSigNamedColor2Tag);
	if (nc2 == NULL) {
		g_set_error_literal (error, 1, 0, "no named color list");
		goto out;
	}

	/* get the number of NCs */
	count = cmsNamedColorCount (nc2);
	if (count == 0) {
		g_set_error_literal (error, 1, 0, "no named colors");
		goto out;
	}
	array = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	for (i = 0; i < count; i++) {

		/* parse title */
		string = g_string_new ("");
		ret = cmsNamedColorInfo (nc2, i,
					 name,
					 prefix,
					 suffix,
					 (cmsUInt16Number *)&pcs,
					 NULL);
		if (!ret) {
			g_warning ("failed to get NC #%i", i);
			goto out;
		}
		if (prefix[0] != '\0')
			g_string_append_printf (string, "%s ", prefix);
		g_string_append (string, name);
		if (suffix[0] != '\0')
			g_string_append_printf (string, " %s", suffix);

		/* check is valid */
		ret = g_utf8_validate (string->str, string->len, NULL);
		if (!ret) {
			g_warning ("invalid 7 bit ASCII / UTF8, repairing");
			for (j = 0; j < string->len; j++) {
				tmp = (guchar) string->str[j];

				/* (R) */
				if (tmp == 0xae) {
					string->str[j] = 0xc2;
					g_string_insert_c (string, j+1, tmp);
					j+=1;
				}

				/* unknown */
				if (tmp == 0x86) {
					g_string_erase (string, j, 1);
				}
			}
		}

		/* check if we repaired it okay */
		ret = g_utf8_validate (string->str, string->len, NULL);
		if (!ret) {
			g_warning ("failed to fix: skipping entry");
			for (j = 0; j < string->len; j++)
				g_print ("'%c' (%x)\n", string->str[j], (gchar)string->str[j]);
			continue;
		}

		/* get color */
		cmsLabEncoded2Float ((cmsCIELab *) &lab, pcs);
		cmsDoTransform (xform, &lab, &xyz, 1);

		/* create new nc */
		nc = gcm_named_color_new ();
		gcm_named_color_set_title (nc, string->str);
		gcm_named_color_set_value (nc, &xyz);
		g_ptr_array_add (array, nc);

		g_string_free (string, TRUE);
	}
out:
	if (profile_lab != NULL)
		cmsCloseProfile (profile_lab);
	if (profile_xyz != NULL)
		cmsCloseProfile (profile_xyz);
	if (xform != NULL)
		cmsDeleteTransform (xform);
	return array;
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
	g_debug ("%s deleted, clearing filename", priv->filename);
	if (priv->file != NULL)
		g_object_unref (priv->file);
	priv->file = NULL;
	g_free (priv->filename);
	priv->filename = NULL;
	g_object_notify (G_OBJECT (profile), "file");
out:
	return;
}

/**
 * gcm_profile_error_cb:
 **/
static void
gcm_profile_error_cb (cmsContext ContextID, cmsUInt32Number errorcode, const char *text)
{
	g_warning ("LCMS error %i: %s", errorcode, text);
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
	case PROP_CAN_DELETE:
		g_value_set_boolean (value, priv->can_delete);
		break;
	case PROP_WHITE:
		g_value_set_boxed (value, g_boxed_copy (CD_TYPE_COLOR_XYZ, priv->white));
		break;
	case PROP_RED:
		g_value_set_boxed (value, g_boxed_copy (CD_TYPE_COLOR_XYZ, priv->red));
		break;
	case PROP_GREEN:
		g_value_set_boxed (value, g_boxed_copy (CD_TYPE_COLOR_XYZ, priv->green));
		break;
	case PROP_BLUE:
		g_value_set_boxed (value, g_boxed_copy (CD_TYPE_COLOR_XYZ, priv->blue));
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
	case PROP_WHITE:
		cd_color_xyz_copy (g_value_get_boxed (value), priv->white);
		break;
	case PROP_RED:
		cd_color_xyz_copy (g_value_get_boxed (value), priv->red);
		break;
	case PROP_GREEN:
		cd_color_xyz_copy (g_value_get_boxed (value), priv->green);
		break;
	case PROP_BLUE:
		cd_color_xyz_copy (g_value_get_boxed (value), priv->blue);
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
				    CD_TYPE_COLOR_XYZ,
				    G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_WHITE, pspec);

	/**
	 * GcmProfile:red:
	 */
	pspec = g_param_spec_boxed ("red", NULL, NULL,
				    CD_TYPE_COLOR_XYZ,
				    G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_RED, pspec);

	/**
	 * GcmProfile:green:
	 */
	pspec = g_param_spec_boxed ("green", NULL, NULL,
				    CD_TYPE_COLOR_XYZ,
				    G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_GREEN, pspec);

	/**
	 * GcmProfile:blue:
	 */
	pspec = g_param_spec_boxed ("blue", NULL, NULL,
				    CD_TYPE_COLOR_XYZ,
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
	profile->priv->dict = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	profile->priv->kind = CD_PROFILE_KIND_UNKNOWN;
	profile->priv->colorspace = CD_COLORSPACE_UNKNOWN;
	profile->priv->white = cd_color_xyz_new ();
	profile->priv->red = cd_color_xyz_new ();
	profile->priv->green = cd_color_xyz_new ();
	profile->priv->blue = cd_color_xyz_new ();

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
	g_free (priv->version);
	cd_color_xyz_free (priv->white);
	cd_color_xyz_free (priv->red);
	cd_color_xyz_free (priv->green);
	cd_color_xyz_free (priv->blue);
	g_hash_table_destroy (profile->priv->dict);
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
 **/
GcmProfile *
gcm_profile_new (void)
{
	GcmProfile *profile;
	profile = g_object_new (GCM_TYPE_PROFILE, NULL);
	return GCM_PROFILE (profile);
}

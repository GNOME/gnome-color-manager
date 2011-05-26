/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
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
#include <tiff.h>
#include <tiffio.h>
#include <libexif/exif-data.h>

#include "gcm-exif.h"

static void     gcm_exif_finalize	(GObject     *object);

#define GCM_EXIF_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GCM_TYPE_EXIF, GcmExifPrivate))

/**
 * GcmExifPrivate:
 *
 * Private #GcmExif data
 **/
struct _GcmExifPrivate
{
	gchar				*manufacturer;
	gchar				*model;
	gchar				*serial;
	CdDeviceKind			 device_kind;
};

enum {
	PROP_0,
	PROP_MANUFACTURER,
	PROP_MODEL,
	PROP_SERIAL,
	PROP_DEVICE_KIND,
	PROP_LAST
};

G_DEFINE_TYPE (GcmExif, gcm_exif, G_TYPE_OBJECT)

/**
 * gcm_exif_parse_tiff:
 **/
static gboolean
gcm_exif_parse_tiff (GcmExif *exif, const gchar *filename, GError **error)
{
	gboolean ret = TRUE;
	const gchar *manufacturer = NULL;
	const gchar *model = NULL;
	const gchar *serial = NULL;
	const gchar *temp = NULL;
	CdDeviceKind device_kind = CD_DEVICE_KIND_UNKNOWN;
	TIFF *tiff;
	GcmExifPrivate *priv = exif->priv;

	/* open file */
	tiff = TIFFOpen (filename, "r");
	TIFFGetField (tiff,TIFFTAG_MAKE, &manufacturer);
	TIFFGetField (tiff,TIFFTAG_MODEL, &model);
	TIFFGetField (tiff,TIFFTAG_CAMERASERIALNUMBER, &serial);

	/* we failed to get data */
	if (manufacturer == NULL || model == NULL) {
		g_set_error (error,
			     GCM_EXIF_ERROR,
			     GCM_EXIF_ERROR_NO_DATA,
			     "Failed to get EXIF data from TIFF");
		ret = FALSE;
		goto out;
	}

	/* these are all camera specific values */
	TIFFGetField (tiff,EXIFTAG_FNUMBER, &temp);
	if (temp != NULL)
		device_kind = CD_DEVICE_KIND_CAMERA;
	TIFFGetField (tiff,TIFFTAG_LENSINFO, &temp);
	if (temp != NULL)
		device_kind = CD_DEVICE_KIND_CAMERA;

	/* crappy fallback */
	if (g_str_has_prefix (manufacturer, "NIKON"))
		device_kind = CD_DEVICE_KIND_CAMERA;

	/* free old versions */
	g_free (priv->manufacturer);
	g_free (priv->model);
	g_free (priv->serial);

	/* create copies for ourselves */
	priv->manufacturer = g_strdup (manufacturer);
	priv->model = g_strdup (model);
	priv->serial = g_strdup (serial);
	priv->device_kind = device_kind;
out:
	TIFFClose (tiff);
	return ret;
}

/**
 * gcm_exif_parse_jpeg:
 **/
static gboolean
gcm_exif_parse_jpeg (GcmExif *exif, const gchar *filename, GError **error)
{
	gboolean ret = TRUE;
	GcmExifPrivate *priv = exif->priv;
	ExifData *ed = NULL;
	ExifEntry *entry;
	CdDeviceKind device_kind = CD_DEVICE_KIND_UNKNOWN;
	gchar make[1024] = { '\0' };
	gchar model[1024] = { '\0' };

	/* load EXIF file */
	ed = exif_data_new_from_file (filename);
	if (ed == NULL) {
		g_set_error (error,
			     GCM_EXIF_ERROR,
			     GCM_EXIF_ERROR_NO_DATA,
			     "File not readable or no EXIF data in file");
		ret = FALSE;
		goto out;
	}

	/* get make */
	entry = exif_content_get_entry (ed->ifd[EXIF_IFD_0], EXIF_TAG_MAKE);
	if (entry != NULL) {
		exif_entry_get_value (entry, make, sizeof (make));
		g_strchomp (make);
	}

	/* get model */
	entry = exif_content_get_entry (ed->ifd[EXIF_IFD_0], EXIF_TAG_MODEL);
	if (entry != NULL) {
		exif_entry_get_value (entry, model, sizeof (model));
		g_strchomp (model);
	}

	/* these are all camera specific values */
	entry = exif_content_get_entry (ed->ifd[EXIF_IFD_EXIF], EXIF_TAG_FNUMBER);
	if (entry != NULL)
		device_kind = CD_DEVICE_KIND_CAMERA;
	entry = exif_content_get_entry (ed->ifd[EXIF_IFD_EXIF], EXIF_TAG_SHUTTER_SPEED_VALUE);
	if (entry != NULL)
		device_kind = CD_DEVICE_KIND_CAMERA;
	entry = exif_content_get_entry (ed->ifd[EXIF_IFD_EXIF], EXIF_TAG_FLASH);
	if (entry != NULL)
		device_kind = CD_DEVICE_KIND_CAMERA;

	/* we failed to get data */
	if (make == NULL || model == NULL) {
		g_set_error (error,
			     GCM_EXIF_ERROR,
			     GCM_EXIF_ERROR_NO_DATA,
			     "Failed to get EXIF data from JPEG");
		ret = FALSE;
		goto out;
	}

	/* free old versions */
	g_free (priv->manufacturer);
	g_free (priv->model);
	g_free (priv->serial);

	/* create copies for ourselves */
	priv->manufacturer = g_strdup (make);
	priv->model = g_strdup (model);
	priv->serial = NULL;
	priv->device_kind = device_kind;
out:
	if (ed != NULL)
		exif_data_unref (ed);
	return ret;
}

#ifdef HAVE_EXIV
/**
 * gcm_exif_parse_exiv:
 **/
static gboolean
gcm_exif_parse_exiv (GcmExif *exif, const gchar *filename, GError **error)
{
	gboolean ret;
	gchar *command_line;
	gint exit_status = 0;
	gchar *standard_output = NULL;
	gchar **split = NULL;
	GcmExifPrivate *priv = exif->priv;

	command_line = g_strdup_printf (LIBEXECDIR "/gcm-helper-exiv \"%s\"", filename);
	ret = g_spawn_command_line_sync (command_line, &standard_output, NULL, &exit_status, error);
	if (!ret)
		goto out;

	/* failed to sniff */
	if (exit_status != 0) {
		ret = FALSE;
		g_set_error (error, GCM_EXIF_ERROR, GCM_EXIF_ERROR_NO_SUPPORT,
			     "Failed to run: %s", standard_output);
		goto out;
	}

	/* get data */
	split = g_strsplit (standard_output, "\n", -1);
	if (g_strv_length (split) != 4) {
		ret = FALSE;
		g_set_error (error, GCM_EXIF_ERROR, GCM_EXIF_ERROR_NO_SUPPORT,
			     "Unexpected output: %s", standard_output);
		goto out;
	}

	/* free old versions */
	g_free (priv->manufacturer);
	g_free (priv->model);
	g_free (priv->serial);

	/* create copies for ourselves */
	if (split[0][0] != '\0')
		priv->manufacturer = g_strdup (split[0]);
	else
		priv->manufacturer = NULL;
	if (split[1][0] != '\0')
		priv->model = g_strdup (split[1]);
	else
		priv->model = NULL;
	if (split[2][0] != '\0')
		priv->serial = g_strdup (split[2]);
	else
		priv->serial = NULL;
	priv->device_kind = CD_DEVICE_KIND_CAMERA;

out:
	g_free (standard_output);
	g_free (command_line);
	g_strfreev (split);
	return ret;
}
#endif

/**
 * gcm_exif_parse:
 **/
gboolean
gcm_exif_parse (GcmExif *exif, GFile *file, GError **error)
{
	gboolean ret = FALSE;
	gchar *filename = NULL;
	GFileInfo *info = NULL;
	const gchar *content_type;

	g_return_val_if_fail (GCM_IS_EXIF (exif), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* check exists */
	info = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
				  G_FILE_QUERY_INFO_NONE, NULL, error);
	if (info == NULL)
		goto out;

	/* get EXIF data in different ways depending on content type */
	content_type = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE);
	if (g_strcmp0 (content_type, "image/tiff") == 0) {
		filename = g_file_get_path (file);
		ret = gcm_exif_parse_tiff (exif, filename, error);
		goto out;
	}
	if (g_strcmp0 (content_type, "image/jpeg") == 0) {
		filename = g_file_get_path (file);
		ret = gcm_exif_parse_jpeg (exif, filename, error);
		goto out;
	}

#ifdef HAVE_EXIV
	if (g_strcmp0 (content_type, "image/x-adobe-dng") == 0 ||
	    g_strcmp0 (content_type, "image/x-canon-crw") == 0 ||
	    g_strcmp0 (content_type, "image/x-fuji-raf") == 0 ||
	    g_strcmp0 (content_type, "image/x-kde-raw") == 0 ||
	    g_strcmp0 (content_type, "image/x-kodak-kdc") == 0 ||
	    g_strcmp0 (content_type, "image/x-minolta-mrw") == 0 ||
	    g_strcmp0 (content_type, "image/x-nikon-nef") == 0 ||
	    g_strcmp0 (content_type, "image/x-olympus-orf") == 0 ||
	    g_strcmp0 (content_type, "image/x-panasonic-raw") == 0 ||
	    g_strcmp0 (content_type, "image/x-pentax-pef") == 0 ||
	    g_strcmp0 (content_type, "image/x-sigma-x3f") == 0 ||
	    g_strcmp0 (content_type, "image/x-sony-arw") == 0) {
		filename = g_file_get_path (file);
		ret = gcm_exif_parse_exiv (exif, filename, error);
		goto out;
	}
#endif

	/* no support */
	g_set_error (error,
		     GCM_EXIF_ERROR,
		     GCM_EXIF_ERROR_NO_SUPPORT,
		     "No support for %s content type", content_type);
out:
	g_free (filename);
	if (info != NULL)
		g_object_unref (info);
	return ret;
}

/**
 * gcm_exif_get_manufacturer:
 **/
const gchar *
gcm_exif_get_manufacturer (GcmExif *exif)
{
	g_return_val_if_fail (GCM_IS_EXIF (exif), NULL);
	return exif->priv->manufacturer;
}

/**
 * gcm_exif_get_model:
 **/
const gchar *
gcm_exif_get_model (GcmExif *exif)
{
	g_return_val_if_fail (GCM_IS_EXIF (exif), NULL);
	return exif->priv->model;
}

/**
 * gcm_exif_get_serial:
 **/
const gchar *
gcm_exif_get_serial (GcmExif *exif)
{
	g_return_val_if_fail (GCM_IS_EXIF (exif), NULL);
	return exif->priv->serial;
}

/**
 * gcm_exif_get_device_kind:
 **/
CdDeviceKind
gcm_exif_get_device_kind (GcmExif *exif)
{
	g_return_val_if_fail (GCM_IS_EXIF (exif), CD_DEVICE_KIND_UNKNOWN);
	return exif->priv->device_kind;
}

/**
 * gcm_exif_get_property:
 **/
static void
gcm_exif_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GcmExif *exif = GCM_EXIF (object);
	GcmExifPrivate *priv = exif->priv;

	switch (prop_id) {
	case PROP_MANUFACTURER:
		g_value_set_string (value, priv->manufacturer);
		break;
	case PROP_MODEL:
		g_value_set_string (value, priv->model);
		break;
	case PROP_SERIAL:
		g_value_set_string (value, priv->serial);
		break;
	case PROP_DEVICE_KIND:
		g_value_set_uint (value, priv->device_kind);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gcm_exif_class_init:
 **/
static void
gcm_exif_class_init (GcmExifClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gcm_exif_finalize;
	object_class->get_property = gcm_exif_get_property;

	/**
	 * GcmExif:manufacturer:
	 */
	pspec = g_param_spec_string ("manufacturer", NULL, NULL,
				     NULL,
				     G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_MANUFACTURER, pspec);

	/**
	 * GcmExif:model:
	 */
	pspec = g_param_spec_string ("model", NULL, NULL,
				     NULL,
				     G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_MODEL, pspec);

	/**
	 * GcmExif:serial:
	 */
	pspec = g_param_spec_string ("serial", NULL, NULL,
				     NULL,
				     G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_SERIAL, pspec);

	/**
	 * GcmExif:device-kind:
	 */
	pspec = g_param_spec_uint ("device-kind", NULL, NULL,
				   0, G_MAXUINT, CD_DEVICE_KIND_UNKNOWN,
				   G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_DEVICE_KIND, pspec);

	g_type_class_add_private (klass, sizeof (GcmExifPrivate));
}

/**
 * gcm_exif_init:
 **/
static void
gcm_exif_init (GcmExif *exif)
{
	exif->priv = GCM_EXIF_GET_PRIVATE (exif);
	exif->priv->manufacturer = NULL;
	exif->priv->model = NULL;
	exif->priv->serial = NULL;
	exif->priv->device_kind = CD_DEVICE_KIND_CAMERA;
}

/**
 * gcm_exif_finalize:
 **/
static void
gcm_exif_finalize (GObject *object)
{
	GcmExif *exif = GCM_EXIF (object);
	GcmExifPrivate *priv = exif->priv;

	g_free (priv->manufacturer);
	g_free (priv->model);
	g_free (priv->serial);

	G_OBJECT_CLASS (gcm_exif_parent_class)->finalize (object);
}

/**
 * gcm_exif_new:
 *
 * Return value: a new GcmExif object.
 **/
GcmExif *
gcm_exif_new (void)
{
	GcmExif *exif;
	exif = g_object_new (GCM_TYPE_EXIF, NULL);
	return GCM_EXIF (exif);
}


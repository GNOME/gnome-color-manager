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

#include <gtk/gtk.h>
#include <lcms2.h>

#include "gcm-image.h"

static void     gcm_image_finalize	(GObject     *object);

#define GCM_IMAGE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GCM_TYPE_IMAGE, GcmImagePrivate))

/**
 * GcmImagePrivate:
 *
 * Private #GcmImage data
 **/
struct _GcmImagePrivate
{
	gboolean			 has_embedded_profile;
	gboolean			 use_embedded_profile;
	GcmProfile			*output_profile;
	GcmProfile			*input_profile;
	GcmProfile			*abstract_profile;
	GdkPixbuf			*original_pixbuf;
};

enum {
	PROP_0,
	PROP_HAS_EMBEDDED_PROFILE,
	PROP_USE_EMBEDDED_PROFILE,
	PROP_OUTPUT_PROFILE,
	PROP_INPUT_PROFILE,
	PROP_ABSTRACT_PROFILE,
	PROP_LAST
};

G_DEFINE_TYPE (GcmImage, gcm_image, GTK_TYPE_IMAGE)

/**
 * gcm_image_get_format:
 **/
static cmsUInt32Number
gcm_image_get_format (GcmImage *image)
{
	guint bits;
	guint has_alpha;
	cmsUInt32Number format = 0;
	GcmImagePrivate *priv = image->priv;

	/* get data */
	has_alpha = gdk_pixbuf_get_has_alpha (priv->original_pixbuf);
	bits = gdk_pixbuf_get_bits_per_sample (priv->original_pixbuf);

	/* no alpha channel */
	if (G_LIKELY (!has_alpha)) {
		if (bits == 8) {
			format = TYPE_RGB_8;
			goto out;
		}
		if (bits == 16) {
			format = TYPE_RGB_16;
			goto out;
		}
		goto out;
	}

	/* alpha channel */
	if (bits == 8) {
		format = TYPE_RGBA_8;
		goto out;
	}
	if (bits == 16) {
		format = TYPE_RGBA_16;
		goto out;
	}
out:
	return format;
}

/**
 * gcm_image_cms_convert_pixbuf:
 **/
static void
gcm_image_cms_convert_pixbuf (GcmImage *image)
{
	const gchar *profile_base64;
	gint i;
	cmsUInt32Number format;
	gint width, height, rowstride;
	guchar *p_in;
	guchar *p_out;
	cmsHPROFILE profile_in = NULL;
	cmsHPROFILE profile_out = NULL;
	cmsHPROFILE profile_abstract = NULL;
	cmsHTRANSFORM transform = NULL;
	GdkPixbuf *pixbuf_cms;
	guchar *profile_data = NULL;
	gsize profile_size;
	gboolean profile_close_input = FALSE;
	gboolean profile_close_output = FALSE;
	GcmImagePrivate *priv = image->priv;

	/* not a pixbuf-backed image */
	if (priv->original_pixbuf == NULL) {
		g_warning ("no pixbuf to convert");
		goto out;
	}

	/* CMYK not supported */
	if (gdk_pixbuf_get_colorspace (priv->original_pixbuf) != GDK_COLORSPACE_RGB) {
		g_debug ("non-RGB not supported");
		goto out;
	}

	/* work out the LCMS format flags */
	format = gcm_image_get_format (image);
	if (format == 0) {
		g_warning ("format not supported");
		goto out;
	}

	/* get profile from pixbuf */
	pixbuf_cms = gtk_image_get_pixbuf (GTK_IMAGE(image));
	profile_base64 = gdk_pixbuf_get_option (pixbuf_cms, "profile");

	/* set the boolean property */
	priv->has_embedded_profile = (profile_base64 != NULL);

	/* get input profile */
	if (priv->use_embedded_profile && priv->has_embedded_profile) {

		/* decode built-in */
		profile_data = g_base64_decode (profile_base64, &profile_size);
		if (profile_data == NULL) {
			g_warning ("failed to decode base64");
			goto out;
		}

		/* use embedded profile */
		profile_in = cmsOpenProfileFromMem (profile_data, profile_size);
		profile_close_input = TRUE;
	} else if (priv->input_profile != NULL) {

		/* not RGB */
		if (gcm_profile_get_colorspace (priv->input_profile) != CD_COLORSPACE_RGB) {
			g_warning ("input colorspace has to be RGB!");
			goto out;
		}

		/* use built-in */
		g_debug ("using input profile of %s", gcm_profile_get_filename (priv->input_profile));
		profile_in = gcm_profile_get_handle (priv->input_profile);
		profile_close_input = FALSE;
	} else {
		g_debug ("no input profile, assume sRGB");
		profile_in = cmsCreate_sRGBProfile ();
		profile_close_input = TRUE;
	}

	/* get output profile */
	if (priv->output_profile != NULL) {

		/* not RGB */
		if (gcm_profile_get_colorspace (priv->output_profile) != CD_COLORSPACE_RGB) {
			g_warning ("output colorspace has to be RGB!");
			goto out;
		}

		/* use built-in */
		g_debug ("using output profile of %s", gcm_profile_get_filename (priv->output_profile));
		profile_out = gcm_profile_get_handle (priv->output_profile);
		profile_close_output = FALSE;
	} else {
		g_debug ("no output profile, assume sRGB");
		profile_out = cmsCreate_sRGBProfile ();
		profile_close_output = TRUE;
	}

	/* get abstract profile */
	if (priv->abstract_profile != NULL) {
		cmsHPROFILE profiles[3];

		/* not LAB */
		if (gcm_profile_get_colorspace (priv->abstract_profile) != CD_COLORSPACE_LAB) {
			g_warning ("abstract profile has to be LAB!");
			goto out;
		}

		/* generate a devicelink */
		profiles[0] = profile_in;
		profiles[1] = gcm_profile_get_handle (priv->abstract_profile);
		profiles[2] = profile_out;
		transform = cmsCreateMultiprofileTransform (profiles, 3, format, format, INTENT_PERCEPTUAL, 0);

	} else {
		/* create basic transform */
		transform = cmsCreateTransform (profile_in, format, profile_out, format, INTENT_PERCEPTUAL, 0);
	}

	/* failed? */
	if (transform == NULL)
		goto out;

	/* process each row */
	height = gdk_pixbuf_get_height (priv->original_pixbuf);
	width = gdk_pixbuf_get_width (priv->original_pixbuf);
	rowstride = gdk_pixbuf_get_rowstride (priv->original_pixbuf);
	p_in = gdk_pixbuf_get_pixels (priv->original_pixbuf);
	p_out = gdk_pixbuf_get_pixels (pixbuf_cms);
	for (i=0; i<height; ++i) {
		cmsDoTransform (transform, p_in, p_out, width);
		p_in += rowstride;
		p_out += rowstride;
	}

	/* refresh widget */
	if (gtk_widget_get_visible (GTK_WIDGET(image))) {
		gtk_widget_set_visible (GTK_WIDGET(image), FALSE);
		gtk_widget_set_visible (GTK_WIDGET(image), TRUE);
	}
out:
	/* destroy lcms state */
	if (transform != NULL)
		cmsDeleteTransform (transform);
	if (profile_in != NULL && profile_close_input)
		cmsCloseProfile (profile_in);
	if (profile_out != NULL && profile_close_output)
		cmsCloseProfile (profile_out);
	if (profile_abstract != NULL)
		cmsCloseProfile (profile_abstract);

	g_free (profile_data);
	return;
}

/**
 * gcm_image_notify_pixbuf_cb:
 **/
static void
gcm_image_notify_pixbuf_cb (GObject *object, GParamSpec *pspec, GcmImage *image)
{
	GdkPixbuf *pixbuf;
	gpointer applied;
	GcmImagePrivate *priv = image->priv;

	/* check */
	pixbuf = gtk_image_get_pixbuf (GTK_IMAGE(image));
	if (pixbuf == NULL)
		goto out;
	applied = g_object_get_data (G_OBJECT(pixbuf), "cms-converted-pixbuf");
	if (applied != NULL) {
		g_debug ("already copied and converted pixbuf, use gcm_image_cms_convert_pixbuf() instead");
		goto out;
	}

	/* unref existing */
	if (priv->original_pixbuf != NULL) {
		g_object_unref (priv->original_pixbuf);
		priv->original_pixbuf = NULL;
	}

	/* get new */
	if (pixbuf != NULL) {
		priv->original_pixbuf = gdk_pixbuf_copy (pixbuf);
		gcm_image_cms_convert_pixbuf (image);
		g_object_set_data (G_OBJECT(pixbuf), "cms-converted-pixbuf", (gpointer)"true");
	}
out:
	/* we do not own the pixbuf */
	return;
}

/**
 * gcm_image_set_input_profile:
 **/
void
gcm_image_set_input_profile (GcmImage *image, GcmProfile *profile)
{
	GcmImagePrivate *priv = image->priv;
	if (priv->input_profile != NULL) {
		g_object_unref (priv->input_profile);
		priv->input_profile = NULL;
	}
	if (profile != NULL) {
		priv->input_profile = g_object_ref (profile);
		gcm_image_use_embedded_profile (image, FALSE);
	}
	gcm_image_cms_convert_pixbuf (image);
}

/**
 * gcm_image_set_output_profile:
 **/
void
gcm_image_set_output_profile (GcmImage *image, GcmProfile *profile)
{
	GcmImagePrivate *priv = image->priv;
	if (priv->output_profile != NULL) {
		g_object_unref (priv->output_profile);
		priv->output_profile = NULL;
	}
	if (profile != NULL)
		priv->output_profile = g_object_ref (profile);
	gcm_image_cms_convert_pixbuf (image);
}

/**
 * gcm_image_set_abstract_profile:
 **/
void
gcm_image_set_abstract_profile (GcmImage *image, GcmProfile *profile)
{
	GcmImagePrivate *priv = image->priv;
	if (priv->abstract_profile != NULL) {
		g_object_unref (priv->abstract_profile);
		priv->abstract_profile = NULL;
	}
	if (profile != NULL)
		priv->abstract_profile = g_object_ref (profile);
	gcm_image_cms_convert_pixbuf (image);
}

/**
 * gcm_image_has_embedded_profile:
 **/
gboolean
gcm_image_has_embedded_profile (GcmImage *image)
{
	return image->priv->has_embedded_profile;
}

/**
 * gcm_image_use_embedded_profile:
 **/
void
gcm_image_use_embedded_profile (GcmImage *image, gboolean use_embedded_profile)
{
	image->priv->use_embedded_profile = use_embedded_profile;
	gcm_image_cms_convert_pixbuf (image);
}

/**
 * gcm_image_get_property:
 **/
static void
gcm_image_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GcmImage *image = GCM_IMAGE (object);
	GcmImagePrivate *priv = image->priv;

	switch (prop_id) {
	case PROP_HAS_EMBEDDED_PROFILE:
		g_value_set_boolean (value, priv->has_embedded_profile);
		break;
	case PROP_USE_EMBEDDED_PROFILE:
		g_value_set_boolean (value, priv->use_embedded_profile);
		break;
	case PROP_OUTPUT_PROFILE:
		g_value_set_object (value, priv->output_profile);
		break;
	case PROP_INPUT_PROFILE:
		g_value_set_object (value, priv->input_profile);
		break;
	case PROP_ABSTRACT_PROFILE:
		g_value_set_object (value, priv->abstract_profile);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gcm_image_set_property:
 **/
static void
gcm_image_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	GcmImage *image = GCM_IMAGE (object);

	switch (prop_id) {
	case PROP_USE_EMBEDDED_PROFILE:
		gcm_image_use_embedded_profile (image, g_value_get_boolean (value));
		break;
	case PROP_OUTPUT_PROFILE:
		gcm_image_set_output_profile (image, g_value_get_object (value));
		break;
	case PROP_INPUT_PROFILE:
		gcm_image_set_input_profile (image, g_value_get_object (value));
		break;
	case PROP_ABSTRACT_PROFILE:
		gcm_image_set_abstract_profile (image, g_value_get_object (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gcm_image_class_init:
 **/
static void
gcm_image_class_init (GcmImageClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gcm_image_finalize;
	object_class->get_property = gcm_image_get_property;
	object_class->set_property = gcm_image_set_property;

	/**
	 * GcmImage:has-embedded-profile:
	 */
	pspec = g_param_spec_boolean ("has-embedded-profile", NULL, NULL,
				      TRUE,
				      G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_HAS_EMBEDDED_PROFILE, pspec);

	/**
	 * GcmImage:use-embedded-profile:
	 */
	pspec = g_param_spec_boolean ("use-embedded-profile", NULL, NULL,
				      TRUE,
				      G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_USE_EMBEDDED_PROFILE, pspec);

	/**
	 * GcmImage:output-profile:
	 */
	pspec = g_param_spec_object ("output-profile", NULL, NULL,
				     GCM_TYPE_PROFILE,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_OUTPUT_PROFILE, pspec);

	/**
	 * GcmImage:input-profile:
	 */
	pspec = g_param_spec_object ("input-profile", NULL, NULL,
				     GCM_TYPE_PROFILE,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_INPUT_PROFILE, pspec);

	/**
	 * GcmImage:abstract-profile:
	 */
	pspec = g_param_spec_object ("abstract-profile", NULL, NULL,
				     GCM_TYPE_PROFILE,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_ABSTRACT_PROFILE, pspec);

	g_type_class_add_private (klass, sizeof (GcmImagePrivate));
}

/**
 * gcm_image_init:
 **/
static void
gcm_image_init (GcmImage *image)
{
	GcmImagePrivate *priv;

	priv = image->priv = GCM_IMAGE_GET_PRIVATE (image);

	priv->has_embedded_profile = FALSE;
	priv->use_embedded_profile = TRUE;
	priv->original_pixbuf = NULL;

	/* only convert pixbuf if the size changes */
	g_signal_connect (GTK_WIDGET(image), "notify::pixbuf",
			  G_CALLBACK(gcm_image_notify_pixbuf_cb), image);
}

/**
 * gcm_image_finalize:
 **/
static void
gcm_image_finalize (GObject *object)
{
	GcmImage *image = GCM_IMAGE (object);
	GcmImagePrivate *priv = image->priv;

	if (priv->original_pixbuf != NULL)
		g_object_unref (priv->original_pixbuf);
	if (priv->output_profile != NULL)
		g_object_unref (priv->output_profile);
	if (priv->input_profile != NULL)
		g_object_unref (priv->input_profile);
	if (priv->abstract_profile != NULL)
		g_object_unref (priv->abstract_profile);

	G_OBJECT_CLASS (gcm_image_parent_class)->finalize (object);
}

/**
 * gcm_image_new:
 *
 * Return value: a new GcmImage object.
 **/
GcmImage *
gcm_image_new (void)
{
	GcmImage *image;
	image = g_object_new (GCM_TYPE_IMAGE, NULL);
	return GCM_IMAGE (image);
}


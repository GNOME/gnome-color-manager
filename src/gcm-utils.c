/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009-2015 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <colord.h>
#include <math.h>

#include "gcm-utils.h"

gchar *
gcm_utils_linkify (const gchar *hostile_text)
{
	guint i;
	guint j = 0;
	gboolean ret;
	GString *string;
	g_autofree gchar *text = NULL;

	/* Properly escape this as some profiles 'helpfully' put markup in like:
	 * "Copyright (C) 2005-2010 Kai-Uwe Behrmann <www.behrmann.name>" */
	text = g_markup_escape_text (hostile_text, -1);

	/* find and replace links */
	string = g_string_new ("");
	for (i=0;; i++) {

		/* find the start of a link */
		ret = g_str_has_prefix (&text[i], "http://");
		if (ret) {
			g_string_append_len (string, text+j, i-j);
			for (j=i;; j++) {
				/* find the end of the link, or the end of the string */
				if (text[j] == '\0' ||
				    text[j] == ' ') {
					g_string_append (string, "<a href=\"");
					g_string_append_len (string, text+i, j-i);
					g_string_append (string, "\">");
					g_string_append_len (string, text+i, j-i);
					g_string_append (string, "</a>");
					break;
				}
			}
		}

		/* end of the string, dump what's left */
		if (text[i] == '\0') {
			g_string_append_len (string, text+j, i-j);
			break;
		}
	}
	return g_string_free (string, FALSE);
}

const gchar *
cd_colorspace_to_localised_string (CdColorspace colorspace)
{
	if (colorspace == CD_COLORSPACE_RGB) {
		/* TRANSLATORS: this is the colorspace, e.g. red, green, blue */
		return _("RGB");
	}
	if (colorspace == CD_COLORSPACE_CMYK) {
		/* TRANSLATORS: this is the colorspace, e.g. cyan, magenta, yellow, black */
		return _("CMYK");
	}
	if (colorspace == CD_COLORSPACE_GRAY) {
		/* TRANSLATORS: this is the colorspace type */
		return _("gray");
	}
	return NULL;
}

static CdPixelFormat
gcm_utils_get_pixel_format (GdkPixbuf *pixbuf)
{
	guint bits;
	CdPixelFormat format = CD_PIXEL_FORMAT_UNKNOWN;

	/* no alpha channel */
	bits = gdk_pixbuf_get_bits_per_sample (pixbuf);
	if (!gdk_pixbuf_get_has_alpha (pixbuf) && bits == 8) {
		format = CD_PIXEL_FORMAT_RGB24;
		goto out;
	}

	/* alpha channel */
	if (bits == 8) {
		format = CD_PIXEL_FORMAT_RGBA32;
		goto out;
	}
out:
	return format;
}

gboolean
gcm_utils_image_convert (GtkImage *image,
			 CdIcc *input,
			 CdIcc *abstract,
			 CdIcc *output,
			 GError **error)
{
	CdPixelFormat pixel_format;
	g_autoptr(CdTransform) transform = NULL;
	GdkPixbuf *pixbuf;
	GdkPixbuf *original_pixbuf;
	gboolean ret = TRUE;
	guchar *data;
	guint bpp;

	/* get pixbuf */
	pixbuf = gtk_image_get_pixbuf (image);
	if (pixbuf == NULL)
		return FALSE;

	/* work out the pixel format */
	pixel_format = gcm_utils_get_pixel_format (pixbuf);
	if (pixel_format == CD_PIXEL_FORMAT_UNKNOWN) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0, "format not supported");
		return FALSE;
	}

	/* get a copy of the original image, *not* a ref */
	original_pixbuf = g_object_get_data (G_OBJECT (pixbuf), "GcmImageOld");
	if (original_pixbuf == NULL) {
		data = g_memdup (gdk_pixbuf_get_pixels (pixbuf),
				 gdk_pixbuf_get_bits_per_sample (pixbuf) *
				 gdk_pixbuf_get_rowstride (pixbuf) *
				 gdk_pixbuf_get_height (pixbuf) / 8);
		original_pixbuf = gdk_pixbuf_new_from_data (data,
			  gdk_pixbuf_get_colorspace (pixbuf),
			  gdk_pixbuf_get_has_alpha (pixbuf),
			  gdk_pixbuf_get_bits_per_sample (pixbuf),
			  gdk_pixbuf_get_width (pixbuf),
			  gdk_pixbuf_get_height (pixbuf),
			  gdk_pixbuf_get_rowstride (pixbuf),
			  (GdkPixbufDestroyNotify) g_free, NULL);
		g_object_set_data_full (G_OBJECT (pixbuf), "GcmImageOld",
					original_pixbuf,
					(GDestroyNotify) g_object_unref);
	}

	/* convert in-place */
	transform = cd_transform_new ();
	cd_transform_set_input_icc (transform, input);
	cd_transform_set_abstract_icc (transform, abstract);
	cd_transform_set_output_icc (transform, output);
	cd_transform_set_rendering_intent (transform, CD_RENDERING_INTENT_PERCEPTUAL);
	cd_transform_set_input_pixel_format (transform, pixel_format);
	cd_transform_set_output_pixel_format (transform, pixel_format);
	bpp = gdk_pixbuf_get_rowstride (pixbuf) / gdk_pixbuf_get_width (pixbuf);
	ret = cd_transform_process (transform,
				    gdk_pixbuf_get_pixels (original_pixbuf),
				    gdk_pixbuf_get_pixels (pixbuf),
				    gdk_pixbuf_get_width (pixbuf),
				    gdk_pixbuf_get_height (pixbuf),
				    gdk_pixbuf_get_rowstride (pixbuf) / bpp,
				    NULL,
				    error);
	if (!ret)
		return FALSE;

	/* refresh */
	g_object_ref (pixbuf);
	gtk_image_set_from_pixbuf (image, pixbuf);
	g_object_unref (pixbuf);
	return TRUE;
}

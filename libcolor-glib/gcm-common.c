/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU Lesser General Public License Version 2.1
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

/**
 * SECTION:gcm-common
 * @short_description: Common functionality
 *
 * A GObject to use for common shizzle.
 */

#include "config.h"

#include <math.h>
#include <glib-object.h>

#include <gcm-common.h>

/**
 * gcm_color_copy_XYZ:
 * @src: the source color.
 * @dest: the destination color
 *
 * Deep copies a color value.
 **/
void
gcm_color_copy_XYZ (GcmColorXYZ *src, GcmColorXYZ *dest)
{
	dest->X = src->X;
	dest->Y = src->Y;
	dest->Z = src->Z;
}

/**
 * gcm_color_copy_RGB:
 * @src: the source color.
 * @dest: the destination color
 *
 * Deep copies a color value.
 **/
void
gcm_color_copy_RGB (GcmColorRgb *src, GcmColorRgb *dest)
{
	dest->red = src->red;
	dest->green = src->green;
	dest->blue = src->blue;
}

/**
 * gcm_convert_rgb_int_to_rgb:
 **/
void
gcm_convert_rgb_int_to_rgb (GcmColorRgbInt *rgb_int, GcmColorRgb *rgb)
{
	rgb->red = (gdouble) rgb_int->red / 255.0f;
	rgb->green = (gdouble) rgb_int->green / 255.0f;
	rgb->blue = (gdouble) rgb_int->blue / 255.0f;
}

/**
 * gcm_convert_rgb_to_rgb_int:
 **/
void
gcm_convert_rgb_to_rgb_int (GcmColorRgb *rgb, GcmColorRgbInt *rgb_int)
{
	rgb_int->red = (gdouble) rgb->red * 255.0f;
	rgb_int->green = (gdouble) rgb->green * 255.0f;
	rgb_int->blue = (gdouble) rgb->blue * 255.0f;
}

/**
 * gcm_convert_Yxy_to_XYZ:
 **/
void
gcm_convert_Yxy_to_XYZ (GcmColorYxy *src, GcmColorXYZ *dest)
{
	g_assert (src->Y >= 0.0f);
	g_assert (src->x >= 0.0f);
	g_assert (src->y >= 0.0f);
	g_assert (src->Y <= 100.0f);
	g_assert (src->x <= 1.0f);
	g_assert (src->y <= 1.0f);

	/* very small luminance */
	if (src->Y < 1e-6) {
		dest->X = 0.0f;
		dest->Y = 0.0f;
		dest->Z = 0.0f;
		return;
	}

	dest->X = (src->x * src->Y) / src->y;
	dest->Y = src->Y;
	dest->Z = (1.0f - src->x - src->y) * src->Y / src->y;
}

/**
 * gcm_convert_XYZ_to_Yxy:
 **/
void
gcm_convert_XYZ_to_Yxy (GcmColorXYZ *src, GcmColorYxy *dest)
{
	gdouble sum;

	g_assert (src->X >= 0.0f);
	g_assert (src->Y >= 0.0f);
	g_assert (src->Z >= 0.0f);
	g_assert (src->X < 96.0f);
	g_assert (src->Y < 100.0f);
	g_assert (src->Z < 109.0f);

	/* prevent division by zero */
	sum = src->X + src->Y + src->Z;
	if (fabs (sum) < 1e-6) {
		dest->Y = 0.0f;
		dest->x = 0.0f;
		dest->y = 0.0f;
		return;
	}

	dest->Y = src->Y;
	dest->x = src->X / sum;
	dest->y = src->Y / sum;
}

/**
 * gcm_vec3_clear:
 **/
void
gcm_vec3_clear (GcmVec3 *src)
{
	src->v0 = 0.0f;
	src->v1 = 0.0f;
	src->v2 = 0.0f;
}

/**
 * gcm_vec3_scalar_multiply:
 **/
void
gcm_vec3_scalar_multiply (GcmVec3 *src, gdouble value, GcmVec3 *dest)
{
	dest->v0 = src->v0 * value;
	dest->v1 = src->v1 * value;
	dest->v2 = src->v2 * value;
}

/**
 * gcm_vec3_to_string:
 **/
gchar *
gcm_vec3_to_string (GcmVec3 *src)
{
	return g_strdup_printf ("\n/ %0 .3f \\\n"
				"| %0 .3f |\n"
				"\\ %0 .3f /\n",
				src->v0, src->v1, src->v2);
}

/**
 * gcm_vec3_get_data:
 **/
gdouble *
gcm_vec3_get_data (GcmVec3 *src)
{
	return (gdouble *) src;
}

/**
 * gcm_mat33_clear:
 **/
void
gcm_mat33_clear (GcmMat3x3 *dest)
{
	guint i;
	gdouble *temp = (gdouble *) dest;
	for (i=0; i<3*3; i++)
		temp[i] = 0.0f;
}

/**
 * gcm_mat33_to_string:
 **/
gchar *
gcm_mat33_to_string (GcmMat3x3 *src)
{
	return g_strdup_printf ("\n/ %0 .3f  %0 .3f  %0 .3f \\\n"
				"| %0 .3f  %0 .3f  %0 .3f |\n"
				"\\ %0 .3f  %0 .3f  %0 .3f /\n",
				src->m00, src->m01, src->m02,
				src->m10, src->m11, src->m12,
				src->m20, src->m21, src->m22);
}

/**
 * gcm_mat33_get_data:
 **/
gdouble *
gcm_mat33_get_data (GcmMat3x3 *src)
{
	return (gdouble *) src;
}

/**
 * gcm_mat33_set_identity:
 **/
void
gcm_mat33_set_identity (GcmMat3x3 *dest)
{
	gcm_mat33_clear (dest);
	dest->m00 = 1.0f;
	dest->m11 = 1.0f;
	dest->m22 = 1.0f;
}

/**
 * gcm_mat33_vector_multiply:
 **/
void
gcm_mat33_vector_multiply (GcmMat3x3 *mat_src, GcmVec3 *vec_src, GcmVec3 *vec_dest)
{
	vec_dest->v0 = mat_src->m00 * vec_src->v0 +
		       mat_src->m01 * vec_src->v1 +
		       mat_src->m02 * vec_src->v2;
	vec_dest->v1 = mat_src->m10 * vec_src->v0 +
		       mat_src->m11 * vec_src->v1 +
		       mat_src->m12 * vec_src->v2;
	vec_dest->v2 = mat_src->m20 * vec_src->v0 +
		       mat_src->m21 * vec_src->v1 +
		       mat_src->m22 * vec_src->v2;
}

/**
 * gcm_mat33_matrix_multiply:
 **/
void
gcm_mat33_matrix_multiply (GcmMat3x3 *mat_src1, GcmMat3x3 *mat_src2, GcmMat3x3 *mat_dest)
{
	guint8 i, j, k;
	gdouble *src1 = gcm_mat33_get_data (mat_src1);
	gdouble *src2 = gcm_mat33_get_data (mat_src2);
	gdouble *dest = gcm_mat33_get_data (mat_dest);

	for (i=0; i<3; i++) {
		for (j=0; j<3; j++) {
			for (k=0; k<3; k++) {
				dest[3 * i + j] += src1[i * 3 + k] * src2[k * 3 + j];
			}
		}
	}
}

/**
 * gcm_mat33_reciprocal:
 *
 * Return value: FALSE if det is zero (singular)
 **/
gboolean
gcm_mat33_reciprocal (GcmMat3x3 *src, GcmMat3x3 *dest)
{
	double det = 0;

	det  = src->m00 * (src->m11 * src->m22 - src->m12 * src->m21);
	det -= src->m01 * (src->m10 * src->m22 - src->m12 * src->m20);
	det += src->m02 * (src->m10 * src->m21 - src->m11 * src->m20);

	/* division by zero */
	if (fabs (det) < 1e-6)
		return FALSE;

	dest->m00 = (src->m11 * src->m22 - src->m12 * src->m21) / det;
	dest->m01 = (src->m02 * src->m21 - src->m01 * src->m22) / det;
	dest->m02 = (src->m01 * src->m12 - src->m02 * src->m11) / det;

	dest->m10 = (src->m12 * src->m20 - src->m10 * src->m22) / det;
	dest->m11 = (src->m00 * src->m22 - src->m02 * src->m20) / det;
	dest->m12 = (src->m02 * src->m10 - src->m00 * src->m12) / det;

	dest->m20 = (src->m10 * src->m21 - src->m11 * src->m20) / det;
	dest->m21 = (src->m01 * src->m20 - src->m00 * src->m21) / det;
	dest->m22 = (src->m00 * src->m11 - src->m01 * src->m10) / det;

	return TRUE;
}

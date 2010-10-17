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
 * SECTION:gcm-math
 * @short_description: Common maths functionality
 *
 * A GObject to use for common maths functionality like vectors and matrices.
 */

#include "config.h"

#include <math.h>
#include <glib-object.h>

#include <gcm-math.h>

/**
 * gcm_vec3_clear:
 * @src: the source vector
 *
 * Clears a vector, setting all it's values to zero.
 *
 * Since: 2.91.1
 **/
void
gcm_vec3_clear (GcmVec3 *src)
{
	src->v0 = 0.0f;
	src->v1 = 0.0f;
	src->v2 = 0.0f;
}

/**
 * gcm_vec3_init:
 * @dest: the destination vector
 * @v0: component value
 * @v1: component value
 * @v2: component value
 *
 * Initialises a vector.
 *
 * Since: 2.91.1
 **/
void
gcm_vec3_init (GcmVec3 *dest, gdouble v0, gdouble v1, gdouble v2)
{
	g_return_if_fail (dest != NULL);

	dest->v0 = v0;
	dest->v1 = v1;
	dest->v2 = v2;
}

/**
 * gcm_vec3_scalar_multiply:
 * @src: the source
 * @value: the scalar multiplier
 * @dest: the destination
 *
 * Multiplies a vector with a scalar.
 * The arguments @src and @dest can be the same value.
 *
 * Since: 2.91.1
 **/
void
gcm_vec3_scalar_multiply (const GcmVec3 *src, gdouble value, GcmVec3 *dest)
{
	dest->v0 = src->v0 * value;
	dest->v1 = src->v1 * value;
	dest->v2 = src->v2 * value;
}

/**
 * gcm_vec3_add:
 * @src1: the source
 * @src2: the other source
 * @dest: the destination
 *
 * Adds two vector quantaties
 * The arguments @src and @dest can be the same value.
 *
 * Since: 2.91.1
 **/
void
gcm_vec3_add (const GcmVec3 *src1, const GcmVec3 *src2, GcmVec3 *dest)
{
	dest->v0 = src1->v0 + src2->v0;
	dest->v1 = src1->v1 + src2->v1;
	dest->v2 = src1->v2 + src2->v2;
}

/**
 * gcm_vec3_subtract:
 * @src1: the source
 * @src2: the other source
 * @dest: the destination
 *
 * Subtracts one vector quantaty from another
 * The arguments @src and @dest can be the same value.
 *
 * Since: 2.91.1
 **/
void
gcm_vec3_subtract (const GcmVec3 *src1, const GcmVec3 *src2, GcmVec3 *dest)
{
	dest->v0 = src1->v0 - src2->v0;
	dest->v1 = src1->v1 - src2->v1;
	dest->v2 = src1->v2 - src2->v2;
}

/**
 * gcm_vec3_to_string:
 * @src: the source
 *
 * Obtains a string representaton of a vector.
 *
 * Return value: the string. Free with g_free()
 *
 * Since: 2.91.1
 **/
gchar *
gcm_vec3_to_string (const GcmVec3 *src)
{
	return g_strdup_printf ("\n/ %0 .6f \\\n"
				"| %0 .6f |\n"
				"\\ %0 .6f /",
				src->v0, src->v1, src->v2);
}

/**
 * gcm_vec3_get_data:
 * @src: the vector source
 *
 * Gets the raw data for the vector.
 *
 * Return value: the pointer to the data segment.
 *
 * Since: 2.91.1
 **/
gdouble *
gcm_vec3_get_data (const GcmVec3 *src)
{
	return (gdouble *) src;
}

/**
 * gcm_mat33_clear:
 * @src: the source
 *
 * Clears a matrix value, setting all it's values to zero.
 *
 * Since: 2.91.1
 **/
void
gcm_mat33_clear (const GcmMat3x3 *src)
{
	guint i;
	gdouble *temp = (gdouble *) src;
	for (i=0; i<3*3; i++)
		temp[i] = 0.0f;
}

/**
 * gcm_mat33_to_string:
 * @src: the source
 *
 * Obtains a string representaton of a matrix.
 *
 * Return value: the string. Free with g_free()
 *
 * Since: 2.91.1
 **/
gchar *
gcm_mat33_to_string (const GcmMat3x3 *src)
{
	return g_strdup_printf ("\n/ %0 .6f  %0 .6f  %0 .6f \\\n"
				"| %0 .6f  %0 .6f  %0 .6f |\n"
				"\\ %0 .6f  %0 .6f  %0 .6f /",
				src->m00, src->m01, src->m02,
				src->m10, src->m11, src->m12,
				src->m20, src->m21, src->m22);
}

/**
 * gcm_mat33_get_data:
 * @src: the matrix source
 *
 * Gets the raw data for the matrix.
 *
 * Return value: the pointer to the data segment.
 *
 * Since: 2.91.1
 **/
gdouble *
gcm_mat33_get_data (const GcmMat3x3 *src)
{
	return (gdouble *) src;
}

/**
 * gcm_mat33_set_identity:
 * @src: the source
 *
 * Sets the matrix to an identity value.
 *
 * Since: 2.91.1
 **/
void
gcm_mat33_set_identity (GcmMat3x3 *src)
{
	gcm_mat33_clear (src);
	src->m00 = 1.0f;
	src->m11 = 1.0f;
	src->m22 = 1.0f;
}

/**
 * gcm_mat33_vector_multiply:
 * @mat_src: the matrix source
 * @vec_src: the vector source
 * @vec_dest: the destination vector
 *
 * Multiplies a matrix with a vector.
 * The arguments @vec_src and @vec_dest cannot be the same value.
 *
 * Since: 2.91.1
 **/
void
gcm_mat33_vector_multiply (const GcmMat3x3 *mat_src, const GcmVec3 *vec_src, GcmVec3 *vec_dest)
{
	g_return_if_fail (vec_src != vec_dest);
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
 * @mat_src1: the matrix source
 * @mat_src2: the other matrix source
 * @mat_dest: the destination
 *
 * Multiply (convolve) one matrix with another.
 * The arguments @mat_src1 cannot be the same as @mat_dest, and
 * @mat_src2 cannot be the same as @mat_dest.
 *
 * Since: 2.91.1
 **/
void
gcm_mat33_matrix_multiply (const GcmMat3x3 *mat_src1, const GcmMat3x3 *mat_src2, GcmMat3x3 *mat_dest)
{
	guint8 i, j, k;
	gdouble *src1 = gcm_mat33_get_data (mat_src1);
	gdouble *src2 = gcm_mat33_get_data (mat_src2);
	gdouble *dest = gcm_mat33_get_data (mat_dest);
	g_return_if_fail (mat_src1 != mat_dest);
	g_return_if_fail (mat_src2 != mat_dest);

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
 * @src: the source
 * @dest: the destination
 *
 * Inverts the matrix.
 * The arguments @src and @dest cannot be the same value.
 *
 * Return value: %FALSE if det is zero (singular).
 *
 * Since: 2.91.1
 **/
gboolean
gcm_mat33_reciprocal (const GcmMat3x3 *src, GcmMat3x3 *dest)
{
	double det = 0;

	g_return_val_if_fail (src != dest, FALSE);

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

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
 * SECTION:gcm-color
 * @short_description: color functionality
 *
 * Functions to manipulate color.
 */

#include "config.h"

#include <math.h>
#include <glib-object.h>

#include <gcm-color.h>

/**
 * gcm_color_dup_XYZ:
 **/
GcmColorXYZ *
gcm_color_dup_XYZ (const GcmColorXYZ *src)
{
	GcmColorXYZ *dest;
	g_return_val_if_fail (src != NULL, NULL);
	dest = gcm_color_new_XYZ ();
	dest->X = src->X;
	dest->Y = src->Y;
	dest->Z = src->Z;
	return dest;
}

/**
 * gcm_color_dup_RGB:
 **/
GcmColorRGB *
gcm_color_dup_RGB (const GcmColorRGB *src)
{
	GcmColorRGB *dest;
	g_return_val_if_fail (src != NULL, NULL);
	dest = gcm_color_new_RGB ();
	dest->R = src->R;
	dest->G = src->G;
	dest->B = src->B;
	return dest;
}

/**
 * gcm_color_dup_Yxy:
 **/
GcmColorYxy *
gcm_color_dup_Yxy (const GcmColorYxy *src)
{
	GcmColorYxy *dest;
	g_return_val_if_fail (src != NULL, NULL);
	dest = gcm_color_new_Yxy ();
	dest->x = src->x;
	dest->y = src->y;
	return dest;
}

/**
 * gcm_color_get_type_XYZ:
 * Return value: a #GType
 *
 * Gets a specific type.
 **/
GType
gcm_color_get_type_XYZ (void)
{
	static GType type_id = 0;
	if (!type_id)
		type_id = g_boxed_type_register_static ("GcmColorXYZ",
							(GBoxedCopyFunc) gcm_color_dup_XYZ,
							(GBoxedFreeFunc) gcm_color_free_XYZ);
	return type_id;
}

/**
 * gcm_color_get_type_RGB:
 * Return value: a #GType
 *
 * Gets a specific type.
 **/
GType
gcm_color_get_type_RGB (void)
{
	static GType type_id = 0;
	if (!type_id)
		type_id = g_boxed_type_register_static ("GcmColorRGB",
							(GBoxedCopyFunc) gcm_color_dup_RGB,
							(GBoxedFreeFunc) gcm_color_free_RGB);
	return type_id;
}

/**
 * gcm_color_get_type_Yxy:
 * Return value: a #GType
 *
 * Gets a specific type.
 **/
GType
gcm_color_get_type_Yxy (void)
{
	static GType type_id = 0;
	if (!type_id)
		type_id = g_boxed_type_register_static ("GcmColorYxy",
							(GBoxedCopyFunc) gcm_color_dup_Yxy,
							(GBoxedFreeFunc) gcm_color_free_Yxy);
	return type_id;
}

/**
 * gcm_color_set_XYZ:
 * @dest: the destination color
 * @X: component value
 * @Y: component value
 * @Z: component value
 *
 * Initialises a color value.
 **/
void
gcm_color_set_XYZ (GcmColorXYZ *dest, gdouble X, gdouble Y, gdouble Z)
{
	g_return_if_fail (dest != NULL);

	dest->X = X;
	dest->Y = Y;
	dest->Z = Z;
}

/**
 * gcm_color_clear_XYZ:
 * @dest: the destination color
 *
 * Initialises a color value.
 **/
void
gcm_color_clear_XYZ (GcmColorXYZ *dest)
{
	g_return_if_fail (dest != NULL);

	dest->X = 0.0f;
	dest->Y = 0.0f;
	dest->Z = 0.0f;
}

/**
 * gcm_color_set_RGB:
 * @dest: the destination color
 * @R: component value
 * @G: component value
 * @B: component value
 *
 * Initialises a color value.
 **/
void
gcm_color_set_RGB (GcmColorRGB *dest, gdouble R, gdouble G, gdouble B)
{
	g_return_if_fail (dest != NULL);

	dest->R = R;
	dest->G = G;
	dest->B = B;
}

/**
 * gcm_color_copy_XYZ:
 * @src: the source color
 * @dest: the destination color
 *
 * Deep copies a color value.
 **/
void
gcm_color_copy_XYZ (const GcmColorXYZ *src, GcmColorXYZ *dest)
{
	g_return_if_fail (src != NULL);
	g_return_if_fail (dest != NULL);

	dest->X = src->X;
	dest->Y = src->Y;
	dest->Z = src->Z;
}

/**
 * gcm_color_copy_Yxy:
 * @src: the source color
 * @dest: the destination color
 *
 * Deep copies a color value.
 **/
void
gcm_color_copy_Yxy (const GcmColorYxy *src, GcmColorYxy *dest)
{
	g_return_if_fail (src != NULL);
	g_return_if_fail (dest != NULL);

	dest->Y = src->Y;
	dest->x = src->x;
	dest->y = src->y;
}

/**
 * gcm_color_copy_RGB:
 * @src: the source color
 * @dest: the destination color
 *
 * Deep copies a color value.
 **/
void
gcm_color_copy_RGB (const GcmColorRGB *src, GcmColorRGB *dest)
{
	g_return_if_fail (src != NULL);
	g_return_if_fail (dest != NULL);

	dest->R = src->R;
	dest->G = src->G;
	dest->B = src->B;
}

/**
 * gcm_color_convert_RGBint_to_RGB:
 * @src: the source color
 * @dest: the destination color
 *
 * Convert from one color format to another.
 **/
void
gcm_color_convert_RGBint_to_RGB (const GcmColorRGBint *src, GcmColorRGB *dest)
{
	g_return_if_fail (src != NULL);
	g_return_if_fail (dest != NULL);

	dest->R = (gdouble) src->R / 255.0f;
	dest->G = (gdouble) src->G / 255.0f;
	dest->B = (gdouble) src->B / 255.0f;
}

/**
 * gcm_color_convert_RGB_to_RGBint:
 * @src: the source color
 * @dest: the destination color
 *
 * Convert from one color format to another.
 **/
void
gcm_color_convert_RGB_to_RGBint (const GcmColorRGB *src, GcmColorRGBint *dest)
{
	g_return_if_fail (src != NULL);
	g_return_if_fail (dest != NULL);

	dest->R = (gdouble) src->R * 255.0f;
	dest->G = (gdouble) src->G * 255.0f;
	dest->B = (gdouble) src->B * 255.0f;
}

/**
 * gcm_color_convert_Yxy_to_XYZ:
 * @src: the source color
 * @dest: the destination color
 *
 * Convert from one color format to another.
 **/
void
gcm_color_convert_Yxy_to_XYZ (const GcmColorYxy *src, GcmColorXYZ *dest)
{
	g_return_if_fail (src != NULL);
	g_return_if_fail (dest != NULL);

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
 * gcm_color_convert_XYZ_to_Yxy:
 * @src: the source color
 * @dest: the destination color
 *
 * Convert from one color format to another.
 **/
void
gcm_color_convert_XYZ_to_Yxy (const GcmColorXYZ *src, GcmColorYxy *dest)
{
	gdouble sum;

	g_return_if_fail (src != NULL);
	g_return_if_fail (dest != NULL);

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
 * gcm_color_get_XYZ_Vec3:
 * @src: the source color
 *
 * Returns the color as a vector component
 *
 * Return value: the vector.
 **/
GcmVec3	*
gcm_color_get_XYZ_Vec3 (GcmColorXYZ *src)
{
	g_return_val_if_fail (src != NULL, NULL);
	return (GcmVec3 *) src;
}

/**
 * gcm_color_get_RGB_Vec3:
 * @src: the source color
 *
 * Returns the color as a vector component
 *
 * Return value: the vector.
 **/
GcmVec3	*
gcm_color_get_RGB_Vec3 (GcmColorRGB *src)
{
	g_return_val_if_fail (src != NULL, NULL);
	return (GcmVec3 *) src;
}

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
 * gcm_color_copy_XYZ:
 * @src: the source color
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
 * @src: the source color
 * @dest: the destination color
 *
 * Deep copies a color value.
 **/
void
gcm_color_copy_RGB (GcmColorRGB *src, GcmColorRGB *dest)
{
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
gcm_color_convert_RGBint_to_RGB (GcmColorRGBint *rgb_int, GcmColorRGB *rgb)
{
	rgb->R = (gdouble) rgb_int->R / 255.0f;
	rgb->G = (gdouble) rgb_int->G / 255.0f;
	rgb->B = (gdouble) rgb_int->B / 255.0f;
}

/**
 * gcm_color_convert_RGB_to_RGBint:
 * @src: the source color
 * @dest: the destination color
 *
 * Convert from one color format to another.
 **/
void
gcm_color_convert_RGB_to_RGBint (GcmColorRGB *rgb, GcmColorRGBint *rgb_int)
{
	rgb_int->R = (gdouble) rgb->R * 255.0f;
	rgb_int->G = (gdouble) rgb->G * 255.0f;
	rgb_int->B = (gdouble) rgb->B * 255.0f;
}

/**
 * gcm_color_convert_Yxy_to_XYZ:
 * @src: the source color
 * @dest: the destination color
 *
 * Convert from one color format to another.
 **/
void
gcm_color_convert_Yxy_to_XYZ (GcmColorYxy *src, GcmColorXYZ *dest)
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
 * gcm_color_convert_XYZ_to_Yxy:
 * @src: the source color
 * @dest: the destination color
 *
 * Convert from one color format to another.
 **/
void
gcm_color_convert_XYZ_to_Yxy (GcmColorXYZ *src, GcmColorYxy *dest)
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

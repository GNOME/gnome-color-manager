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
 * SECTION:gcm-enum
 * @short_description: Common routines to convert enumerated values to strings and vice versa.
 *
 * Functions to convert enumerated values to strings and vice versa.
 */

#include "config.h"

#include "gcm-enum.h"

/**
 * gcm_colorspace_to_string:
 **/
const gchar *
gcm_colorspace_to_string (GcmColorspace colorspace)
{
	if (colorspace == GCM_COLORSPACE_XYZ)
		return "xyz";
	if (colorspace == GCM_COLORSPACE_LAB)
		return "lab";
	if (colorspace == GCM_COLORSPACE_LUV)
		return "luv";
	if (colorspace == GCM_COLORSPACE_YCBCR)
		return "ycbcr";
	if (colorspace == GCM_COLORSPACE_YXY)
		return "yxy";
	if (colorspace == GCM_COLORSPACE_RGB)
		return "rgb";
	if (colorspace == GCM_COLORSPACE_GRAY)
		return "gray";
	if (colorspace == GCM_COLORSPACE_HSV)
		return "hsv";
	if (colorspace == GCM_COLORSPACE_CMYK)
		return "cmyk";
	if (colorspace == GCM_COLORSPACE_CMY)
		return "cmy";
	return "unknown";
}

/**
 * gcm_colorspace_from_string:
 **/
GcmColorspace
gcm_colorspace_from_string (const gchar *colorspace)
{
	if (g_strcmp0 (colorspace, "xyz") == 0)
		return GCM_COLORSPACE_XYZ;
	if (g_strcmp0 (colorspace, "lab") == 0)
		return GCM_COLORSPACE_LAB;
	if (g_strcmp0 (colorspace, "luv") == 0)
		return GCM_COLORSPACE_LUV;
	if (g_strcmp0 (colorspace, "ycbcr") == 0)
		return GCM_COLORSPACE_YCBCR;
	if (g_strcmp0 (colorspace, "yxy") == 0)
		return GCM_COLORSPACE_YXY;
	if (g_strcmp0 (colorspace, "rgb") == 0)
		return GCM_COLORSPACE_RGB;
	if (g_strcmp0 (colorspace, "gray") == 0)
		return GCM_COLORSPACE_GRAY;
	if (g_strcmp0 (colorspace, "hsv") == 0)
		return GCM_COLORSPACE_HSV;
	if (g_strcmp0 (colorspace, "cmyk") == 0)
		return GCM_COLORSPACE_CMYK;
	if (g_strcmp0 (colorspace, "cmy") == 0)
		return GCM_COLORSPACE_CMY;
	return GCM_COLORSPACE_UNKNOWN;
}


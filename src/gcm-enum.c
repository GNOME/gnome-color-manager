/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009 Richard Hughes <richard@hughsie.com>
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
 * GNU General Public License for more utils.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include <glib/gi18n.h>

#include "gcm-enum.h"

/**
 * gcm_intent_enum_to_string:
 **/
const gchar *
gcm_intent_enum_to_string (GcmIntentEnum intent)
{
	if (intent == GCM_INTENT_ENUM_PERCEPTUAL)
		return "perceptual";
	if (intent == GCM_INTENT_ENUM_RELATIVE_COLORMETRIC)
		return "relative-colormetric";
	if (intent == GCM_INTENT_ENUM_SATURATION)
		return "saturation";
	if (intent == GCM_INTENT_ENUM_ABSOLUTE_COLORMETRIC)
		return "absolute-colormetric";
	return "unknown";
}

/**
 * gcm_intent_enum_from_string:
 **/
GcmIntentEnum
gcm_intent_enum_from_string (const gchar *intent)
{
	if (g_strcmp0 (intent, "perceptual") == 0)
		return GCM_INTENT_ENUM_PERCEPTUAL;
	if (g_strcmp0 (intent, "relative-colormetric") == 0)
		return GCM_INTENT_ENUM_RELATIVE_COLORMETRIC;
	if (g_strcmp0 (intent, "saturation") == 0)
		return GCM_INTENT_ENUM_SATURATION;
	if (g_strcmp0 (intent, "absolute-colormetric") == 0)
		return GCM_INTENT_ENUM_ABSOLUTE_COLORMETRIC;
	return GCM_INTENT_ENUM_UNKNOWN;
}

/**
 * gcm_profile_type_enum_to_string:
 **/
const gchar *
gcm_profile_type_enum_to_string (GcmProfileTypeEnum type)
{
	if (type == GCM_PROFILE_TYPE_ENUM_INPUT_DEVICE)
		return "input-device";
	if (type == GCM_PROFILE_TYPE_ENUM_DISPLAY_DEVICE)
		return "display-device";
	if (type == GCM_PROFILE_TYPE_ENUM_OUTPUT_DEVICE)
		return "output-device";
	if (type == GCM_PROFILE_TYPE_ENUM_DEVICELINK)
		return "devicelink";
	if (type == GCM_PROFILE_TYPE_ENUM_COLORSPACE_CONVERSION)
		return "colorspace-conversion";
	if (type == GCM_PROFILE_TYPE_ENUM_ABSTRACT)
		return "abstract";
	if (type == GCM_PROFILE_TYPE_ENUM_NAMED_COLOR)
		return "named-color";
	return "unknown";
}

/**
 * gcm_colorspace_enum_to_string:
 **/
const gchar *
gcm_colorspace_enum_to_string (GcmColorspaceEnum type)
{
	if (type == GCM_COLORSPACE_ENUM_XYZ)
		return "xyz";
	if (type == GCM_COLORSPACE_ENUM_LAB)
		return "lab";
	if (type == GCM_COLORSPACE_ENUM_LUV)
		return "luv";
	if (type == GCM_COLORSPACE_ENUM_YCBCR)
		return "ycbcr";
	if (type == GCM_COLORSPACE_ENUM_YXY)
		return "yxy";
	if (type == GCM_COLORSPACE_ENUM_RGB)
		return "rgb";
	if (type == GCM_COLORSPACE_ENUM_GRAY)
		return "gray";
	if (type == GCM_COLORSPACE_ENUM_HSV)
		return "hsv";
	if (type == GCM_COLORSPACE_ENUM_CMYK)
		return "cmyk";
	if (type == GCM_COLORSPACE_ENUM_CMY)
		return "cmy";
	return "unknown";
}


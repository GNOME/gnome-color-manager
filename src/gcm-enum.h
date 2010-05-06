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

#ifndef __GCM_ENUM_H
#define __GCM_ENUM_H

#include <glib/gi18n.h>

typedef enum {
	GCM_INTENT_UNKNOWN,
	GCM_INTENT_PERCEPTUAL,
	GCM_INTENT_RELATIVE_COLORMETRIC,
	GCM_INTENT_SATURATION,
	GCM_INTENT_ABSOLUTE_COLORMETRIC,
	GCM_INTENT_LAST
} GcmIntent;

typedef enum {
	GCM_PROFILE_KIND_UNKNOWN,
	GCM_PROFILE_KIND_INPUT_DEVICE,
	GCM_PROFILE_KIND_DISPLAY_DEVICE,
	GCM_PROFILE_KIND_OUTPUT_DEVICE,
	GCM_PROFILE_KIND_DEVICELINK,
	GCM_PROFILE_KIND_COLORSPACE_CONVERSION,
	GCM_PROFILE_KIND_ABSTRACT,
	GCM_PROFILE_KIND_NAMED_COLOR,
	GCM_PROFILE_KIND_LAST
} GcmProfileKind;

typedef enum {
	GCM_COLORSPACE_UNKNOWN,
	GCM_COLORSPACE_XYZ,
	GCM_COLORSPACE_LAB,
	GCM_COLORSPACE_LUV,
	GCM_COLORSPACE_YCBCR,
	GCM_COLORSPACE_YXY,
	GCM_COLORSPACE_RGB,
	GCM_COLORSPACE_GRAY,
	GCM_COLORSPACE_HSV,
	GCM_COLORSPACE_CMYK,
	GCM_COLORSPACE_CMY,
	GCM_COLORSPACE_LAST
} GcmColorspace;

typedef enum {
	GCM_DEVICE_KIND_UNKNOWN,
	GCM_DEVICE_KIND_DISPLAY,
	GCM_DEVICE_KIND_SCANNER,
	GCM_DEVICE_KIND_PRINTER,
	GCM_DEVICE_KIND_CAMERA,
	GCM_DEVICE_KIND_LAST
} GcmDeviceKind;

GcmIntent	 gcm_intent_from_string			(const gchar		*intent);
const gchar	*gcm_intent_to_string			(GcmIntent		 intent);
const gchar	*gcm_profile_kind_to_string		(GcmProfileKind		 profile_kind);
const gchar	*gcm_colorspace_to_string		(GcmColorspace		 colorspace);
const gchar	*gcm_colorspace_to_localised_string	(GcmColorspace	 colorspace);
GcmColorspace	 gcm_colorspace_from_string		(const gchar		*colorspace);
GcmDeviceKind	 gcm_device_kind_from_string		(const gchar		*kind);
const gchar	*gcm_device_kind_to_string		(GcmDeviceKind		 kind);

#endif /* __GCM_ENUM_H */


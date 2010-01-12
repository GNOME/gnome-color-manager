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
 * GNU General Public License for more utils.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __GCM_ENUM_H
#define __GCM_ENUM_H

#include <glib/gi18n.h>

typedef enum {
	GCM_INTENT_ENUM_UNKNOWN,
	GCM_INTENT_ENUM_PERCEPTUAL,
	GCM_INTENT_ENUM_RELATIVE_COLORMETRIC,
	GCM_INTENT_ENUM_SATURATION,
	GCM_INTENT_ENUM_ABSOLUTE_COLORMETRIC,
	GCM_INTENT_ENUM_LAST
} GcmIntentEnum;

typedef enum {
	GCM_PROFILE_TYPE_ENUM_UNKNOWN,
	GCM_PROFILE_TYPE_ENUM_INPUT_DEVICE,
	GCM_PROFILE_TYPE_ENUM_DISPLAY_DEVICE,
	GCM_PROFILE_TYPE_ENUM_OUTPUT_DEVICE,
	GCM_PROFILE_TYPE_ENUM_DEVICELINK,
	GCM_PROFILE_TYPE_ENUM_COLORSPACE_CONVERSION,
	GCM_PROFILE_TYPE_ENUM_ABSTRACT,
	GCM_PROFILE_TYPE_ENUM_NAMED_COLOR,
	GCM_PROFILE_TYPE_ENUM_LAST
} GcmProfileTypeEnum;

typedef enum {
	GCM_COLORSPACE_ENUM_UNKNOWN,
	GCM_COLORSPACE_ENUM_XYZ,
	GCM_COLORSPACE_ENUM_LAB,
	GCM_COLORSPACE_ENUM_LUV,
	GCM_COLORSPACE_ENUM_YCBCR,
	GCM_COLORSPACE_ENUM_YXY,
	GCM_COLORSPACE_ENUM_RGB,
	GCM_COLORSPACE_ENUM_GRAY,
	GCM_COLORSPACE_ENUM_HSV,
	GCM_COLORSPACE_ENUM_CMYK,
	GCM_COLORSPACE_ENUM_CMY,
	GCM_COLORSPACE_ENUM_LAST
} GcmColorspaceEnum;

typedef enum {
	GCM_DEVICE_TYPE_ENUM_UNKNOWN,
	GCM_DEVICE_TYPE_ENUM_DISPLAY,
	GCM_DEVICE_TYPE_ENUM_SCANNER,
	GCM_DEVICE_TYPE_ENUM_PRINTER,
	GCM_DEVICE_TYPE_ENUM_CAMERA,
	GCM_DEVICE_TYPE_ENUM_LAST
} GcmDeviceTypeEnum;

GcmIntentEnum		 gcm_intent_enum_from_string		(const gchar		*intent);
const gchar		*gcm_intent_enum_to_string		(GcmIntentEnum		 intent);
const gchar		*gcm_profile_type_enum_to_string	(GcmProfileTypeEnum	 profile_type);
const gchar		*gcm_colorspace_enum_to_string		(GcmColorspaceEnum	 colorspace);
GcmDeviceTypeEnum	 gcm_device_type_enum_from_string	(const gchar		*type);
const gchar		*gcm_device_type_enum_to_string		(GcmDeviceTypeEnum	 type);

#endif /* __GCM_ENUM_H */


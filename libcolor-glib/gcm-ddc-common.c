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
 * SECTION:gcm-ddc-common
 * @short_description: Common functionality for DDC/CI objects
 *
 * A GObject to use for common shizzle.
 */

#include "config.h"

#include <glib-object.h>

#include <gcm-ddc-common.h>

typedef struct {
	guchar		 index;
	const gchar	*shortname;
} GcmDescription;

static const GcmDescription vcp_descriptions[] = {
//	{ 0x00, "degauss" },
	{ 0x01,	"degauss" },
	{ 0x02,	"secondary-degauss" },
	{ 0x04,	"reset-factory-defaults" },
	{ 0x05,	"reset-brightness-and-contrast" },
	{ 0x06,	"reset-factory-geometry" },
	{ 0x08,	"reset-factory-default-color" },
	{ 0x0a,	"reset-factory-default-position" },
	{ 0x0c,	"reset-factory-default-size" },
	{ 0x0e, "image-lock-coarse" },
	{ 0x10, "brightness" },
	{ 0x12, "contrast" },
	{ 0x13, "backlight" },
	{ 0x14,	"select-color-preset" },
	{ 0x16,	"red-video-gain" },
	{ 0x18,	"green-video-gain" },
	{ 0x1a,	"blue-video-gain" },
	{ 0x1c, "hue" },
	{ 0x1e,	"auto-size-center" },
	{ 0x20,	"horizontal-position" },
	{ 0x22,	"horizontal-size" },
	{ 0x24,	"horizontal-pincushion" },
	{ 0x26,	"horizontal-pincushion-balance" },
	{ 0x28,	"horizontal-misconvergence" },
	{ 0x2a,	"horizontal-linearity" },
	{ 0x2c,	"horizontal-linearity-balance" },
	{ 0x30,	"vertical-position" },
	{ 0x32,	"vertical-size" },
	{ 0x34,	"vertical-pincushion" },
	{ 0x36,	"vertical-pincushion-balance" },
	{ 0x38,	"vertical-misconvergence" },
	{ 0x3a,	"vertical-linearity" },
	{ 0x3c,	"vertical-linearity-balance" },
	{ 0x3e,	"image-lock-fine" },
	{ 0x40,	"parallelogram-distortion" },
	{ 0x42,	"trapezoidal-distortion" },
	{ 0x44, "tilt" },
	{ 0x46,	"top-corner-distortion-control" },
	{ 0x48,	"top-corner-distortion-balance" },
	{ 0x4a,	"bottom-corner-distortion-control" },
	{ 0x4c,	"bottom-corner-distortion-balance" },
	{ 0x50,	"hue" },
	{ 0x52,	"saturation" },
	{ 0x54, "color-temp" },
	{ 0x56,	"horizontal-moire" },
	{ 0x58,	"vertical-moire" },
	{ 0x5a, "auto-size" },
	{ 0x5c,	"landing-adjust" },
	{ 0x5e,	"input-level-select" },
	{ 0x60,	"input-source-select" },
	{ 0x62,	"audio-speaker-volume-adjust" },
	{ 0x64,	"audio-microphone-volume-adjust" },
	{ 0x66,	"on-screen-displa" },
	{ 0x68,	"language-select" },
	{ 0x6c,	"red-video-black-level" },
	{ 0x6e,	"green-video-black-level" },
	{ 0x70,	"blue-video-black-level" },
	{ 0x8c, "sharpness" },
	{ 0x94, "mute" },
	{ 0xa2,	"auto-size-center" },
	{ 0xa4,	"polarity-horizontal-synchronization" },
	{ 0xa6,	"polarity-vertical-synchronization" },
	{ 0xa8,	"synchronization-type" },
	{ 0xaa,	"screen-orientation" },
	{ 0xac,	"horizontal-frequency" },
	{ 0xae,	"vertical-frequency" },
	{ 0xb0,	"settings" },
	{ 0xca,	"on-screen-display" },
	{ 0xcc,	"samsung-on-screen-display-language" },
	{ 0xc9,	"firmware-version" },
	{ 0xd4,	"stereo-mode" },
	{ 0xd6,	"dpms-control-(1---on/4---stby)" },
	{ 0xdc,	"magicbright-(1---text/2---internet/3---entertain/4---custom)" },
	{ 0xdf,	"vcp-version" },
	{ 0xe0,	"samsung-color-preset-(0---normal/1---warm/2---cool)" },
	{ 0xe1,	"power-control-(0---off/1---on)" },
	{ 0xe2, "auto-source" },
	{ 0xe8, "tl-purity" },
	{ 0xe9, "tr-purity" },
	{ 0xea, "bl-purity" },
	{ 0xeb, "br-purity" },
	{ 0xed,	"samsung-red-video-black-level" },
	{ 0xee,	"samsung-green-video-black-level" },
	{ 0xef,	"samsung-blue-video-black-level" },
	{ 0xf0, "magic-color" },
	{ 0xf1, "fe-brightness" },
	{ 0xf2, "fe-clarity / gamma" },
	{ 0xf3, "fe-color" },
	{ 0xf5, "samsung-osd" },
	{ 0xf6, "resolutionnotifier" },
	{ 0xf9, "super-bright" },
	{ 0xfc, "fe-mode" },
	{ 0xfd, "power-led" },
	{ GCM_VCP_ID_INVALID, NULL }
	};

/**
 * gcm_get_vcp_description_from_index:
 *
 * Since: 0.0.1
 **/
const gchar *
gcm_get_vcp_description_from_index (guchar idx)
{
	guint i;

	g_return_val_if_fail (idx != GCM_VCP_ID_INVALID, NULL);

	for (i=0;;i++) {
		if (vcp_descriptions[i].index == idx ||
		    vcp_descriptions[i].index == GCM_VCP_ID_INVALID)
			break;
	}
	return vcp_descriptions[i].shortname;
}

/**
 * gcm_get_vcp_index_from_description:
 *
 * Since: 0.0.1
 **/
guchar
gcm_get_vcp_index_from_description (const gchar *description)
{
	guint i;

	g_return_val_if_fail (description != NULL, GCM_VCP_ID_INVALID);

	for (i=0;;i++) {
		if (g_strcmp0 (vcp_descriptions[i].shortname, description) == 0 ||
		    vcp_descriptions[i].shortname == NULL)
			break;
	}
	return vcp_descriptions[i].index;
}

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

#ifndef __GCM_UTILS_H
#define __GCM_UTILS_H

#include <glib-object.h>
#include <gtk/gtk.h>

#include "gcm-device.h"
#include "gcm-profile.h"
#include "gcm-enum.h"

#define GCM_STOCK_ICON					"gnome-color-manager"
#define GCM_DBUS_SERVICE				"org.gnome.ColorManager"
#define GCM_DBUS_INTERFACE				"org.gnome.ColorManager"
#define GCM_DBUS_PATH					"/org/gnome/ColorManager"

#define GCM_SETTINGS_SCHEMA				"org.gnome.color-manager"
#define GCM_SETTINGS_DEFAULT_GAMMA			"default-gamma"
#define GCM_SETTINGS_GLOBAL_DISPLAY_CORRECTION		"global-display-correction"
#define GCM_SETTINGS_SET_ICC_PROFILE_ATOM		"set-icc-profile-atom"
#define GCM_SETTINGS_RENDERING_INTENT_DISPLAY		"rendering-intent-display"
#define GCM_SETTINGS_RENDERING_INTENT_SOFTPROOF		"rendering-intent-softproof"
#define GCM_SETTINGS_COLORSPACE_RGB			"colorspace-rgb"
#define GCM_SETTINGS_COLORSPACE_CMYK			"colorspace-cmyk"
#define GCM_SETTINGS_COLORSPACE_GRAY			"colorspace-gray"
#define GCM_SETTINGS_CALIBRATION_LENGTH			"calibration-length"
#define GCM_SETTINGS_SHOW_FINE_TUNING			"show-fine-tuning"
#define GCM_SETTINGS_SHOW_NOTIFICATIONS			"show-notifications"
#define GCM_SETTINGS_MIGRATE_CONFIG_VERSION		"migrate-config-version"
#define GCM_SETTINGS_RECALIBRATE_PRINTER_THRESHOLD	"recalibrate-printer-threshold"
#define GCM_SETTINGS_RECALIBRATE_DISPLAY_THRESHOLD	"recalibrate-display-threshold"
#define GCM_SETTINGS_ENABLE_SANE			"enable-sane"
#define GCM_SETTINGS_PROFILE_GRAPH_TYPE			"profile-graph-type"

#define GCM_CONFIG_VERSION_ORIGINAL			0
#define GCM_CONFIG_VERSION_SHARED_SPEC			1

/* DISTROS: you will have to patch if you have changed the name of these packages */
#define GCM_PREFS_PACKAGE_NAME_SHARED_COLOR_TARGETS	"shared-color-targets"
#define GCM_PREFS_PACKAGE_NAME_ARGYLLCMS		"argyllcms"
#define GCM_PREFS_PACKAGE_NAME_COLOR_PROFILES		"shared-color-profiles"
#define GCM_PREFS_PACKAGE_NAME_COLOR_PROFILES_EXTRA	"shared-color-profiles-extra"

gboolean	 gcm_utils_mkdir_for_filename		(const gchar		*filename,
							 GError			**error);
gboolean	 gcm_utils_mkdir_and_copy		(GFile			*source,
							 GFile			*destination,
							 GError			**error);
GFile		*gcm_utils_get_profile_destination	(GFile			*file);
gchar		**gcm_utils_ptr_array_to_strv		(GPtrArray		*array);
gboolean	 gcm_gnome_help				(const gchar		*link_id);
gboolean	 gcm_utils_output_is_lcd_internal	(const gchar		*output_name);
gboolean	 gcm_utils_output_is_lcd		(const gchar		*output_name);
void		 gcm_utils_alphanum_lcase		(gchar			*string);
void		 gcm_utils_ensure_sensible_filename	(gchar			*string);
gchar		*gcm_utils_get_default_config_location	(void);
CdProfileKind	 gcm_utils_device_kind_to_profile_kind	(CdDeviceKind		 kind);
gboolean	 gcm_utils_install_package		(const gchar		*package_name,
							 GtkWindow		*window);
gboolean	 gcm_utils_is_icc_profile		(GFile			*file);
gchar		*gcm_utils_linkify			(const gchar		*text);
const gchar	*gcm_intent_to_localized_text		(GcmIntent	 intent);
const gchar	*gcm_intent_to_localized_description	(GcmIntent	 intent);
const gchar	*gcm_colorspace_to_localised_string	(GcmColorspace	 colorspace);

#endif /* __GCM_UTILS_H */


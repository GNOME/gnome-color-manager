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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __GCM_UTILS_H
#define __GCM_UTILS_H

#include <glib-object.h>
#include <gtk/gtk.h>

#include "gcm-device.h"
#include "gcm-profile.h"

#define GCM_STOCK_ICON				"gnome-color-manager"
#define GCM_PROFILE_PATH			"/.color/icc"

#define GCM_SETTINGS_DIR			"/apps/gnome-color-manager"
#define GCM_SETTINGS_DEFAULT_GAMMA		"/apps/gnome-color-manager/default_gamma"
#define GCM_SETTINGS_GLOBAL_DISPLAY_CORRECTION	"/apps/gnome-color-manager/global_display_correction"
#define GCM_SETTINGS_SET_ICC_PROFILE_ATOM	"/apps/gnome-color-manager/set_icc_profile_atom"
#define GCM_SETTINGS_OUTPUT_INTENT		"/apps/gnome-color-manager/output_intent"

gboolean	 gcm_utils_set_gamma_for_device		(GcmDevice		*device,
							 GError			**error);
GPtrArray	*gcm_utils_get_profile_filenames	(void);
gboolean	 gcm_utils_mkdir_and_copy		(const gchar		*source,
							 const gchar		*destination,
							 GError			**error);
gchar		*gcm_utils_get_profile_destination	(const gchar		*filename);
gchar		**gcm_utils_ptr_array_to_strv		(GPtrArray		*array);
gboolean	 gcm_gnome_help				(const gchar		*link_id);
gboolean	 gcm_utils_output_is_lcd_internal	(const gchar		*output_name);
gboolean	 gcm_utils_output_is_lcd		(const gchar		*output_name);
void		 gcm_utils_alphanum_lcase		(gchar			*string);
void		 gcm_utils_ensure_sensible_filename	(gchar			*string);
gchar		*gcm_utils_get_default_config_location	(void);
GcmProfileType	 gcm_utils_device_type_to_profile_type	(GcmDeviceType		 type);
gchar		*gcm_utils_format_date_time		(const struct tm	*created);
gboolean	 gcm_utils_install_package		(const gchar		*package_name,
							 GtkWindow		*window);
void		 gcm_utils_ensure_sane_length		(gchar			*text,
							 guint			 max_length);
void		 gcm_utils_ensure_printable		(gchar			*text);
gboolean	 gcm_utils_is_icc_profile		(const gchar		*filename);

#endif /* __GCM_UTILS_H */


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

#define GCM_STOCK_ICON					"gnome-color-manager"
#define GCM_DBUS_SERVICE				"org.gnome.ColorManager"
#define GCM_DBUS_INTERFACE				"org.gnome.ColorManager"
#define GCM_DBUS_PATH					"/org/gnome/ColorManager"

/* DISTROS: you will have to patch if you have changed the name of these packages */
#define GCM_PREFS_PACKAGE_NAME_SHARED_COLOR_TARGETS	"shared-color-targets"
#define GCM_PREFS_PACKAGE_NAME_ARGYLLCMS		"argyllcms"
#define GCM_PREFS_PACKAGE_NAME_COLOR_PROFILES		"shared-color-profiles"
#define GCM_PREFS_PACKAGE_NAME_COLOR_PROFILES_EXTRA	"shared-color-profiles-extra"

GFile		*gcm_utils_get_profile_destination	(GFile			*file);
gchar		**gcm_utils_ptr_array_to_strv		(GPtrArray		*array);
gboolean	 gcm_gnome_help				(const gchar		*link_id);
gboolean	 gcm_utils_output_is_lcd_internal	(const gchar		*output_name);
gboolean	 gcm_utils_output_is_lcd		(const gchar		*output_name);
void		 gcm_utils_alphanum_lcase		(gchar			*string);
void		 gcm_utils_ensure_sensible_filename	(gchar			*string);
gboolean	 gcm_utils_install_package		(const gchar		*package_name,
							 GtkWindow		*window);
gchar		*gcm_utils_linkify			(const gchar		*text);
const gchar	*cd_colorspace_to_localised_string	(CdColorspace		 colorspace);
gboolean	 gcm_utils_image_convert		(GtkImage		*image,
							 CdIcc			*input,
							 CdIcc			*abstract,
							 CdIcc			*output,
							 GError			**error);

#endif /* __GCM_UTILS_H */


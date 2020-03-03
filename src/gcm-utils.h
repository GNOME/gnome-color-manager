/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009-2010 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#pragma once

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

gchar		*gcm_utils_linkify			(const gchar		*text);
const gchar	*cd_colorspace_to_localised_string	(CdColorspace		 colorspace);
gboolean	 gcm_utils_image_convert		(GtkImage		*image,
							 CdIcc			*input,
							 CdIcc			*abstract,
							 CdIcc			*output,
							 GError			**error);


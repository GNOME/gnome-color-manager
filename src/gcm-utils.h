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

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnomeui/gnome-rr.h>

#include <X11/extensions/Xrandr.h>

#include "gcm-clut.h"

//TODO: needs to be a function
#define GCM_PROFILE_LOCATION	"./.gnome2/gnome-color-manager/config.dat"
#define GCM_STOCK_ICON		"video-display"
#define GCM_PROFILE_PATH	"/.color/icc"

gchar		*gcm_utils_get_output_name		(GnomeRROutput		*output);
guint		 gcm_utils_get_gamma_size		(GnomeRRCrtc		*crtc,
							 GError			**error);
gboolean	 gcm_utils_set_crtc_gamma		(GnomeRRCrtc		*crtc,
							 GcmClut		*clut,
							 GError			**error);
gboolean	 gcm_utils_set_output_gamma		(GnomeRROutput		*output,
							 const gchar		*config,
							 GError			**error);
GcmClut		*gcm_utils_get_clut_for_output		(GnomeRROutput		*output,
							 const gchar		*config,
							 GError			**error);
GPtrArray	*gcm_utils_get_profile_filenames	(void);

#endif /* __GCM_UTILS_H */


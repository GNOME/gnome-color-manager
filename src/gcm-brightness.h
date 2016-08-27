/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009-2015 Richard Hughes <richard@hughsie.com>
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

#ifndef __GCM_BRIGHTNESS_H
#define __GCM_BRIGHTNESS_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GCM_TYPE_BRIGHTNESS (gcm_brightness_get_type ())
G_DECLARE_DERIVABLE_TYPE (GcmBrightness, gcm_brightness, GCM, BRIGHTNESS, GObject)

struct _GcmBrightnessClass
{
	GObjectClass	parent_class;
};

GcmBrightness	*gcm_brightness_new			(void);
gboolean	 gcm_brightness_set_percentage		(GcmBrightness		*brightness,
							 guint			 percentage,
							 GError			**error);
gboolean	 gcm_brightness_get_percentage		(GcmBrightness		*brightness,
							 guint			*percentage,
							 GError			**error);

G_END_DECLS

#endif /* __GCM_BRIGHTNESS_H */


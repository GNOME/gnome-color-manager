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

#ifndef __GCM_BRIGHTNESS_H
#define __GCM_BRIGHTNESS_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GCM_TYPE_BRIGHTNESS		(gcm_brightness_get_type ())
#define GCM_BRIGHTNESS(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GCM_TYPE_BRIGHTNESS, GcmBrightness))
#define GCM_BRIGHTNESS_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), GCM_TYPE_BRIGHTNESS, GcmBrightnessClass))
#define GCM_IS_BRIGHTNESS(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GCM_TYPE_BRIGHTNESS))
#define GCM_IS_BRIGHTNESS_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GCM_TYPE_BRIGHTNESS))
#define GCM_BRIGHTNESS_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GCM_TYPE_BRIGHTNESS, GcmBrightnessClass))

typedef struct _GcmBrightnessPrivate	GcmBrightnessPrivate;
typedef struct _GcmBrightness		GcmBrightness;
typedef struct _GcmBrightnessClass	GcmBrightnessClass;

struct _GcmBrightness
{
	 GObject		 parent;
	 GcmBrightnessPrivate	*priv;
};

struct _GcmBrightnessClass
{
	GObjectClass	parent_class;
	/* padding for future expansion */
	void (*_gcm_reserved1) (void);
	void (*_gcm_reserved2) (void);
	void (*_gcm_reserved3) (void);
	void (*_gcm_reserved4) (void);
	void (*_gcm_reserved5) (void);
};

GType		 gcm_brightness_get_type		(void);
GcmBrightness	*gcm_brightness_new			(void);
gboolean	 gcm_brightness_set_percentage		(GcmBrightness		*brightness,
							 guint			 percentage,
							 GError			**error);
gboolean	 gcm_brightness_get_percentage		(GcmBrightness		*brightness,
							 guint			*percentage,
							 GError			**error);

G_END_DECLS

#endif /* __GCM_BRIGHTNESS_H */


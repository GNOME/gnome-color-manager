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

#ifndef __GCM_DEVICE_XRANDR_H
#define __GCM_DEVICE_XRANDR_H

#include <glib-object.h>
#include <libgnomeui/gnome-rr.h>

#include "gcm-device.h"

G_BEGIN_DECLS

#define GCM_TYPE_DEVICE_XRANDR		(gcm_device_xrandr_get_type ())
#define GCM_DEVICE_XRANDR(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GCM_TYPE_DEVICE_XRANDR, GcmDeviceXrandr))
#define GCM_IS_DEVICE_XRANDR(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GCM_TYPE_DEVICE_XRANDR))

typedef struct _GcmDeviceXrandrPrivate	GcmDeviceXrandrPrivate;
typedef struct _GcmDeviceXrandr		GcmDeviceXrandr;
typedef struct _GcmDeviceXrandrClass	GcmDeviceXrandrClass;

struct _GcmDeviceXrandr
{
	 GcmDevice			 parent;
	 GcmDeviceXrandrPrivate		*priv;
};

struct _GcmDeviceXrandrClass
{
	GcmDeviceClass	parent_class;
};

GType		 gcm_device_xrandr_get_type		(void);
GcmDevice	*gcm_device_xrandr_new			(void);
gboolean	 gcm_device_xrandr_set_from_output	(GcmDevice		*device,
							 GnomeRROutput		*output,
							 GError			**error);
void		 gcm_device_xrandr_set_remove_atom	(GcmDeviceXrandr	*device_xrandr,
							 gboolean		 remove_atom);
const gchar	*gcm_device_xrandr_get_native_device	(GcmDeviceXrandr	*device_xrandr);
gboolean	 gcm_device_xrandr_get_fallback		(GcmDeviceXrandr	*device_xrandr);

G_END_DECLS

#endif /* __GCM_DEVICE_XRANDR_H */


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
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __GCM_DEVICE_H
#define __GCM_DEVICE_H

#include <glib-object.h>

#include "gcm-enum.h"

G_BEGIN_DECLS

#define GCM_TYPE_DEVICE			(gcm_device_get_type ())
#define GCM_DEVICE(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), GCM_TYPE_DEVICE, GcmDevice))
#define GCM_DEVICE_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), GCM_TYPE_DEVICE, GcmDeviceClass))
#define GCM_IS_DEVICE(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GCM_TYPE_DEVICE))
#define GCM_IS_DEVICE_CLASS(k)		(G_TYPE_CHECK_CLASS_TYPE ((k), GCM_TYPE_DEVICE))
#define GCM_DEVICE_GET_CLASS(o)		(G_TYPE_INSTANCE_GET_CLASS ((o), GCM_TYPE_DEVICE, GcmDeviceClass))

typedef struct _GcmDevicePrivate	GcmDevicePrivate;
typedef struct _GcmDevice		GcmDevice;
typedef struct _GcmDeviceClass		GcmDeviceClass;

struct _GcmDevice
{
	 GObject			 parent;
	 GcmDevicePrivate		*priv;
};

struct _GcmDeviceClass
{
	GObjectClass	parent_class;
	gboolean	 (*apply)			(GcmDevice		*device,
							 GError			**error);
	/* padding for future expansion */
	void (*_gcm_reserved1) (void);
	void (*_gcm_reserved2) (void);
	void (*_gcm_reserved3) (void);
	void (*_gcm_reserved4) (void);
	void (*_gcm_reserved5) (void);
};

GType			 gcm_device_get_type		  	(void);
gboolean		 gcm_device_load			(GcmDevice	*device,
								 GError		**error);
gboolean		 gcm_device_save			(GcmDevice	*device,
								 GError		**error);
gboolean		 gcm_device_apply			(GcmDevice	*device,
								 GError		**error);

/* accessors */
GcmDeviceTypeEnum	 gcm_device_get_kind			(GcmDevice	*device);
void			 gcm_device_set_kind			(GcmDevice	*device,
								 GcmDeviceTypeEnum kind);
gboolean		 gcm_device_get_connected		(GcmDevice	*device);
void			 gcm_device_set_connected		(GcmDevice	*device,
								 gboolean	 connected);
gboolean		 gcm_device_get_virtual			(GcmDevice	*device);
void			 gcm_device_set_virtual			(GcmDevice	*device,
								 gboolean	 virtual);
gboolean		 gcm_device_get_saved			(GcmDevice	*device);
void			 gcm_device_set_saved			(GcmDevice	*device,
								 gboolean	 saved);
gfloat			 gcm_device_get_gamma			(GcmDevice	*device);
void			 gcm_device_set_gamma			(GcmDevice	*device,
								 gfloat		 gamma);
gfloat			 gcm_device_get_brightness		(GcmDevice	*device);
void			 gcm_device_set_brightness		(GcmDevice	*device,
								 gfloat		 brightness);
gfloat			 gcm_device_get_contrast		(GcmDevice	*device);
void			 gcm_device_set_contrast		(GcmDevice	*device,
								 gfloat		 contrast);
GcmColorspaceEnum	 gcm_device_get_colorspace		(GcmDevice	*device);
void			 gcm_device_set_colorspace		(GcmDevice	*device,
								 GcmColorspaceEnum colorspace);
const gchar		*gcm_device_get_id			(GcmDevice	*device);
void			 gcm_device_set_id			(GcmDevice	*device,
								 const gchar 	*id);
const gchar		*gcm_device_get_serial			(GcmDevice	*device);
void			 gcm_device_set_serial			(GcmDevice	*device,
								 const gchar 	*serial);
const gchar		*gcm_device_get_manufacturer		(GcmDevice	*device);
void			 gcm_device_set_manufacturer		(GcmDevice	*device,
								 const gchar 	*manufacturer);
const gchar		*gcm_device_get_model			(GcmDevice	*device);
void			 gcm_device_set_model			(GcmDevice	*device,
								 const gchar 	*model);
const gchar		*gcm_device_get_title			(GcmDevice	*device);
void			 gcm_device_set_title			(GcmDevice	*device,
								 const gchar 	*title);
const gchar		*gcm_device_get_profile_filename	(GcmDevice	*device);
void			 gcm_device_set_profile_filename	(GcmDevice	*device,
								 const gchar 	*profile_filename);

G_END_DECLS

#endif /* __GCM_DEVICE_H */


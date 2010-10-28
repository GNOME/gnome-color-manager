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

#ifndef __GCM_DEVICE_H
#define __GCM_DEVICE_H

#include <glib-object.h>

#include "gcm-enum.h"
#include "gcm-profile.h"

G_BEGIN_DECLS

#define GCM_TYPE_DEVICE			(gcm_device_get_type ())
#define GCM_DEVICE(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), GCM_TYPE_DEVICE, GcmDevice))
#define GCM_DEVICE_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), GCM_TYPE_DEVICE, GcmDeviceClass))
#define GCM_IS_DEVICE(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GCM_TYPE_DEVICE))
#define GCM_IS_DEVICE_CLASS(k)		(G_TYPE_CHECK_CLASS_TYPE ((k), GCM_TYPE_DEVICE))
#define GCM_DEVICE_GET_CLASS(o)		(G_TYPE_INSTANCE_GET_CLASS ((o), GCM_TYPE_DEVICE, GcmDeviceClass))

#define GCM_DEVICE_ERROR		1
#define GCM_DEVICE_ERROR_INTERNAL	0
#define GCM_DEVICE_ERROR_NO_SUPPPORT	0

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
	void		 (*changed)			(GcmDevice		*device);
	gboolean	 (*apply)			(GcmDevice		*device,
							 GError			**error);
	GcmProfile	*(*generate_profile)		(GcmDevice		*device,
							 GError			**error);
	gchar		*(*get_config_data)		(GcmDevice		*device);
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
GcmProfile		*gcm_device_generate_profile		(GcmDevice	*device,
								 GError		**error);

/* accessors */
GcmDeviceKind		 gcm_device_get_kind			(GcmDevice	*device);
void			 gcm_device_set_kind			(GcmDevice	*device,
								 GcmDeviceKind kind);
gboolean		 gcm_device_get_connected		(GcmDevice	*device);
void			 gcm_device_set_connected		(GcmDevice	*device,
								 gboolean	 connected);
gboolean		 gcm_device_get_virtual			(GcmDevice	*device);
void			 gcm_device_set_virtual			(GcmDevice	*device,
								 gboolean	 virtual);
gboolean		 gcm_device_get_use_edid_profile	(GcmDevice	*device);
void			 gcm_device_set_use_edid_profile	(GcmDevice	*device,
								 gboolean	 use_edid_profile);
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
GcmColorspace		 gcm_device_get_colorspace		(GcmDevice	*device);
void			 gcm_device_set_colorspace		(GcmDevice	*device,
								 GcmColorspace colorspace);
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
GPtrArray		*gcm_device_get_profiles		(GcmDevice	*device);
void			 gcm_device_set_profiles		(GcmDevice	*device,
								 GPtrArray	*profiles);
gboolean		 gcm_device_profile_add			(GcmDevice	*device,
								 GcmProfile 	*profile,
								 GError		**error);
gboolean		 gcm_device_profile_remove		(GcmDevice	*device,
								 GcmProfile 	*profile,
								 GError		**error);
gboolean		 gcm_device_profile_set_default		(GcmDevice	*device,
								 GcmProfile 	*profile,
								 GError		**error);
glong			 gcm_device_get_modified_time		(GcmDevice	*device);

/* helpers */
GcmProfile		*gcm_device_get_default_profile		(GcmDevice	*device);
const gchar		*gcm_device_get_default_profile_filename (GcmDevice	*device);
void			 gcm_device_set_default_profile_filename (GcmDevice	*device,
								 const gchar 	*profile_filename);

G_END_DECLS

#endif /* __GCM_DEVICE_H */


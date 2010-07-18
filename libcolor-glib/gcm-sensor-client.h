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

#ifndef __GCM_SENSOR_CLIENT_H
#define __GCM_SENSOR_CLIENT_H

#include <glib-object.h>

#include "gcm-sensor.h"

G_BEGIN_DECLS

#define GCM_TYPE_SENSOR_CLIENT		(gcm_sensor_client_get_type ())
#define GCM_SENSOR_CLIENT(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GCM_TYPE_SENSOR_CLIENT, GcmSensorClient))
#define GCM_SENSOR_CLIENT_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), GCM_TYPE_SENSOR_CLIENT, GcmSensorClientClass))
#define GCM_IS_SENSOR_CLIENT(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GCM_TYPE_SENSOR_CLIENT))
#define GCM_IS_SENSOR_CLIENT_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GCM_TYPE_SENSOR_CLIENT))
#define GCM_SENSOR_CLIENT_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GCM_TYPE_SENSOR_CLIENT, GcmSensorClientClass))

typedef struct _GcmSensorClientPrivate	GcmSensorClientPrivate;
typedef struct _GcmSensorClient		GcmSensorClient;
typedef struct _GcmSensorClientClass	GcmSensorClientClass;

struct _GcmSensorClient
{
	 GObject			 parent;
	 GcmSensorClientPrivate		*priv;
};

struct _GcmSensorClientClass
{
	GObjectClass	parent_class;
	void		(* changed)			(void);
	/* padding for future expansion */
	void (*_gcm_reserved1) (void);
	void (*_gcm_reserved2) (void);
	void (*_gcm_reserved3) (void);
	void (*_gcm_reserved4) (void);
	void (*_gcm_reserved5) (void);
};

GType			 gcm_sensor_client_get_type		(void);
GcmSensorClient		*gcm_sensor_client_new			(void);

/* accessors */
GcmSensor		*gcm_sensor_client_get_sensor		(GcmSensorClient	*sensor_client);
gboolean		 gcm_sensor_client_get_present		(GcmSensorClient	*sensor_client);

G_END_DECLS

#endif /* __GCM_SENSOR_CLIENT_H */


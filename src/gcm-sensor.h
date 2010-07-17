/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
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

#ifndef __GCM_SENSOR_H
#define __GCM_SENSOR_H

#include <glib-object.h>
#include <libcolor-glib.h>

G_BEGIN_DECLS

#define GCM_TYPE_SENSOR		(gcm_sensor_get_type ())
#define GCM_SENSOR(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GCM_TYPE_SENSOR, GcmSensor))
#define GCM_SENSOR_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), GCM_TYPE_SENSOR, GcmSensorClass))
#define GCM_IS_SENSOR(o)	(G_TYPE_CHECK_INSTANCE_TYPE ((o), GCM_TYPE_SENSOR))
#define GCM_IS_SENSOR_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GCM_TYPE_SENSOR))
#define GCM_SENSOR_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GCM_TYPE_SENSOR, GcmSensorClass))

typedef struct _GcmSensorPrivate	GcmSensorPrivate;
typedef struct _GcmSensor		GcmSensor;
typedef struct _GcmSensorClass		GcmSensorClass;

struct _GcmSensor
{
	 GObject			 parent;
	 GcmSensorPrivate		*priv;
};

struct _GcmSensorClass
{
	GObjectClass	parent_class;
	/* vtable */
	gboolean	 (*get_ambient)			(GcmSensor	*sensor,
							 gdouble	*value,
							 GError		**error);
	gboolean	 (*set_leds)			(GcmSensor	*sensor,
							 guint8		 value,
							 GError		**error);
	gboolean	 (*sample)			(GcmSensor	*sensor,
							 GcmColorXYZ	*value,
							 GError		**error);
	gboolean	 (*startup)			(GcmSensor	*sensor,
							 GError		**error);
	/* padding for future expansion */
	void (*_gcm_reserved1) (void);
	void (*_gcm_reserved2) (void);
	void (*_gcm_reserved3) (void);
	void (*_gcm_reserved4) (void);
	void (*_gcm_reserved5) (void);
};

typedef enum
{
	GCM_SENSOR_ERROR_USER_ABORT,
	GCM_SENSOR_ERROR_NO_SUPPORT,
	GCM_SENSOR_ERROR_NO_DATA,
	GCM_SENSOR_ERROR_INTERNAL
} GcmSensorError;

/* dummy */
#define GCM_SENSOR_ERROR	1

GType		 gcm_sensor_get_type			(void);
GcmSensor	*gcm_sensor_new				(void);

gboolean	 gcm_sensor_get_ambient			(GcmSensor	*sensor,
							 gdouble	*value,
							 GError		**error);
gboolean	 gcm_sensor_set_leds			(GcmSensor	*sensor,
							 guint8		 value,
							 GError		**error);
gboolean	 gcm_sensor_sample			(GcmSensor	*sensor,
							 GcmColorXYZ	*value,
							 GError		**error);
gboolean	 gcm_sensor_startup			(GcmSensor	*sensor,
							 GError		**error);

G_END_DECLS

#endif /* __GCM_SENSOR_H */


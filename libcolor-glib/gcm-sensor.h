/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU Lesser General Public License Version 2.1
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#if !defined (__LIBCOLOR_GLIB_H_INSIDE__) && !defined (LIBCOLOR_GLIB_COMPILATION)
#error "Only <libcolor-glib.h> can be included directly."
#endif

#ifndef __GCM_SENSOR_H
#define __GCM_SENSOR_H

#include <glib-object.h>
#include <gudev/gudev.h>
#include <gio/gio.h>
#include <gcm-color.h>

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
	void		 (*get_ambient_async)		(GcmSensor	*sensor,
							 GCancellable	*cancellable,
							 GAsyncResult	*res);
	gboolean	 (*get_ambient_finish)		(GcmSensor	*sensor,
							 GAsyncResult	*res,
							 gdouble	*value,
							 GError		**error);
	void		 (*sample_async)		(GcmSensor	*sensor,
							 GCancellable	*cancellable,
							 GAsyncResult	*res);
	gboolean	 (*sample_finish)		(GcmSensor	*sensor,
							 GAsyncResult	*res,
							 GcmColorXYZ	*value,
							 GError		**error);
	gboolean	 (*set_leds)			(GcmSensor	*sensor,
							 guint8		 value,
							 GError		**error);
	gboolean	 (*dump)			(GcmSensor	*sensor,
							 GString	*data,
							 GError		**error);
	/* signals */
	void		(* button_pressed)		(void);
	/* padding for future expansion */
	void (*_gcm_reserved1) (void);
	void (*_gcm_reserved2) (void);
	void (*_gcm_reserved3) (void);
	void (*_gcm_reserved4) (void);
	void (*_gcm_reserved5) (void);
};

/**
 * GcmSensorError:
 *
 * The error code.
 **/
typedef enum {
	GCM_SENSOR_ERROR_USER_ABORT,
	GCM_SENSOR_ERROR_NO_SUPPORT,
	GCM_SENSOR_ERROR_NO_DATA,
	GCM_SENSOR_ERROR_INTERNAL
} GcmSensorError;

/**
 * GcmSensorOutputType:
 *
 * The output type.
 **/
typedef enum {
	GCM_SENSOR_OUTPUT_TYPE_UNKNOWN,
	GCM_SENSOR_OUTPUT_TYPE_LCD,
	GCM_SENSOR_OUTPUT_TYPE_CRT,
	GCM_SENSOR_OUTPUT_TYPE_PROJECTOR
} GcmSensorOutputType;

/**
 * GcmSensorKind:
 *
 * The sensor type.
 **/
typedef enum {
	GCM_SENSOR_KIND_HUEY,
	GCM_SENSOR_KIND_COLOR_MUNKI,
	GCM_SENSOR_KIND_SPYDER,
	GCM_SENSOR_KIND_DTP20,
	GCM_SENSOR_KIND_DTP22,
	GCM_SENSOR_KIND_DTP41,
	GCM_SENSOR_KIND_DTP51,
	GCM_SENSOR_KIND_DTP94,
	GCM_SENSOR_KIND_SPECTRO_SCAN,
	GCM_SENSOR_KIND_I1_PRO,
	GCM_SENSOR_KIND_COLORIMTRE_HCFR,
	GCM_SENSOR_KIND_UNKNOWN
} GcmSensorKind;

/**
 * GcmSensorState:
 *
 * The state of the sensor.
 **/
typedef enum {
	GCM_SENSOR_STATE_STARTING,
	GCM_SENSOR_STATE_IDLE,
	GCM_SENSOR_STATE_MEASURING
} GcmSensorState;

/* dummy */
#define GCM_SENSOR_ERROR	1

GType			 gcm_sensor_get_type		(void);
GcmSensor		*gcm_sensor_new			(void);

void			 gcm_sensor_button_pressed	(GcmSensor		*sensor);
void			 gcm_sensor_set_state		(GcmSensor		*sensor,
							 GcmSensorState		 state);
GcmSensorState		 gcm_sensor_get_state		(GcmSensor		*sensor);
gboolean		 gcm_sensor_dump		(GcmSensor		*sensor,
							 GString		*data,
							 GError			**error);
gboolean		 gcm_sensor_set_leds		(GcmSensor		*sensor,
							 guint8			 value,
							 GError			**error);
gboolean		 gcm_sensor_set_from_device	(GcmSensor		*sensor,
							 GUdevDevice		*device,
							 GError			**error);
void			 gcm_sensor_set_output_type	(GcmSensor		*sensor,
							 GcmSensorOutputType	 output_type);
GcmSensorOutputType	 gcm_sensor_get_output_type	(GcmSensor		*sensor);
void			 gcm_sensor_set_serial_number	(GcmSensor		*sensor,
							 const gchar		*serial_number);
const gchar		*gcm_sensor_get_serial_number	(GcmSensor		*sensor);
const gchar		*gcm_sensor_get_model		(GcmSensor		*sensor);
const gchar		*gcm_sensor_get_vendor		(GcmSensor		*sensor);
GcmSensorKind		 gcm_sensor_get_kind		(GcmSensor		*sensor);
gboolean		 gcm_sensor_supports_display	(GcmSensor 		*sensor);
gboolean		 gcm_sensor_supports_projector	(GcmSensor 		*sensor);
gboolean		 gcm_sensor_supports_printer	(GcmSensor		*sensor);
gboolean		 gcm_sensor_supports_spot	(GcmSensor		*sensor);
gboolean		 gcm_sensor_is_native		(GcmSensor		*sensor);
const gchar		*gcm_sensor_kind_to_string	(GcmSensorKind		 sensor_kind);
GcmSensorKind		 gcm_sensor_kind_from_string	(const gchar		*sensor_kind);
const gchar		*gcm_sensor_get_image_display	(GcmSensor		*sensor);
const gchar		*gcm_sensor_get_image_calibrate	(GcmSensor		*sensor);
const gchar		*gcm_sensor_get_image_spotread	(GcmSensor		*sensor);

/* async sampling functions that take a lot of time */
void			 gcm_sensor_get_ambient_async	(GcmSensor		*sensor,
							 GCancellable		*cancellable,
							 GAsyncReadyCallback	 callback,
							 gpointer		 user_data);
gboolean		 gcm_sensor_get_ambient_finish	(GcmSensor		*sensor,
							 GAsyncResult		*res,
							 gdouble		*value,
							 GError			**error);
void			 gcm_sensor_sample_async	(GcmSensor		*sensor,
							 GCancellable		*cancellable,
							 GAsyncReadyCallback	 callback,
							 gpointer		 user_data);
gboolean		 gcm_sensor_sample_finish	(GcmSensor		*sensor,
							 GAsyncResult		*res,
							 GcmColorXYZ		*value,
							 GError			**error);

/* sync versions */
gboolean		 gcm_sensor_get_ambient		(GcmSensor		*sensor,
							 GCancellable		*cancellable,
							 gdouble		*value,
							 GError			**error);
gboolean		 gcm_sensor_sample		(GcmSensor		*sensor,
							 GCancellable		*cancellable,
							 GcmColorXYZ		*value,
							 GError			**error);

G_END_DECLS

#endif /* __GCM_SENSOR_H */


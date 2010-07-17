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

#if !defined (__GCM_H_INSIDE__) && !defined (GCM_COMPILATION)
#error "Only <gcm.h> can be included directly."
#endif

#ifndef __GCM_DDC_DEVICE_H
#define __GCM_DDC_DEVICE_H

#include <glib-object.h>
#include <gio/gio.h>

#include <gcm-ddc-common.h>
#include <gcm-ddc-device.h>

G_BEGIN_DECLS

#define GCM_TYPE_DDC_DEVICE		(gcm_ddc_device_get_type ())
#define GCM_DDC_DEVICE(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GCM_TYPE_DDC_DEVICE, GcmDdcDevice))
#define GCM_DDC_DEVICE_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), GCM_TYPE_DDC_DEVICE, GcmDdcDeviceClass))
#define GCM_IS_DDC_DEVICE(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GCM_TYPE_DDC_DEVICE))
#define GCM_IS_DDC_DEVICE_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GCM_TYPE_DDC_DEVICE))
#define GCM_DDC_DEVICE_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GCM_TYPE_DDC_DEVICE, GcmDdcDeviceClass))
#define GCM_DDC_DEVICE_ERROR		(gcm_ddc_device_error_quark ())
#define GCM_DDC_DEVICE_TYPE_ERROR	(gcm_ddc_device_error_get_type ())

/**
 * GcmDdcDeviceError:
 * @GCM_DDC_DEVICE_ERROR_FAILED: the transaction failed for an unknown reason
 *
 * Errors that can be thrown
 */
typedef enum
{
	GCM_DDC_DEVICE_ERROR_FAILED
} GcmDdcDeviceError;

typedef struct _GcmDdcDevicePrivate		GcmDdcDevicePrivate;
typedef struct _GcmDdcDevice			GcmDdcDevice;
typedef struct _GcmDdcDeviceClass		GcmDdcDeviceClass;

struct _GcmDdcDevice
{
	 GObject		 parent;
	 GcmDdcDevicePrivate	*priv;
};

struct _GcmDdcDeviceClass
{
	GObjectClass	parent_class;

	/* signals */
	void		(* changed)			(GcmDdcDevice	*device);
	/* padding for future expansion */
	void (*_gcm_reserved1) (void);
	void (*_gcm_reserved2) (void);
	void (*_gcm_reserved3) (void);
	void (*_gcm_reserved4) (void);
	void (*_gcm_reserved5) (void);
};

typedef enum {
	GCM_DDC_DEVICE_KIND_LCD,
	GCM_DDC_DEVICE_KIND_CRT,
	GCM_DDC_DEVICE_KIND_UNKNOWN
} GcmDdcDeviceKind;

/* incest */
#include <gcm-ddc-control.h>

GQuark		 gcm_ddc_device_error_quark		(void);
GType		 gcm_ddc_device_get_type		  	(void);
GcmDdcDevice	*gcm_ddc_device_new			(void);

gboolean	 gcm_ddc_device_open			(GcmDdcDevice	*device,
							 const gchar	*filename,
							 GError		**error);
gboolean	 gcm_ddc_device_close			(GcmDdcDevice	*device,
							 GError		**error);
const guint8	*gcm_ddc_device_get_edid		(GcmDdcDevice	*device,
							 gsize		*length,
							 GError		**error);
const gchar	*gcm_ddc_device_get_edid_md5		(GcmDdcDevice	*device,
							 GError		**error);
gboolean	 gcm_ddc_device_write			(GcmDdcDevice	*device,
							 guchar		 *data,
							 gsize		 length,
							 GError		**error);
gboolean	 gcm_ddc_device_read			(GcmDdcDevice	*device,
							 guchar		*data,
							 gsize		 data_length,
							 gsize		*recieved_length,
							 GError		**error);
gboolean	 gcm_ddc_device_save			(GcmDdcDevice	*device,
							 GError		**error);
const gchar	*gcm_ddc_device_get_pnpid		(GcmDdcDevice	*device,
							 GError		**error);
const gchar	*gcm_ddc_device_get_model		(GcmDdcDevice	*device,
							 GError		**error);
GcmDdcDeviceKind gcm_ddc_device_get_kind		(GcmDdcDevice	*device,
							 GError		**error);
GPtrArray	*gcm_ddc_device_get_controls		(GcmDdcDevice	*device,
							 GError		**error);
GcmDdcControl	*gcm_ddc_device_get_control_by_id	(GcmDdcDevice	*device,
							 guchar		 id,
							 GError		**error);
void		 gcm_ddc_device_set_verbose		(GcmDdcDevice	*device,
							 GcmVerbose verbose);

G_END_DECLS

#endif /* __GCM_DDC_DEVICE_H */


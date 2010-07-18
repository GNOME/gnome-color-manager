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

#ifndef __GCM_DDC_CONTROL_H
#define __GCM_DDC_CONTROL_H

#include <glib-object.h>
#include <gio/gio.h>

#include <gcm-ddc-common.h>
#include <gcm-ddc-control.h>
#include <gcm-ddc-device.h>

G_BEGIN_DECLS

#define GCM_TYPE_DDC_CONTROL		(gcm_ddc_control_get_type ())
#define GCM_DDC_CONTROL(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GCM_TYPE_DDC_CONTROL, GcmDdcControl))
#define GCM_DDC_CONTROL_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), GCM_TYPE_DDC_CONTROL, GcmDdcControlClass))
#define GCM_IS_DDC_CONTROL(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GCM_TYPE_DDC_CONTROL))
#define GCM_IS_DDC_CONTROL_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GCM_TYPE_DDC_CONTROL))
#define GCM_DDC_CONTROL_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GCM_TYPE_DDC_CONTROL, GcmDdcControlClass))
#define GCM_DDC_CONTROL_ERROR		(gcm_ddc_control_error_quark ())
#define GCM_DDC_CONTROL_TYPE_ERROR	(gcm_ddc_control_error_get_type ())

/**
 * GcmDdcControlError:
 * @GCM_DDC_CONTROL_ERROR_FAILED: the transaction failed for an unknown reason
 *
 * Errors that can be thrown
 */
typedef enum
{
	GCM_DDC_CONTROL_ERROR_FAILED
} GcmDdcControlError;

typedef struct _GcmDdcControlPrivate		GcmDdcControlPrivate;
typedef struct _GcmDdcControl			GcmDdcControl;
typedef struct _GcmDdcControlClass		GcmDdcControlClass;

struct _GcmDdcControl
{
	 GObject		 parent;
	 GcmDdcControlPrivate	*priv;
};

struct _GcmDdcControlClass
{
	GObjectClass	parent_class;
	/* padding for future expansion */
	void (*_gcm_reserved1) (void);
	void (*_gcm_reserved2) (void);
	void (*_gcm_reserved3) (void);
	void (*_gcm_reserved4) (void);
	void (*_gcm_reserved5) (void);
};

typedef struct {
	guint			 id;
	GPtrArray		*int_values;
} GcmDdcControlCap;

/* control numbers */
#define GCM_DDC_CONTROL_ID_BRIGHTNESS			0x10

GQuark		 gcm_ddc_control_error_quark		(void);
GType		 gcm_ddc_control_get_type		(void);
GcmDdcControl	*gcm_ddc_control_new			(void);

void		 gcm_ddc_control_parse			(GcmDdcControl	*control,
							 guchar		 id,
							 const gchar	*values);
void		 gcm_ddc_control_set_device		(GcmDdcControl	*control,
							 GcmDdcDevice	*device);
void		 gcm_ddc_control_set_verbose		(GcmDdcControl	*control,
							 GcmVerbose verbose);
gboolean	 gcm_ddc_control_run			(GcmDdcControl	*control,
							 GError		**error);
gboolean	 gcm_ddc_control_request			(GcmDdcControl	*control,
							 guint16	*value,
							 guint16	*maximum,
							 GError		**error);
gboolean	 gcm_ddc_control_set			(GcmDdcControl	*control,
							 guint16	 value,
							 GError		**error);
gboolean	 gcm_ddc_control_reset			(GcmDdcControl	*control,
							 GError		**error);
guchar		 gcm_ddc_control_get_id			(GcmDdcControl	*control);
const gchar	*gcm_ddc_control_get_description		(GcmDdcControl	*control);
GArray		*gcm_ddc_control_get_values		(GcmDdcControl	*control);

G_END_DECLS

#endif /* __GCM_DDC_CONTROL_H */


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

#ifndef __GCM_DDC_CLIENT_H
#define __GCM_DDC_CLIENT_H

#include <glib-object.h>
#include <gio/gio.h>

#include <gcm-ddc-common.h>
#include <gcm-ddc-device.h>
#include <gcm-ddc-client.h>

G_BEGIN_DECLS

#define GCM_TYPE_DDC_CLIENT		(gcm_ddc_client_get_type ())
#define GCM_DDC_CLIENT(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GCM_TYPE_DDC_CLIENT, GcmDdcClient))
#define GCM_DDC_CLIENT_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), GCM_TYPE_DDC_CLIENT, GcmDdcClientClass))
#define GCM_IS_DDC_CLIENT(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GCM_TYPE_DDC_CLIENT))
#define GCM_IS_DDC_CLIENT_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GCM_TYPE_DDC_CLIENT))
#define GCM_DDC_CLIENT_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GCM_TYPE_DDC_CLIENT, GcmDdcClientClass))
#define GCM_DDC_CLIENT_ERROR		(gcm_ddc_client_error_quark ())
#define GCM_DDC_CLIENT_TYPE_ERROR	(gcm_ddc_client_error_get_type ())

/**
 * GcmDdcClientError:
 * @GCM_DDC_CLIENT_ERROR_FAILED: the transaction failed for an unknown reason
 *
 * Errors that can be thrown
 */
typedef enum
{
	GCM_DDC_CLIENT_ERROR_FAILED
} GcmDdcClientError;

typedef struct _GcmDdcClientPrivate		GcmDdcClientPrivate;
typedef struct _GcmDdcClient			GcmDdcClient;
typedef struct _GcmDdcClientClass		GcmDdcClientClass;

struct _GcmDdcClient
{
	 GObject		 parent;
	 GcmDdcClientPrivate	*priv;
};

struct _GcmDdcClientClass
{
	GObjectClass	parent_class;

	/* signals */
	void		(* changed)			(GcmDdcClient	*client);
	/* padding for future expansion */
	void (*_gcm_reserved1) (void);
	void (*_gcm_reserved2) (void);
	void (*_gcm_reserved3) (void);
	void (*_gcm_reserved4) (void);
	void (*_gcm_reserved5) (void);
};

GQuark		 gcm_ddc_client_error_quark		(void);
GType		 gcm_ddc_client_get_type		  	(void);
GcmDdcClient	*gcm_ddc_client_new			(void);

gboolean	 gcm_ddc_client_close			(GcmDdcClient		*client,
							 GError			**error);
GPtrArray	*gcm_ddc_client_get_devices		(GcmDdcClient		*client,
							 GError			**error);
GcmDdcDevice	*gcm_ddc_client_get_device_from_edid	(GcmDdcClient		*client,
							 const gchar		*edid_md5,
							 GError			**error);
void		 gcm_ddc_client_set_verbose		(GcmDdcClient		*client,
							 GcmVerbose		 verbose);

G_END_DECLS

#endif /* __GCM_DDC_CLIENT_H */


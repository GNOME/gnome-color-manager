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

#include "config.h"

#include <glib-object.h>

#include "gcm-device-virtual.h"
#include "gcm-enum.h"
#include "gcm-utils.h"

#include "egg-debug.h"

#define GCM_DEVICE_VIRTUAL_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GCM_TYPE_DEVICE_VIRTUAL, GcmDeviceVirtualPrivate))

G_DEFINE_TYPE (GcmDeviceVirtual, gcm_device_virtual, GCM_TYPE_DEVICE)

/**
 * gcm_device_virtual_create_from_params:
 **/
gboolean
gcm_device_virtual_create_from_params (GcmDeviceVirtual *device_virtual,
				       GcmDeviceTypeEnum device_type,
				       const gchar      *model,
				       const gchar      *manufacturer,
				       GcmColorspaceEnum colorspace)
{
	gchar *id;
	gchar *title;

	/* make some stuff up */
	title = g_strdup_printf ("%s - %s", manufacturer, model);
	id = g_strdup_printf ("%s_%s", manufacturer, model);
	gcm_utils_alphanum_lcase (id);

	/* create the device */
	egg_debug ("adding %s '%s'", id, title);
	g_object_set (device_virtual,
		      "connected", FALSE,
		      "type", device_type,
		      "id", id,
		      "manufacturer", manufacturer,
		      "model", model,
		      "title", title,
		      "colorspace", colorspace,
		      "virtual", TRUE,
		      NULL);
	g_free (id);
	g_free (title);
	return TRUE;
}

/**
 * gcm_device_virtual_class_init:
 **/
static void
gcm_device_virtual_class_init (GcmDeviceVirtualClass *klass)
{
}

/**
 * gcm_device_virtual_init:
 **/
static void
gcm_device_virtual_init (GcmDeviceVirtual *device_virtual)
{
}

/**
 * gcm_device_virtual_new:
 *
 * Return value: a new #GcmDevice object.
 **/
GcmDevice *
gcm_device_virtual_new (void)
{
	GcmDevice *device;
	device = g_object_new (GCM_TYPE_DEVICE_VIRTUAL, NULL);
	return GCM_DEVICE (device);
}


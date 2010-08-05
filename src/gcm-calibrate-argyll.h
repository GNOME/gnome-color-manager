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

#ifndef __GCM_CALIBRATE_ARGYLL_H
#define __GCM_CALIBRATE_ARGYLL_H

#include <glib-object.h>
#include <gtk/gtk.h>

#include "gcm-calibrate.h"

G_BEGIN_DECLS

#define GCM_TYPE_CALIBRATE_ARGYLL		(gcm_calibrate_argyll_get_type ())
#define GCM_CALIBRATE_ARGYLL(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), GCM_TYPE_CALIBRATE_ARGYLL, GcmCalibrateArgyll))
#define GCM_IS_CALIBRATE_ARGYLL(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GCM_TYPE_CALIBRATE_ARGYLL))

typedef struct _GcmCalibrateArgyllPrivate	GcmCalibrateArgyllPrivate;
typedef struct _GcmCalibrateArgyll		GcmCalibrateArgyll;
typedef struct _GcmCalibrateArgyllClass		GcmCalibrateArgyllClass;

struct _GcmCalibrateArgyll
{
	 GcmCalibrate				 parent;
	 GcmCalibrateArgyllPrivate		*priv;
};

struct _GcmCalibrateArgyllClass
{
	GcmCalibrateClass	parent_class;
};

GType		 	 gcm_calibrate_argyll_get_type			(void);
GcmCalibrate		*gcm_calibrate_argyll_new			(void);

G_END_DECLS

#endif /* __GCM_CALIBRATE_ARGYLL_H */


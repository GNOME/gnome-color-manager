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

#ifndef __GCM_CALIBRATE_ARGYLL_H
#define __GCM_CALIBRATE_ARGYLL_H

#include <glib-object.h>
#include <gtk/gtk.h>

#include "gcm-calibrate.h"

G_BEGIN_DECLS

#define GCM_TYPE_CALIBRATE_ARGYLL		(gcm_calibrate_argyll_get_type ())
#define GCM_CALIBRATE_ARGYLL(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), GCM_TYPE_CALIBRATE_ARGYLL, GcmCalibrateArgyll))
#define GCM_CALIBRATE_ARGYLL_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), GCM_TYPE_CALIBRATE_ARGYLL, GcmCalibrateArgyllClass))
#define GCM_IS_CALIBRATE_ARGYLL(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GCM_TYPE_CALIBRATE_ARGYLL))
#define GCM_IS_CALIBRATE_ARGYLL_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GCM_TYPE_CALIBRATE_ARGYLL))
#define GCM_CALIBRATE_ARGYLL_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GCM_TYPE_CALIBRATE_ARGYLL, GcmCalibrateArgyllClass))

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
	/* padding for future expansion */
	void (*_gcm_reserved1) (void);
	void (*_gcm_reserved2) (void);
	void (*_gcm_reserved3) (void);
	void (*_gcm_reserved4) (void);
	void (*_gcm_reserved5) (void);
};

typedef enum {
	GCM_CALIBRATE_ARGYLL_REFERENCE_KIND_CMP_DIGITAL_TARGET_3,
	GCM_CALIBRATE_ARGYLL_REFERENCE_KIND_CMP_DT_003,
	GCM_CALIBRATE_ARGYLL_REFERENCE_KIND_COLOR_CHECKER,
	GCM_CALIBRATE_ARGYLL_REFERENCE_KIND_COLOR_CHECKER_DC,
	GCM_CALIBRATE_ARGYLL_REFERENCE_KIND_COLOR_CHECKER_SG,
	GCM_CALIBRATE_ARGYLL_REFERENCE_KIND_HUTCHCOLOR,
	GCM_CALIBRATE_ARGYLL_REFERENCE_KIND_I1_RGB_SCAN_1_4,
	GCM_CALIBRATE_ARGYLL_REFERENCE_KIND_IT8,
	GCM_CALIBRATE_ARGYLL_REFERENCE_KIND_LASER_SOFT_DC_PRO,
	GCM_CALIBRATE_ARGYLL_REFERENCE_KIND_QPCARD_201,
	GCM_CALIBRATE_ARGYLL_REFERENCE_KIND_UNKNOWN
} GcmCalibrateArgyllReferenceKind;

GType		 	 gcm_calibrate_argyll_get_type			(void);
GcmCalibrateArgyll	*gcm_calibrate_argyll_new			(void);
void			 gcm_calibrate_argyll_set_reference_kind	(GcmCalibrateArgyll	*calibrate_argyll,
									 GcmCalibrateArgyllReferenceKind reference_kind);

G_END_DECLS

#endif /* __GCM_CALIBRATE_ARGYLL_H */


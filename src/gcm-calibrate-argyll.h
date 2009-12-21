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
 * GNU General Public License for more calibrate_argyll.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __GCM_CALIBRATE_ARGYLL_H
#define __GCM_CALIBRATE_ARGYLL_H

#include <glib-object.h>

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
	 GObject				 parent;
	 GcmCalibrateArgyllPrivate		*priv;
};

struct _GcmCalibrateArgyllClass
{
	GObjectClass	parent_class;
	/* padding for future expansion */
	void (*_gcm_reserved1) (void);
	void (*_gcm_reserved2) (void);
	void (*_gcm_reserved3) (void);
	void (*_gcm_reserved4) (void);
	void (*_gcm_reserved5) (void);
};

GType		 	 gcm_calibrate_argyll_get_type		(void);
GcmCalibrateArgyll	*gcm_calibrate_argyll_new		(void);
gboolean	 	 gcm_calibrate_argyll_display		(GcmCalibrateArgyll	*calibrate_argyll,
								 GtkWindow		*window,
								 GError			**error);
gboolean		 gcm_calibrate_argyll_device		(GcmCalibrateArgyll	*calibrate_argyll,
								 GtkWindow		*window,
								 GError			**error);

G_END_DECLS

#endif /* __GCM_CALIBRATE_ARGYLL_H */


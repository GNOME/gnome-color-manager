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

#ifndef __GCM_SAMPLE_WINDOW_H
#define __GCM_SAMPLE_WINDOW_H

#include <glib-object.h>
#include <gtk/gtk.h>
#include <gcm-color.h>

G_BEGIN_DECLS

#define GCM_TYPE_SAMPLE_WINDOW		(gcm_sample_window_get_type ())
#define GCM_SAMPLE_WINDOW(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GCM_TYPE_SAMPLE_WINDOW, GcmSampleWindow))
#define GCM_IS_SAMPLE_WINDOW(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GCM_TYPE_SAMPLE_WINDOW))

typedef struct _GcmSampleWindowPrivate	GcmSampleWindowPrivate;
typedef struct _GcmSampleWindow		GcmSampleWindow;
typedef struct _GcmSampleWindowClass	GcmSampleWindowClass;

struct _GcmSampleWindow
{
	 GtkWindow			 parent;
	 GcmSampleWindowPrivate		*priv;
};

struct _GcmSampleWindowClass
{
	GtkWindowClass			 parent_class;
};

#define	GCM_SAMPLE_WINDOW_PERCENTAGE_PULSE		101

GType		 gcm_sample_window_get_type		(void);
GtkWindow	*gcm_sample_window_new			(void);
void		 gcm_sample_window_set_color		(GcmSampleWindow	*sample_window,
							 const GcmColorRGB	*color);
void		 gcm_sample_window_set_percentage	(GcmSampleWindow	*sample_window,
							 guint			 percentage);

G_END_DECLS

#endif /* __GCM_SAMPLE_WINDOW_H */


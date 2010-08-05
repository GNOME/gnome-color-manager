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

#if !defined (__LIBCOLOR_GLIB_H_INSIDE__) && !defined (LIBCOLOR_GLIB_COMPILATION)
#error "Only <libcolor-glib.h> can be included directly."
#endif

#ifndef __GCM_COMPAT_H__
#define __GCM_COMPAT_H__

#include <gtk/gtk.h>
#include "config.h"

/* only libusb 1.0.8 has libusb_strerror */
#ifndef HAVE_NEW_USB
#define	libusb_strerror(f1)				"unknown"
#endif

/* GtkApplication is missing */
#if (!GTK_CHECK_VERSION(2,21,6))
#define GtkApplication					GObject
#define gtk_application_quit(f1)			g_assert(f1!=NULL)
#define gtk_application_run(f1)				g_assert(f1!=NULL)
#define gtk_application_add_window(f1,f2)		g_assert(f1!=NULL)
#define gtk_application_new(f1,f2,f3)			g_object_new(G_TYPE_OBJECT, NULL)
#endif

#endif /* __GCM_COMPAT_H__ */


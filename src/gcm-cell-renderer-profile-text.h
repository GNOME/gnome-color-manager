/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#pragma once

#include <glib-object.h>
#include <gtk/gtk.h>
#include <colord.h>

#define GCM_TYPE_CELL_RENDERER_PROFILE_TEXT		(gcm_cell_renderer_profile_text_get_type())
#define GCM_CELL_RENDERER_PROFILE_TEXT(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj), GCM_TYPE_CELL_RENDERER_PROFILE_TEXT, GcmCellRendererProfileText))
#define GCM_CELL_RENDERER_PROFILE_TEXT_CLASS(cls)	(G_TYPE_CHECK_CLASS_CAST((cls), GCM_TYPE_CELL_RENDERER_PROFILE_TEXT, GcmCellRendererProfileTextClass))
#define GCM_IS_CELL_RENDERER_PROFILE_TEXT(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), GCM_TYPE_CELL_RENDERER_PROFILE_TEXT))
#define GCM_IS_CELL_RENDERER_PROFILE_TEXT_CLASS(cls)	(G_TYPE_CHECK_CLASS_TYPE((cls), GCM_TYPE_CELL_RENDERER_PROFILE_TEXT))
#define GCM_CELL_RENDERER_PROFILE_TEXT_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), GCM_TYPE_CELL_RENDERER_PROFILE_TEXT, GcmCellRendererProfileTextClass))

typedef struct _GcmCellRendererProfileText		GcmCellRendererProfileText;
typedef struct _GcmCellRendererProfileTextClass		GcmCellRendererProfileTextClass;

struct _GcmCellRendererProfileText
{
	GtkCellRendererText	 parent;
	CdProfile		*profile;
	gboolean		 is_default;
	gchar			*markup;
};

struct _GcmCellRendererProfileTextClass
{
	GtkCellRendererTextClass parent_class;
};

GType		 gcm_cell_renderer_profile_text_get_type	(void);
GtkCellRenderer	*gcm_cell_renderer_profile_text_new		(void);

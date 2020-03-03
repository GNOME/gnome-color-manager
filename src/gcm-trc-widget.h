/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2006-2010 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#pragma once

#include <gtk/gtk.h>

#define GCM_TYPE_TRC_WIDGET		(gcm_trc_widget_get_type ())
#define GCM_TRC_WIDGET(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GCM_TYPE_TRC_WIDGET, GcmTrcWidget))
#define GCM_TRC_WIDGET_CLASS(obj)	(G_TYPE_CHECK_CLASS_CAST ((obj), GCM_TRC_WIDGET, GcmTrcWidgetClass))
#define GCM_IS_TRC_WIDGET(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GCM_TYPE_TRC_WIDGET))
#define GCM_IS_TRC_WIDGET_CLASS(obj)	(G_TYPE_CHECK_CLASS_TYPE ((obj), EFF_TYPE_TRC_WIDGET))
#define GCM_TRC_WIDGET_GET_CLASS	(G_TYPE_INSTANCE_GET_CLASS ((obj), GCM_TYPE_TRC_WIDGET, GcmTrcWidgetClass))

typedef struct GcmTrcWidget		GcmTrcWidget;
typedef struct GcmTrcWidgetClass	GcmTrcWidgetClass;
typedef struct GcmTrcWidgetPrivate	GcmTrcWidgetPrivate;

struct GcmTrcWidget
{
	GtkDrawingArea		 parent;
	GcmTrcWidgetPrivate	*priv;
};

struct GcmTrcWidgetClass
{
	GtkDrawingAreaClass parent_class;
};

GType		 gcm_trc_widget_get_type		(void);
GtkWidget	*gcm_trc_widget_new			(void);

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009-2010 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#pragma once

#include <gtk/gtk.h>

#define GCM_TYPE_GAMMA_WIDGET		(gcm_gamma_widget_get_type ())
#define GCM_GAMMA_WIDGET(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GCM_TYPE_GAMMA_WIDGET, GcmGammaWidget))
#define GCM_GAMMA_WIDGET_CLASS(obj)	(G_TYPE_CHECK_CLASS_CAST ((obj), GCM_GAMMA_WIDGET, GcmGammaWidgetClass))
#define GCM_IS_GAMMA_WIDGET(obj)	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GCM_TYPE_GAMMA_WIDGET))
#define GCM_IS_GAMMA_WIDGET_CLASS(obj)	(G_TYPE_CHECK_CLASS_TYPE ((obj), EFF_TYPE_GAMMA_WIDGET))
#define GCM_GAMMA_WIDGET_GET_CLASS	(G_TYPE_INSTANCE_GET_CLASS ((obj), GCM_TYPE_GAMMA_WIDGET, GcmGammaWidgetClass))

typedef struct GcmGammaWidget		GcmGammaWidget;
typedef struct GcmGammaWidgetClass	GcmGammaWidgetClass;
typedef struct GcmGammaWidgetPrivate	GcmGammaWidgetPrivate;

struct GcmGammaWidget
{
	GtkDrawingArea		 parent;
	GcmGammaWidgetPrivate	*priv;
};

struct GcmGammaWidgetClass
{
	GtkDrawingAreaClass parent_class;
};

GType		 gcm_gamma_widget_get_type		(void);
GtkWidget	*gcm_gamma_widget_new			(void);

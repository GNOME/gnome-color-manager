/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2006-2010 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#pragma once

#include <gtk/gtk.h>
#include <colord.h>

#define GCM_TYPE_CIE_WIDGET		(gcm_cie_widget_get_type ())
#define GCM_CIE_WIDGET(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GCM_TYPE_CIE_WIDGET, GcmCieWidget))
#define GCM_CIE_WIDGET_CLASS(obj)	(G_TYPE_CHECK_CLASS_CAST ((obj), GCM_CIE_WIDGET, GcmCieWidgetClass))
#define GCM_IS_CIE_WIDGET(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GCM_TYPE_CIE_WIDGET))
#define GCM_IS_CIE_WIDGET_CLASS(obj)	(G_TYPE_CHECK_CLASS_TYPE ((obj), EFF_TYPE_CIE_WIDGET))
#define GCM_CIE_WIDGET_GET_CLASS	(G_TYPE_INSTANCE_GET_CLASS ((obj), GCM_TYPE_CIE_WIDGET, GcmCieWidgetClass))

typedef struct GcmCieWidget		GcmCieWidget;
typedef struct GcmCieWidgetClass	GcmCieWidgetClass;
typedef struct GcmCieWidgetPrivate	GcmCieWidgetPrivate;

struct GcmCieWidget
{
	GtkDrawingArea		 parent;
	GcmCieWidgetPrivate	*priv;
};

struct GcmCieWidgetClass
{
	GtkDrawingAreaClass parent_class;
};

GType		 gcm_cie_widget_get_type		(void);
GtkWidget	*gcm_cie_widget_new			(void);
void		 gcm_cie_widget_set_from_profile	(GtkWidget	*widget,
							 CdIcc		*profile);

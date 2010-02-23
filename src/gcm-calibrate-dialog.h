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

#ifndef __GCM_CALIBRATE_DIALOG_H
#define __GCM_CALIBRATE_DIALOG_H

#include <glib-object.h>
#include <gtk/gtk.h>

#include "gcm-device.h"

G_BEGIN_DECLS

#define GCM_TYPE_CALIBRATE_DIALOG		(gcm_calibrate_dialog_get_type ())
#define GCM_CALIBRATE_DIALOG(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), GCM_TYPE_CALIBRATE_DIALOG, GcmCalibrateDialog))
#define GCM_CALIBRATE_DIALOG_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), GCM_TYPE_CALIBRATE_DIALOG, GcmCalibrateDialogClass))
#define GCM_IS_CALIBRATE_DIALOG(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GCM_TYPE_CALIBRATE_DIALOG))
#define GCM_IS_CALIBRATE_DIALOG_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GCM_TYPE_CALIBRATE_DIALOG))
#define GCM_CALIBRATE_DIALOG_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GCM_TYPE_CALIBRATE_DIALOG, GcmCalibrateDialogClass))

typedef struct _GcmCalibrateDialogPrivate	GcmCalibrateDialogPrivate;
typedef struct _GcmCalibrateDialog		GcmCalibrateDialog;
typedef struct _GcmCalibrateDialogClass		GcmCalibrateDialogClass;

struct _GcmCalibrateDialog
{
	 GObject				 parent;
	 GcmCalibrateDialogPrivate		*priv;
};

struct _GcmCalibrateDialogClass
{
	GObjectClass	parent_class;
	void		(* response)				(GcmCalibrateDialog	*calibrate_dialog,
								 GtkResponseType		 response);
};

typedef enum {
	GCM_CALIBRATE_DIALOG_TAB_DISPLAY_TYPE,
	GCM_CALIBRATE_DIALOG_TAB_TARGET_TYPE,
	GCM_CALIBRATE_DIALOG_TAB_MANUAL,
	GCM_CALIBRATE_DIALOG_TAB_GENERIC,
	GCM_CALIBRATE_DIALOG_TAB_PRINT_MODE,
	GCM_CALIBRATE_DIALOG_TAB_LAST
} GcmCalibrateDialogTab;

GType			 gcm_calibrate_dialog_get_type		(void);
GcmCalibrateDialog	*gcm_calibrate_dialog_new		(void);

void			 gcm_calibrate_dialog_set_window	(GcmCalibrateDialog	*calibrate_dialog,
								 GtkWindow		*window);
void			 gcm_calibrate_dialog_show		(GcmCalibrateDialog	*calibrate_dialog,
								 GcmCalibrateDialogTab	 tab,
								 const gchar		*title,
								 const gchar		*message);
void			 gcm_calibrate_dialog_set_move_window	(GcmCalibrateDialog	*calibrate_dialog,
								 gboolean		 move_window);
void			 gcm_calibrate_dialog_set_show_expander	(GcmCalibrateDialog	*calibrate_dialog,
								 gboolean		 visible);
void			 gcm_calibrate_dialog_set_show_button_ok (GcmCalibrateDialog	*calibrate_dialog,
								 gboolean		 visible);
void			 gcm_calibrate_dialog_set_image_filename (GcmCalibrateDialog	*calibrate_dialog,
								 const gchar		*image_filename);
void			 gcm_calibrate_dialog_pop		(GcmCalibrateDialog	*calibrate_dialog);
void			 gcm_calibrate_dialog_hide		(GcmCalibrateDialog	*calibrate_dialog);
GtkResponseType		 gcm_calibrate_dialog_run		(GcmCalibrateDialog	*calibrate_dialog);
GtkWindow		*gcm_calibrate_dialog_get_window	(GcmCalibrateDialog	*calibrate_dialog);
void			 gcm_calibrate_dialog_pack_details	(GcmCalibrateDialog	*calibrate_dialog,
								 GtkWidget		*widget);

G_END_DECLS

#endif /* __GCM_CALIBRATE_DIALOG_H */


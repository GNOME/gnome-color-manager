/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009-2015 Richard Hughes <richard@hughsie.com>
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

#ifndef __GCM_CALIBRATE_H
#define __GCM_CALIBRATE_H

#include <glib-object.h>
#include <gtk/gtk.h>
#include <colord.h>

G_BEGIN_DECLS

#define GCM_TYPE_CALIBRATE (gcm_calibrate_get_type ())
G_DECLARE_DERIVABLE_TYPE (GcmCalibrate, gcm_calibrate, GCM, CALIBRATE, GObject)

struct _GcmCalibrateClass
{
	GObjectClass	parent_class;
	/* vtable */
	gboolean	 (*calibrate_display)		(GcmCalibrate	*calibrate,
							 CdDevice	*device,
							 CdSensor	*sensor,
							 GtkWindow	*window,
							 GError		**error);
	gboolean	 (*calibrate_device)		(GcmCalibrate	*calibrate,
							 CdDevice	*device,
							 CdSensor	*sensor,
							 GtkWindow	*window,
							 GError		**error);
	gboolean	 (*calibrate_printer)		(GcmCalibrate	*calibrate,
							 CdDevice	*device,
							 CdSensor	*sensor,
							 GtkWindow	*window,
							 GError		**error);
	gboolean	 (*get_enabled)	(GcmCalibrate	*calibrate);
	void		 (*interaction)			(GcmCalibrate	*calibrate,
							 GtkResponseType response);

	/* signal */
	void		(* title_changed)		(GcmCalibrate	*calibrate,
							 const gchar	*title);
	void		(* message_changed)		(GcmCalibrate	*calibrate,
							 const gchar	*message);
	void		(* image_changed)		(GcmCalibrate	*calibrate,
							 const gchar	*filename);
	void		(* progress_changed)		(GcmCalibrate	*calibrate,
							 guint		 percentage);
	void		(* interaction_required)	(GcmCalibrate	*calibrate,
							 const gchar	*button_text);
};

typedef enum {
	GCM_CALIBRATE_ERROR_USER_ABORT,
	GCM_CALIBRATE_ERROR_NO_SUPPORT,
	GCM_CALIBRATE_ERROR_NO_DATA,
	GCM_CALIBRATE_ERROR_INTERNAL
} GcmCalibrateError;

/* dummy */
#define GCM_CALIBRATE_ERROR	1

typedef enum {
	GCM_CALIBRATE_REFERENCE_KIND_CMP_DIGITAL_TARGET_3,
	GCM_CALIBRATE_REFERENCE_KIND_CMP_DT_003,
	GCM_CALIBRATE_REFERENCE_KIND_COLOR_CHECKER,
	GCM_CALIBRATE_REFERENCE_KIND_COLOR_CHECKER_DC,
	GCM_CALIBRATE_REFERENCE_KIND_COLOR_CHECKER_SG,
	GCM_CALIBRATE_REFERENCE_KIND_HUTCHCOLOR,
	GCM_CALIBRATE_REFERENCE_KIND_I1_RGB_SCAN_1_4,
	GCM_CALIBRATE_REFERENCE_KIND_IT8,
	GCM_CALIBRATE_REFERENCE_KIND_LASER_SOFT_DC_PRO,
	GCM_CALIBRATE_REFERENCE_KIND_QPCARD_201,
	GCM_CALIBRATE_REFERENCE_KIND_UNKNOWN
} GcmCalibrateReferenceKind;

typedef enum {
	GCM_CALIBRATE_DEVICE_KIND_LCD_CCFL,
	GCM_CALIBRATE_DEVICE_KIND_LCD_LED_WHITE,
	GCM_CALIBRATE_DEVICE_KIND_LCD_LED_RGB,
	GCM_CALIBRATE_DEVICE_KIND_LCD_LED_RGB_WIDE,
	GCM_CALIBRATE_DEVICE_KIND_LCD_CCFL_WIDE,
	GCM_CALIBRATE_DEVICE_KIND_CRT,
	GCM_CALIBRATE_DEVICE_KIND_PROJECTOR,
	GCM_CALIBRATE_DEVICE_KIND_UNKNOWN
} GcmCalibrateDisplayKind;

typedef enum {
	GCM_CALIBRATE_PRINT_KIND_LOCAL,
	GCM_CALIBRATE_PRINT_KIND_GENERATE,
	GCM_CALIBRATE_PRINT_KIND_ANALYZE,
	GCM_CALIBRATE_PRINT_KIND_UNKNOWN
} GcmCalibratePrintKind;

typedef enum {
	GCM_CALIBRATE_PRECISION_SHORT,
	GCM_CALIBRATE_PRECISION_NORMAL,
	GCM_CALIBRATE_PRECISION_LONG,
	GCM_CALIBRATE_PRECISION_UNKNOWN
} GcmCalibratePrecision;

typedef enum {
	GCM_CALIBRATE_UI_INTERACTION,
	GCM_CALIBRATE_UI_STATUS,
	GCM_CALIBRATE_UI_ERROR,
	GCM_CALIBRATE_UI_POST_INTERACTION,
	GCM_CALIBRATE_UI_UNKNOWN
} GcmCalibrateUiType;

GcmCalibrate	*gcm_calibrate_new			(void);

/* designed for gcm-calibrate to call */
gboolean	 gcm_calibrate_device			(GcmCalibrate	*calibrate,
							 CdDevice	*device,
							 GtkWindow	*window,
							 GError		**error);
void		 gcm_calibrate_set_content_widget	(GcmCalibrate	*calibrate,
							 GtkWidget	*widget);
void		 gcm_calibrate_interaction		(GcmCalibrate	*calibrate,
							 GtkResponseType response);

/* designed for super-classes to call */
void		 gcm_calibrate_set_sensor		(GcmCalibrate	*calibrate,
							 CdSensor	*sensor);
CdSensor	*gcm_calibrate_get_sensor		(GcmCalibrate	*calibrate);
void		 gcm_calibrate_set_title		(GcmCalibrate	*calibrate,
							 const gchar	*title,
							 GcmCalibrateUiType ui_type);
void		 gcm_calibrate_set_message		(GcmCalibrate	*calibrate,
							 const gchar	*message,
							 GcmCalibrateUiType ui_type);
void		 gcm_calibrate_set_image		(GcmCalibrate	*calibrate,
							 const gchar	*filename);
void		 gcm_calibrate_set_progress		(GcmCalibrate	*calibrate,
							 guint		 percentage);
void		 gcm_calibrate_interaction_required	(GcmCalibrate	*calibrate,
							 const gchar	*button_text);
GtkWidget	*gcm_calibrate_get_content_widget	(GcmCalibrate	*calibrate);
void		 gcm_calibrate_interaction_attach	(GcmCalibrate	*calibrate);
void		 gcm_calibrate_interaction_calibrate	(GcmCalibrate	*calibrate);
void		 gcm_calibrate_interaction_screen	(GcmCalibrate	*calibrate);

/* JUNK */
gboolean	 gcm_calibrate_set_from_exif		(GcmCalibrate	*calibrate,
							 const gchar	*filename,
							 GError		**error);
const gchar	*gcm_calibrate_get_filename_result	(GcmCalibrate	*calibrate);
const gchar	*gcm_calibrate_get_working_path		(GcmCalibrate	*calibrate);
const gchar	*gcm_calibrate_get_basename		(GcmCalibrate	*calibrate);
guint		 gcm_calibrate_get_screen_brightness	(GcmCalibrate	*calibrate);
gboolean	 gcm_calibrate_get_enabled		(GcmCalibrate	*calibrate);

G_END_DECLS

#endif /* __GCM_CALIBRATE_H */


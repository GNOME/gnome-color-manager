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

#ifndef __GCM_COLOR_H__
#define __GCM_COLOR_H__

#include <glib-object.h>
#include <gcm-math.h>

typedef struct {
	guint8	 R;
	guint8	 G;
	guint8	 B;
} GcmColorRGBint;

typedef struct {
	gdouble	 L;
	gdouble	 a;
	gdouble	 b;
} GcmColorLab;

typedef struct {
	gdouble	 Y;
	gdouble	 x;
	gdouble	 y;
} GcmColorYxy;

typedef struct {
	gdouble	 X;
	gdouble	 Y;
	gdouble	 Z;
} GcmColorXYZ;

typedef struct {
	gdouble	 R;
	gdouble	 G;
	gdouble	 B;
} GcmColorRGB;

#define	GCM_TYPE_COLOR_RGB	(gcm_color_get_type_RGB ())
#define	GCM_TYPE_COLOR_XYZ	(gcm_color_get_type_XYZ ())
#define	GCM_TYPE_COLOR_YXY	(gcm_color_get_type_Yxy ())

/* types */
GType		 gcm_color_get_type_XYZ			(void);
GType		 gcm_color_get_type_RGB			(void);
GType		 gcm_color_get_type_Yxy			(void);

/* allocate and deallocate helpers */
#define		 gcm_color_new_XYZ()			g_new0 (GcmColorXYZ, 1)
#define		 gcm_color_new_RGB()			g_new0 (GcmColorRGB, 1)
#define		 gcm_color_new_Yxy()			g_new0 (GcmColorYxy, 1)
#define		 gcm_color_free_XYZ			g_free
#define		 gcm_color_free_RGB			g_free
#define		 gcm_color_free_Yxy			g_free
GcmColorXYZ	*gcm_color_dup_XYZ			(const GcmColorXYZ	*src);
GcmColorRGB	*gcm_color_dup_RGB			(const GcmColorRGB	*src);
GcmColorYxy	*gcm_color_dup_Yxy			(const GcmColorYxy	*src);

void		 gcm_color_set_XYZ			(GcmColorXYZ		*dest,
							 gdouble		 X,
							 gdouble		 Y,
							 gdouble		 Z);
void		 gcm_color_set_RGB			(GcmColorRGB		*dest,
							 gdouble		 R,
							 gdouble		 G,
							 gdouble		 B);
void		 gcm_color_set_Yxy			(GcmColorYxy		*dest,
							 gdouble		 Y,
							 gdouble		 x,
							 gdouble		 y);
void		 gcm_color_copy_XYZ			(const GcmColorXYZ	*src,
							 GcmColorXYZ		*dest);
void		 gcm_color_copy_Yxy			(const GcmColorYxy	*src,
							 GcmColorYxy		*dest);
void		 gcm_color_clear_XYZ			(GcmColorXYZ		*dest);
void		 gcm_color_copy_RGB			(const GcmColorRGB	*src,
							 GcmColorRGB		*dest);
void		 gcm_color_convert_RGBint_to_RGB	(const GcmColorRGBint	*src,
							 GcmColorRGB		*dest);
void		 gcm_color_convert_RGB_to_RGBint	(const GcmColorRGB	*src,
							 GcmColorRGBint		*dest);
void		 gcm_color_convert_Yxy_to_XYZ		(const GcmColorYxy	*src,
							 GcmColorXYZ		*dest);
void		 gcm_color_convert_XYZ_to_Yxy		(const GcmColorXYZ	*src,
							 GcmColorYxy		*dest);
GcmVec3		*gcm_color_get_XYZ_Vec3			(GcmColorXYZ		*src);
GcmVec3		*gcm_color_get_RGB_Vec3			(GcmColorRGB		*src);

#endif /* __GCM_COLOR_H__ */


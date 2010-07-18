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

#ifndef __GCM_COMMON_H__
#define __GCM_COMMON_H__

#define __GCM_COMMON_H_INSIDE__

typedef struct {
	guint8	 red;
	guint8	 green;
	guint8	 blue;
} GcmColorRgbInt;

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
	gdouble	 red;
	gdouble	 green;
	gdouble	 blue;
} GcmColorRgb;

typedef struct {
	gdouble	 m00, m01, m02;
	gdouble	 m10, m11, m12;
	gdouble	 m20, m21, m22;
	/* any addition fields go *after* the data */
} GcmMat3x3;

typedef struct {
	double	 v0, v1, v2;
	/* any addition fields go *after* the data */
} GcmVec3;

void		 gcm_convert_rgb_int_to_rgb	(GcmColorRgbInt		*rgb_int,
						 GcmColorRgb		*rgb);
void		 gcm_convert_rgb_to_rgb_int	(GcmColorRgb		*rgb,
						 GcmColorRgbInt		*rgb_int);
void		 gcm_convert_Yxy_to_XYZ		(GcmColorYxy		*src,
						 GcmColorXYZ		*dest);
void		 gcm_convert_XYZ_to_Yxy		(GcmColorXYZ		*src,
						 GcmColorYxy		*dest);
void		 gcm_vec3_clear			(GcmVec3		*src);
void		 gcm_vec3_scalar_multiply	(GcmVec3		*src,
						 gdouble		 value,
						 GcmVec3		*dest);
gchar		*gcm_vec3_to_string		(GcmVec3		*src);
gdouble		*gcm_vec3_get_data		(GcmVec3		*src);
void		 gcm_mat33_clear		(GcmMat3x3		*dest);
gchar		*gcm_mat33_to_string		(GcmMat3x3		*src);
gdouble		*gcm_mat33_get_data		(GcmMat3x3		*src);
void		 gcm_mat33_set_identity		(GcmMat3x3		*dest);
void		 gcm_mat33_vector_multiply	(GcmMat3x3		*mat_src,
						 GcmVec3		*vec_src,
						 GcmVec3		*vec_dest);
void		 gcm_mat33_matrix_multiply	(GcmMat3x3		*mat_src1,
						 GcmMat3x3		*mat_src2,
						 GcmMat3x3		*mat_dest);
gboolean	 gcm_mat33_reciprocal		(GcmMat3x3		*src,
						 GcmMat3x3		*dest);


#undef __GCM_COMMON_H_INSIDE__

#endif /* __GCM_COMMON_H__ */


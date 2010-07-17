/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007-2010 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU Lesser General Public License Version 2.1
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include "config.h"

#include <glib-object.h>

#include "gcm-common.h"

static void
gcm_test_common_func (void)
{
	GcmColorRgbInt rgb_int;
	GcmColorRgb rgb;
	GcmColorYxy Yxy;
	GcmColorXYZ XYZ;
	GcmMat3x3 mat;
	GcmMat3x3 matsrc;

	/* black */
	rgb_int.red = 0x00; rgb_int.green = 0x00; rgb_int.blue = 0x00;
	gcm_convert_rgb_int_to_rgb (&rgb_int, &rgb);
	g_assert_cmpfloat (rgb.red, <, 0.01);
	g_assert_cmpfloat (rgb.green, <, 0.01);
	g_assert_cmpfloat (rgb.blue, <, 0.01);
	g_assert_cmpfloat (rgb.red, >, -0.01);
	g_assert_cmpfloat (rgb.green, >, -0.01);
	g_assert_cmpfloat (rgb.blue, >, -0.01);

	/* white */
	rgb_int.red = 0xff; rgb_int.green = 0xff; rgb_int.blue = 0xff;
	gcm_convert_rgb_int_to_rgb (&rgb_int, &rgb);
	g_assert_cmpfloat (rgb.red, <, 1.01);
	g_assert_cmpfloat (rgb.green, <, 1.01);
	g_assert_cmpfloat (rgb.blue, <, 1.01);
	g_assert_cmpfloat (rgb.red, >, 0.99);
	g_assert_cmpfloat (rgb.green, >, 0.99);
	g_assert_cmpfloat (rgb.blue, >, 0.99);

	/* and back */
	gcm_convert_rgb_to_rgb_int (&rgb, &rgb_int);
	g_assert_cmpint (rgb_int.red, ==, 0xff);
	g_assert_cmpint (rgb_int.green, ==, 0xff);
	g_assert_cmpint (rgb_int.blue, ==, 0xff);

	/* black */
	rgb.red = 0.0f; rgb.green = 0.0f; rgb.blue = 0.0f;
	gcm_convert_rgb_to_rgb_int (&rgb, &rgb_int);
	g_assert_cmpint (rgb_int.red, ==, 0x00);
	g_assert_cmpint (rgb_int.green, ==, 0x00);
	g_assert_cmpint (rgb_int.blue, ==, 0x00);

	/* Yxy -> XYZ */
	Yxy.Y = 21.5;
	Yxy.x = 0.31;
	Yxy.y = 0.32;
	gcm_convert_Yxy_to_XYZ (&Yxy, &XYZ);
	g_assert_cmpfloat (XYZ.X, <, 21.0);
	g_assert_cmpfloat (XYZ.X, >, 20.5);
	g_assert_cmpfloat (XYZ.Y, <, 22.0);
	g_assert_cmpfloat (XYZ.Y, >, 21.0);
	g_assert_cmpfloat (XYZ.Z, <, 25.0);
	g_assert_cmpfloat (XYZ.Z, >, 24.5);

	/* and back */
	gcm_convert_XYZ_to_Yxy (&XYZ, &Yxy);
	g_assert_cmpfloat (Yxy.Y, <, 22.0);
	g_assert_cmpfloat (Yxy.Y, >, 21.0);
	g_assert_cmpfloat (Yxy.x, <, 0.35);
	g_assert_cmpfloat (Yxy.x, >, 0.25);
	g_assert_cmpfloat (Yxy.y, <, 0.35);
	g_assert_cmpfloat (Yxy.y, >, 0.25);

	/* matrix */
	mat.m00 = 1.00f;
	gcm_mat33_clear (&mat);
	g_assert_cmpfloat (mat.m00, <, 0.001f);
	g_assert_cmpfloat (mat.m00, >, -0.001f);
	g_assert_cmpfloat (mat.m22, <, 0.001f);
	g_assert_cmpfloat (mat.m22, >, -0.001f);

	/* multiply two matrices */
	gcm_mat33_clear (&matsrc);
	matsrc.m01 = matsrc.m10 = 2.0f;
	gcm_mat33_matrix_multiply (&matsrc, &matsrc, &mat);
	g_assert_cmpfloat (mat.m00, <, 4.1f);
	g_assert_cmpfloat (mat.m00, >, 3.9f);
	g_assert_cmpfloat (mat.m11, <, 4.1f);
	g_assert_cmpfloat (mat.m11, >, 3.9f);
	g_assert_cmpfloat (mat.m22, <, 0.001f);
	g_assert_cmpfloat (mat.m22, >, -0.001f);
}

int
main (int argc, char **argv)
{
	g_type_init ();

	g_test_init (&argc, &argv, NULL);

	/* tests go here */
	g_test_add_func ("/libcolor-glib/common", gcm_test_common_func);

	return g_test_run ();
}


/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 Richard Hughes <richard@hughsie.com>
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

#include <glib-object.h>
#include "egg-test.h"
#include <egg-debug.h>

/* prototypes */
void gcm_print_test (EggTest *test);
void gcm_edid_test (EggTest *test);
void gcm_image_test (EggTest *test);
void gcm_tables_test (EggTest *test);
void gcm_utils_test (EggTest *test);
void gcm_device_test (EggTest *test);
void gcm_profile_test (EggTest *test);
void gcm_brightness_test (EggTest *test);
void gcm_clut_test (EggTest *test);
void gcm_dmi_test (EggTest *test);
void gcm_xyz_test (EggTest *test);
void gcm_cie_widget_test (EggTest *test);
void gcm_trc_widget_test (EggTest *test);
void gcm_calibrate_test (EggTest *test);
void gcm_calibrate_manual_test (EggTest *test);

int
main (int argc, char **argv)
{
	EggTest *test;

	if (! g_thread_supported ())
		g_thread_init (NULL);
	gtk_init (&argc, &argv);
	test = egg_test_init ();
	egg_debug_init (&argc, &argv);

	/* components */
	gcm_calibrate_test (test);
	gcm_edid_test (test);
	gcm_tables_test (test);
	gcm_utils_test (test);
	gcm_device_test (test);
	gcm_profile_test (test);
	gcm_brightness_test (test);
	gcm_clut_test (test);
	gcm_dmi_test (test);
	gcm_xyz_test (test);
	gcm_trc_widget_test (test);
	gcm_cie_widget_test (test);
	gcm_gamma_widget_test (test);
	gcm_image_test (test);
	gcm_print_test (test);
//	gcm_calibrate_manual_test (test);

	return (egg_test_finish (test));
}


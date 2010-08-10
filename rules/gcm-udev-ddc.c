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

#include "config.h"

#include <glib.h>
#include <libcolor-glib.h>

/**
 * main:
 **/
int
main (int argc, char **argv)
{
	guint retval = 1;
	gboolean ret;
	GError *error = NULL;
	GcmDdcDevice *device;
	const gchar *edid_md5;

	g_type_init ();

	/* find devices */
	device = gcm_ddc_device_new ();

	/* check we got a device */
	if (argv[1] == NULL) {
		g_printerr ("FAILED get device\n");
		goto out;
	}

	/* get the default device */
	ret = gcm_ddc_device_open (device, argv[1], &error);
	if (!ret) {
		g_printerr ("FAILED to open device: %s\n", error->message);
		g_error_free (error);
		goto out;
	}

	/* check we can use it */
	edid_md5 = gcm_ddc_device_get_edid_md5 (device, &error);
	if (edid_md5 == NULL) {
		g_print ("FAILED to get edid: %s\n", error->message);
		g_error_free (error);
		goto out;
	}

	/* success */
	retval = 0;
out:
	g_object_unref (device);
	return retval;
}


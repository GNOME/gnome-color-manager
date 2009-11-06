/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007-2008 Richard Hughes <richard@hughsie.com>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <glib/gi18n.h>
#include <dbus/dbus-glib.h>
#include <gtk/gtk.h>

#include "egg-debug.h"
#include "gcm-dbus.h"
#include "org.gnome.ColorManager.h"

#define GCM_DBUS_SERVICE	"org.gnome.ColorManager"
#define GCM_DBUS_INTERFACE	"org.gnome.ColorManager"
#define GCM_DBUS_PATH		"/org/gnome/ColorManager"

/**
 * gcm_session_object_register:
 * @connection: What we want to register to
 * @object: The GObject we want to register
 *
 * Return value: success
 **/
static gboolean
gcm_session_object_register (DBusGConnection *connection, GObject *object)
{
	DBusGProxy *bus_proxy = NULL;
	GError *error = NULL;
	guint request_name_result;
	gboolean ret;

	/* connect to the bus */
	bus_proxy = dbus_g_proxy_new_for_name (connection, DBUS_SERVICE_DBUS,
					       DBUS_PATH_DBUS, DBUS_INTERFACE_DBUS);

	/* get our name */
	ret = dbus_g_proxy_call (bus_proxy, "RequestName", &error,
				 G_TYPE_STRING, GCM_DBUS_SERVICE,
				 G_TYPE_UINT, DBUS_NAME_FLAG_ALLOW_REPLACEMENT |
					      DBUS_NAME_FLAG_REPLACE_EXISTING |
					      DBUS_NAME_FLAG_DO_NOT_QUEUE,
				 G_TYPE_INVALID,
				 G_TYPE_UINT, &request_name_result,
				 G_TYPE_INVALID);
	if (ret == FALSE) {
		/* abort as the DBUS method failed */
		egg_warning ("RequestName failed: %s", error->message);
		g_error_free (error);
		return FALSE;
	}

	/* free the bus_proxy */
	g_object_unref (G_OBJECT (bus_proxy));

	/* already running */
	if (request_name_result != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER)
		return FALSE;

	dbus_g_object_type_install_info (GCM_TYPE_DBUS, &dbus_glib_gcm_dbus_object_info);
	dbus_g_error_domain_register (GCM_DBUS_ERROR, NULL, GCM_DBUS_TYPE_ERROR);
	dbus_g_connection_register_g_object (connection, GCM_DBUS_PATH, object);

	return TRUE;
}

/**
 * main:
 **/
int
main (int argc, char *argv[])
{
	gboolean verbose = FALSE;
	gboolean timed_exit = FALSE;
	GcmDbus *dbus = NULL;
	GOptionContext *context;
	GError *error = NULL;
	gboolean ret;
	guint retval = 0;
	DBusGConnection *connection;

	const GOptionEntry options[] = {
		{ "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose,
		  _("Show extra debugging information"), NULL },
		{ "timed-exit", '\0', 0, G_OPTION_ARG_NONE, &timed_exit,
		  _("Exit after a small delay"), NULL },
		{ NULL}
	};

	if (! g_thread_supported ())
		g_thread_init (NULL);
	dbus_g_thread_init ();
	g_type_init ();

	/* TRANSLATORS: program name, a session wide daemon to watch for updates and changing system state */
	g_set_application_name (_("Color Management"));
	context = g_option_context_new (NULL);
	g_option_context_set_summary (context, _("Color Management DBus Service"));
	g_option_context_add_main_entries (context, options, NULL);
	g_option_context_parse (context, &argc, &argv, NULL);
	g_option_context_free (context);

	egg_debug_init (verbose);
	gtk_init (&argc, &argv);

	/* create new objects */
	dbus = gcm_dbus_new ();

	/* get the bus */
	connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	if (error) {
		egg_warning ("%s", error->message);
		g_error_free (error);
		retval = 1;
		goto out;
	}

	/* try to register */
	ret = gcm_session_object_register (connection, G_OBJECT (dbus));
	if (!ret) {
		egg_warning ("failed to replace running instance.");
		retval = 1;
		goto out;
	}

	/* Only timeout if we have specified iton the command line */
	if (timed_exit)
		g_timeout_add_seconds (120, (GSourceFunc) gtk_main_quit, NULL);

	/* wait */
	gtk_main ();
out:
	g_object_unref (dbus);
	return retval;
}


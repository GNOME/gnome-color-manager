/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009 Richard Hughes <richard@hughsie.com>
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
 * GNU General Public License for more utils.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <libgnomeui/gnome-rr.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/xf86vmode.h>
#include <gconf/gconf-client.h>
#include <dbus/dbus-glib.h>

#include "gcm-utils.h"
#include "gcm-screen.h"
#include "gcm-clut.h"
#include "gcm-xserver.h"

#include "egg-debug.h"

/**
 * gcm_utils_linkify:
 **/
gchar *
gcm_utils_linkify (const gchar *text)
{
	guint i;
	guint j = 0;
	gboolean ret;
	GString *string;

	/* find and replace links */
	string = g_string_new ("");
	for (i=0;; i++) {

		/* find the start of a link */
		ret = g_str_has_prefix (&text[i], "http://");
		if (ret) {
			g_string_append_len (string, text+j, i-j);
			for (j=i;; j++) {
				/* find the end of the link, or the end of the string */
				if (text[j] == '\0' ||
				    text[j] == ' ') {
					g_string_append (string, "<a href=\"");
					g_string_append_len (string, text+i, j-i);
					g_string_append (string, "\">");
					g_string_append_len (string, text+i, j-i);
					g_string_append (string, "</a>");
					break;
				}
			}
		}

		/* end of the string, dump what's left */
		if (text[i] == '\0') {
			g_string_append_len (string, text+j, i-j);
			break;
		}
	}
	return g_string_free (string, FALSE);
}

/**
 * gcm_utils_is_icc_profile:
 **/
gboolean
gcm_utils_is_icc_profile (const gchar *filename)
{
	GFile *file;
	GFileInfo *info;
	const gchar *type;
	GError *error = NULL;
	gboolean ret = FALSE;

	/* get content type for file */
	file = g_file_new_for_path (filename);
	info = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE, G_FILE_QUERY_INFO_NONE, NULL, &error);
	if (info != NULL) {
		type = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE);
		if (g_strcmp0 (type, "application/vnd.iccprofile") == 0) {
			ret = TRUE;
			goto out;
		}
	} else {
		egg_warning ("failed to get content type of %s: %s", filename, error->message);
		g_error_free (error);
	}

	/* fall back if we have not got a new enought s-m-i */
	if (g_str_has_suffix (filename, ".icc")) {
		ret = TRUE;
		goto out;
	}
	if (g_str_has_suffix (filename, ".icm")) {
		ret = TRUE;
		goto out;
	}
	if (g_str_has_suffix (filename, ".ICC")) {
		ret = TRUE;
		goto out;
	}
	if (g_str_has_suffix (filename, ".ICM")) {
		ret = TRUE;
		goto out;
	}
out:
	if (info != NULL)
		g_object_unref (info);
	g_object_unref (file);
	return ret;
}

/**
 * gcm_utils_install_package:
 **/
gboolean
gcm_utils_install_package (const gchar *package_name, GtkWindow *window)
{
	DBusGConnection *connection;
	DBusGProxy *proxy;
	GError *error = NULL;
	gboolean ret;
	guint32 xid = 0;
	gchar **packages = NULL;

	g_return_val_if_fail (package_name != NULL, FALSE);

#ifndef GCM_USE_PACKAGEKIT
	egg_warning ("cannot install %s: this package was not compiled with --enable-packagekit", package_name);
	return FALSE;
#endif

	/* get xid of this window */
	if (window != NULL)
		xid = gdk_x11_drawable_get_xid (gtk_widget_get_window (GTK_WIDGET(window)));

	/* we're expecting an array of packages */
	packages = g_strsplit (package_name, "|", 1);

	/* get a session bus connection */
	connection = dbus_g_bus_get (DBUS_BUS_SESSION, NULL);

	/* connect to PackageKit */
	proxy = dbus_g_proxy_new_for_name (connection,
					   "org.freedesktop.PackageKit",
					   "/org/freedesktop/PackageKit",
					   "org.freedesktop.PackageKit.Modify");

	/* execute sync method */
	ret = dbus_g_proxy_call (proxy, "InstallPackageNames", &error,
				 G_TYPE_UINT, xid,
				 G_TYPE_STRV, packages,
				 G_TYPE_STRING, "hide-confirm-search,hide-finished",
				 G_TYPE_INVALID, G_TYPE_INVALID);
	if (!ret) {
		egg_warning ("failed to install package: %s", error->message);
		g_error_free (error);
		goto out;
	}
out:
	g_object_unref (proxy);
	g_strfreev (packages);
	return ret;
}

/**
 * gcm_utils_output_is_lcd_internal:
 * @output_name: the output name
 *
 * Return value: %TRUE is the output is an internal LCD panel
 **/
gboolean
gcm_utils_output_is_lcd_internal (const gchar *output_name)
{
	g_return_val_if_fail (output_name != NULL, FALSE);
	if (g_strstr_len (output_name, -1, "LVDS") != NULL)
		return TRUE;
	if (g_strstr_len (output_name, -1, "lvds") != NULL)
		return TRUE;
	return FALSE;
}

/**
 * gcm_utils_output_is_lcd:
 * @output_name: the output name
 *
 * Return value: %TRUE is the output is an internal or external LCD panel
 **/
gboolean
gcm_utils_output_is_lcd (const gchar *output_name)
{
	g_return_val_if_fail (output_name != NULL, FALSE);
	if (gcm_utils_output_is_lcd_internal (output_name))
		return TRUE;
	if (g_strstr_len (output_name, -1, "DVI") != NULL)
		return TRUE;
	if (g_strstr_len (output_name, -1, "dvi") != NULL)
		return TRUE;
	return FALSE;
}

/**
 * gcm_utils_ensure_printable:
 **/
void
gcm_utils_ensure_printable (gchar *text)
{
	guint i;
	guint idx = 0;

	g_return_if_fail (text != NULL);

	for (i=0; text[i] != '\0'; i++) {
		if (g_ascii_isalnum (text[i]) ||
		    g_ascii_ispunct (text[i]) ||
		    text[i] == ' ')
			text[idx++] = text[i];
	}
	text[idx] = '\0';

	/* broken profiles have _ instead of spaces */
	g_strdelimit (text, "_", ' ');
}

/**
 * gcm_utils_get_gamma_size_fallback:
 **/
static guint
gcm_utils_get_gamma_size_fallback (void)
{
	guint size;
	Bool rc;

	/* this is per-screen, not per output which is less than ideal */
	gdk_error_trap_push ();
	egg_warning ("using PER-SCREEN gamma tables as driver is not XRANDR 1.3 compliant");
	rc = XF86VidModeGetGammaRampSize (GDK_DISPLAY(), gdk_x11_get_default_screen (), (int*) &size);
	gdk_error_trap_pop ();
	if (!rc)
		size = 0;

	return size;
}

/**
 * gcm_utils_get_gamma_size:
 *
 * Return value: the gamma size, or 0 if error;
 **/
static guint
gcm_utils_get_gamma_size (GnomeRRCrtc *crtc, GError **error)
{
	guint id;
	guint size;

	/* get id that X recognises */
	id = gnome_rr_crtc_get_id (crtc);

	/* get the value, and catch errors */
	gdk_error_trap_push ();
	size = XRRGetCrtcGammaSize (GDK_DISPLAY(), id);
	if (gdk_error_trap_pop ())
		size = 0;

	/* some drivers support Xrandr 1.2, not 1.3 */
	if (size == 0)
		size = gcm_utils_get_gamma_size_fallback ();

	/* no size, or X popped an error */
	if (size == 0) {
		g_set_error_literal (error, 1, 0, "failed to get gamma size");
		goto out;
	}
out:
	return size;
}

/**
 * gcm_utils_set_gamma_fallback:
 **/
static gboolean
gcm_utils_set_gamma_fallback (XRRCrtcGamma *gamma, guint size)
{
	Bool rc;

	/* this is per-screen, not per output which is less than ideal */
	gdk_error_trap_push ();
	egg_warning ("using PER-SCREEN gamma tables as driver is not XRANDR 1.3 compliant");
	rc = XF86VidModeSetGammaRamp (GDK_DISPLAY(), gdk_x11_get_default_screen (), size, gamma->red, gamma->green, gamma->blue);
	gdk_error_trap_pop ();

	return rc;
}

/**
 * gcm_utils_set_gamma_for_crtc:
 *
 * Return value: %TRUE for success;
 **/
static gboolean
gcm_utils_set_gamma_for_crtc (GnomeRRCrtc *crtc, GcmClut *clut, GError **error)
{
	guint id;
	gboolean ret = TRUE;
	GPtrArray *array = NULL;
	XRRCrtcGamma *gamma = NULL;
	guint i;
	GcmClutData *data;

	/* get data */
	array = gcm_clut_get_array (clut);
	if (array == NULL) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0, "failed to get CLUT data");
		goto out;
	}

	/* no length? */
	if (array->len == 0) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0, "no data in the CLUT array");
		goto out;
	}

	/* convert to a type X understands */
	gamma = XRRAllocGamma (array->len);
	for (i=0; i<array->len; i++) {
		data = g_ptr_array_index (array, i);
		gamma->red[i] = data->red;
		gamma->green[i] = data->green;
		gamma->blue[i] = data->blue;
	}

	/* get id that X recognises */
	id = gnome_rr_crtc_get_id (crtc);

	/* get the value, and catch errors */
	gdk_error_trap_push ();
	XRRSetCrtcGamma (GDK_DISPLAY(), id, gamma);
	gdk_flush ();
	if (gdk_error_trap_pop ()) {
		/* some drivers support Xrandr 1.2, not 1.3 */
		ret = gcm_utils_set_gamma_fallback (gamma, array->len);
		if (!ret) {
			g_set_error (error, 1, 0, "failed to set crtc gamma %p (%i) on %i", gamma, array->len, id);
			goto out;
		}
	}
out:
	if (gamma != NULL)
		XRRFreeGamma (gamma);
	if (array != NULL)
		g_ptr_array_unref (array);
	return ret;
}

/**
 * gcm_utils_set_gamma_for_device:
 *
 * Return value: %TRUE for success;
 **/
gboolean
gcm_utils_set_gamma_for_device (GcmDevice *device, GError **error)
{
	gboolean ret = FALSE;
	GcmClut *clut = NULL;
	GcmProfile *profile = NULL;
	GcmXserver *xserver = NULL;
	GnomeRRCrtc *crtc;
	GnomeRROutput *output;
	gint x, y;
	gchar *filename = NULL;
	gchar *filename_systemwide = NULL;
	gfloat gamma;
	gfloat brightness;
	gfloat contrast;
	gchar *output_name;
	gchar *id = NULL;
	guint size;
	gboolean saved;
	gboolean use_global;
	gboolean use_atom;
	gboolean leftmost_screen = FALSE;
	GcmDeviceTypeEnum type;
	GcmScreen *screen = NULL;
	GConfClient *gconf_client = NULL;

	g_return_val_if_fail (device != NULL, FALSE);

	/* use gconf to decide to set LUT or set ATOMs */
	gconf_client = gconf_client_get_default ();

	/* get details about the device */
	g_object_get (device,
		      "id", &id,
		      "type", &type,
		      "saved", &saved,
		      "profile-filename", &filename,
		      "gamma", &gamma,
		      "brightness", &brightness,
		      "contrast", &contrast,
		      "native-device-xrandr", &output_name,
		      NULL);

	/* do no set the gamma for non-display types */
	if (type != GCM_DEVICE_TYPE_ENUM_DISPLAY) {
		g_set_error (error, 1, 0, "not a display: %s", id);
		goto out;
	}

	/* should be set for display types */
	if (output_name == NULL) {
		g_set_error (error, 1, 0, "no output name for display: %s", id);
		goto out;
	}

	/* if not saved, try to find default filename */
	if (!saved && filename == NULL) {
		filename_systemwide = g_strdup_printf ("%s/%s.icc", GCM_SYSTEM_PROFILES_DIR, id);
		ret = g_file_test (filename_systemwide, G_FILE_TEST_EXISTS);
		if (ret) {
			egg_error ("using systemwide %s as profile", filename_systemwide);
			filename = g_strdup (filename_systemwide);
		}
	}

	/* check we have an output */
	screen = gcm_screen_new ();
	output = gcm_screen_get_output_by_name (screen, output_name, error);
	if (output == NULL)
		goto out;

	/* get crtc size */
	crtc = gnome_rr_output_get_crtc (output);
	if (crtc == NULL) {
		g_set_error (error, 1, 0, "failed to get crtc for device: %s", id);
		goto out;
	}

	/* get gamma table size */
	size = gcm_utils_get_gamma_size (crtc, error);
	if (size == 0)
		goto out;

	/* only set the CLUT if we're not seting the atom */
	use_global = gconf_client_get_bool (gconf_client, GCM_SETTINGS_GLOBAL_DISPLAY_CORRECTION, NULL);
	if (use_global && filename != NULL) {
		/* create CLUT */
		profile = gcm_profile_default_new ();
		ret = gcm_profile_parse (profile, filename, error);
		if (!ret)
			goto out;

		/* create a CLUT from the profile */
		clut = gcm_profile_generate_vcgt (profile, size);
	}

	/* create dummy CLUT if we failed */
	if (clut == NULL) {
		clut = gcm_clut_new ();
		g_object_set (clut,
			      "size", size,
			      NULL);
	}

	/* do fine adjustment */
	if (use_global) {
		g_object_set (clut,
			      "gamma", gamma,
			      "brightness", brightness,
			      "contrast", contrast,
			      NULL);
	}

	/* actually set the gamma */
	ret = gcm_utils_set_gamma_for_crtc (crtc, clut, error);
	if (!ret)
		goto out;

	/* set per output and per-screen atoms */
	xserver = gcm_xserver_new ();

	/* is the monitor our primary monitor */
	gnome_rr_output_get_position (output, &x, &y);
	leftmost_screen = (x == 0 && y == 0);

	/* either remove the atoms or set them */
	use_atom = gconf_client_get_bool (gconf_client, GCM_SETTINGS_SET_ICC_PROFILE_ATOM, NULL);
	if (!use_atom || filename == NULL) {

		/* remove the output atom if there's nothing to show */
		ret = gcm_xserver_remove_output_profile (xserver, output_name, error);
		if (!ret)
			goto out;

		/* primary screen */
		if (leftmost_screen) {
			ret = gcm_xserver_remove_root_window_profile (xserver, error);
			if (!ret)
				goto out;
		}
	} else {
		/* set the per-output and per screen profile atoms */
		ret = gcm_xserver_set_output_profile (xserver, output_name, filename, error);
		if (!ret)
			goto out;

		/* primary screen */
		if (leftmost_screen) {
			ret = gcm_xserver_set_root_window_profile (xserver, filename, error);
			if (!ret)
				goto out;
		}
	}
out:
	g_free (id);
	g_free (filename);
	g_free (filename_systemwide);
	g_free (output_name);
	if (gconf_client != NULL)
		g_object_unref (gconf_client);
	if (screen != NULL)
		g_object_unref (screen);
	if (clut != NULL)
		g_object_unref (clut);
	if (profile != NULL)
		g_object_unref (profile);
	if (xserver != NULL)
		g_object_unref (xserver);
	return ret;
}

/**
 * gcm_utils_mkdir_and_copy:
 **/
gboolean
gcm_utils_mkdir_and_copy (const gchar *source, const gchar *destination, GError **error)
{
	gboolean ret;
	gchar *path;
	GFile *sourcefile;
	GFile *destfile;
	GFile *destpath;

	g_return_val_if_fail (source != NULL, FALSE);
	g_return_val_if_fail (destination != NULL, FALSE);

	/* setup paths */
	sourcefile = g_file_new_for_path (source);
	path = g_path_get_dirname (destination);
	destpath = g_file_new_for_path (path);
	destfile = g_file_new_for_path (destination);

	/* ensure desination exists */
	ret = g_file_test (path, G_FILE_TEST_EXISTS);
	if (!ret) {
		ret = g_file_make_directory_with_parents  (destpath, NULL, error);
		if (!ret)
			goto out;
	}

	/* do the copy */
	egg_debug ("copying from %s to %s", source, path);
	ret = g_file_copy (sourcefile, destfile, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, error);
	if (!ret)
		goto out;
out:
	g_free (path);
	g_object_unref (sourcefile);
	g_object_unref (destpath);
	g_object_unref (destfile);
	return ret;
}

/**
 * gcm_utils_get_profile_destination:
 **/
gchar *
gcm_utils_get_profile_destination (const gchar *filename)
{
	gchar *basename;
	gchar *destination;

	g_return_val_if_fail (filename != NULL, NULL);

	/* get destination filename for this source file */
	basename = g_path_get_basename (filename);
	destination = g_build_filename (g_get_home_dir (), GCM_PROFILE_PATH, basename, NULL);

	g_free (basename);
	return destination;
}

/**
 * gcm_utils_ptr_array_to_strv:
 * @array: the GPtrArray of strings
 *
 * Form a composite string array of strings.
 * The data in the GPtrArray is copied.
 *
 * Return value: the string array, or %NULL if invalid
 **/
gchar **
gcm_utils_ptr_array_to_strv (GPtrArray *array)
{
	gchar **value;
	const gchar *value_temp;
	guint i;

	g_return_val_if_fail (array != NULL, NULL);

	/* copy the array to a strv */
	value = g_new0 (gchar *, array->len + 1);
	for (i=0; i<array->len; i++) {
		value_temp = (const gchar *) g_ptr_array_index (array, i);
		value[i] = g_strdup (value_temp);
	}

	return value;
}

/**
 * gcm_gnome_help:
 * @link_id: Subsection of help file, or %NULL.
 **/
gboolean
gcm_gnome_help (const gchar *link_id)
{
	GError *error = NULL;
	gchar *uri;
	gboolean ret = TRUE;

	if (link_id)
		uri = g_strconcat ("ghelp:gnome-color-manager?", link_id, NULL);
	else
		uri = g_strdup ("ghelp:gnome-color-manager");
	egg_debug ("opening uri %s", uri);

	gtk_show_uri (NULL, uri, GDK_CURRENT_TIME, &error);

	if (error != NULL) {
		GtkWidget *d;
		d = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					    GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "%s", error->message);
		gtk_dialog_run (GTK_DIALOG(d));
		gtk_widget_destroy (d);
		g_error_free (error);
		ret = FALSE;
	}

	g_free (uri);
	return ret;
}

/**
 * gcm_utils_alphanum_lcase:
 **/
void
gcm_utils_alphanum_lcase (gchar *data)
{
	guint i;

	g_return_if_fail (data != NULL);

	/* replace unsafe chars, and make lowercase */
	for (i=0; data[i] != '\0'; i++) {
		if (!g_ascii_isalnum (data[i]))
			data[i] = '_';
		data[i] = g_ascii_tolower (data[i]);
	}
}

/**
 * gcm_utils_ensure_sensible_filename:
 **/
void
gcm_utils_ensure_sensible_filename (gchar *data)
{
	guint i;

	g_return_if_fail (data != NULL);

	/* replace unsafe chars, and make lowercase */
	for (i=0; data[i] != '\0'; i++) {
		if (data[i] != ' ' &&
		    data[i] != '-' &&
		    data[i] != '(' &&
		    data[i] != ')' &&
		    data[i] != '[' &&
		    data[i] != ']' &&
		    data[i] != ',' &&
		    !g_ascii_isalnum (data[i]))
			data[i] = '_';
	}
}

/**
 * gcm_utils_get_default_config_location:
 **/
gchar *
gcm_utils_get_default_config_location (void)
{
	gchar *filename;

#ifdef EGG_TEST
	filename = g_strdup ("/tmp/device-profiles.conf");
#else
	/* create default path */
	filename = g_build_filename (g_get_user_config_dir (), "gnome-color-manager", "device-profiles.conf", NULL);
#endif

	return filename;
}

/**
 * gcm_utils_device_type_to_profile_type:
 **/
GcmProfileTypeEnum
gcm_utils_device_type_to_profile_type (GcmDeviceTypeEnum type)
{
	GcmProfileTypeEnum profile_type;
	switch (type) {
	case GCM_DEVICE_TYPE_ENUM_DISPLAY:
		profile_type = GCM_PROFILE_TYPE_ENUM_DISPLAY_DEVICE;
		break;
	case GCM_DEVICE_TYPE_ENUM_CAMERA:
	case GCM_DEVICE_TYPE_ENUM_SCANNER:
		profile_type = GCM_PROFILE_TYPE_ENUM_INPUT_DEVICE;
		break;
	case GCM_DEVICE_TYPE_ENUM_PRINTER:
		profile_type = GCM_PROFILE_TYPE_ENUM_OUTPUT_DEVICE;
		break;
	default:
		egg_warning ("unknown type: %i", type);
		profile_type = GCM_PROFILE_TYPE_ENUM_UNKNOWN;
	}
	return profile_type;
}

/**
 * gcm_utils_format_date_time:
 **/
gchar *
gcm_utils_format_date_time (const struct tm *created)
{
	gchar buffer[256];

	/* TRANSLATORS: this is the profile creation date strftime format */
	strftime (buffer, sizeof(buffer), _("%B %e %Y, %I:%M:%S %p"), created);

	return g_strdup (g_strchug (buffer));
}

/**
 * gcm_intent_enum_to_localized_text:
 **/
const gchar *
gcm_intent_enum_to_localized_text (GcmIntentEnum intent)
{
	if (intent == GCM_INTENT_ENUM_PERCEPTUAL) {
		/* TRANSLATORS: rendering intent: you probably want to google this */
		return _("Perceptual");
	}
	if (intent == GCM_INTENT_ENUM_RELATIVE_COLORMETRIC) {
		/* TRANSLATORS: rendering intent: you probably want to google this */
		return _("Relative colormetric");
	}
	if (intent == GCM_INTENT_ENUM_SATURATION) {
		/* TRANSLATORS: rendering intent: you probably want to google this */
		return _("Saturation");
	}
	if (intent == GCM_INTENT_ENUM_ABSOLUTE_COLORMETRIC) {
		/* TRANSLATORS: rendering intent: you probably want to google this */
		return _("Absolute colormetric");
	}
	return "unknown";
}

/***************************************************************************
 ***                          MAKE CHECK TESTS                           ***
 ***************************************************************************/
#ifdef EGG_TEST
#include "egg-test.h"

void
gcm_utils_test (EggTest *test)
{
	gboolean ret;
	GError *error = NULL;
	GPtrArray *array;
	gchar *text;
	gchar *filename;
	GcmProfileTypeEnum profile_type;
	GcmDeviceTypeEnum device_type;

	if (!egg_test_start (test, "GcmUtils"))
		return;

	/************************************************************/
	egg_test_title (test, "Linkify text");
	text = gcm_utils_linkify ("http://www.dave.org is text http://www.hughsie.com that needs to be linked to http://www.bbc.co.uk really");
	if (g_strcmp0 (text, "<a href=\"http://www.dave.org\">http://www.dave.org</a> is text "
			     "<a href=\"http://www.hughsie.com\">http://www.hughsie.com</a> that needs to be linked to "
			     "<a href=\"http://www.bbc.co.uk\">http://www.bbc.co.uk</a> really") == 0)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "failed to linkify text: %s", text);
	g_free (text);

	/************************************************************/
	egg_test_title (test, "get filename of data file");
	filename = gcm_utils_get_profile_destination ("dave.icc");
	if (g_str_has_suffix (filename, "/.color/icc/dave.icc"))
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "failed to get filename: %s", filename);
	g_free (filename);

	/************************************************************/
	egg_test_title (test, "check is icc profile");
	filename = egg_test_get_data_file ("bluish.icc");
	ret = gcm_utils_is_icc_profile (filename);
	egg_test_assert (test, ret);
	g_free (filename);

	/************************************************************/
	egg_test_title (test, "detect LVDS panels");
	ret = gcm_utils_output_is_lcd_internal ("LVDS1");
	egg_test_assert (test, ret);

	/************************************************************/
	egg_test_title (test, "detect external panels");
	ret = gcm_utils_output_is_lcd_internal ("DVI1");
	egg_test_assert (test, !ret);

	/************************************************************/
	egg_test_title (test, "detect LCD panels");
	ret = gcm_utils_output_is_lcd ("LVDS1");
	egg_test_assert (test, ret);

	/************************************************************/
	egg_test_title (test, "detect LCD panels (2)");
	ret = gcm_utils_output_is_lcd ("DVI1");
	egg_test_assert (test, ret);

	/************************************************************/
	egg_test_title (test, "Make sensible id");
	filename = g_strdup ("Hello\n\rWorld!");
	gcm_utils_alphanum_lcase (filename);
	if (g_strcmp0 (filename, "hello__world_") == 0)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "failed to get filename: %s", filename);
	g_free (filename);

	/************************************************************/
	egg_test_title (test, "Make sensible filename");
	filename = g_strdup ("Hel lo\n\rWo-(r)ld!");
	gcm_utils_ensure_sensible_filename (filename);
	if (g_strcmp0 (filename, "Hel lo__Wo-(r)ld_") == 0)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "failed to get filename: %s", filename);
	g_free (filename);

	/************************************************************/
	egg_test_title (test, "check strip printable");
	text = g_strdup ("1\r34 67_90");
	gcm_utils_ensure_printable (text);
	if (g_strcmp0 (text, "134 67 90") == 0)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "invalid value: %s", text);
	g_free (text);

	/************************************************************/
	egg_test_title (test, "get default config location (when in make check)");
	filename = gcm_utils_get_default_config_location ();
	if (g_strcmp0 (filename, "/tmp/device-profiles.conf") == 0)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "failed to get correct config location: %s", filename);
	g_free (filename);

	/************************************************************/
	egg_test_title (test, "convert valid device type to profile type");
	profile_type = gcm_utils_device_type_to_profile_type (GCM_DEVICE_TYPE_ENUM_SCANNER);
	egg_test_assert (test, (profile_type == GCM_PROFILE_TYPE_ENUM_INPUT_DEVICE));

	/************************************************************/
	egg_test_title (test, "convert invalid device type to profile type");
	profile_type = gcm_utils_device_type_to_profile_type (GCM_DEVICE_TYPE_ENUM_UNKNOWN);
	egg_test_assert (test, (profile_type == GCM_PROFILE_TYPE_ENUM_UNKNOWN));

	egg_test_end (test);
}
#endif


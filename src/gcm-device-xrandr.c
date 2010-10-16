/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009-2010 Richard Hughes <richard@hughsie.com>
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

#include <glib-object.h>
#include <glib/gi18n.h>
#include <math.h>
#include <X11/extensions/Xrandr.h>
#include <gdk/gdkx.h>

#include "gcm-device-xrandr.h"
#include "gcm-edid.h"
#include "gcm-dmi.h"
#include "gcm-utils.h"
#include "gcm-x11-screen.h"
#include "gcm-clut.h"
#include "gcm-x11-output.h"
#include "gcm-x11-screen.h"

static void     gcm_device_xrandr_finalize	(GObject     *object);

#define GCM_DEVICE_XRANDR_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GCM_TYPE_DEVICE_XRANDR, GcmDeviceXrandrPrivate))

/**
 * GcmDeviceXrandrPrivate:
 *
 * Private #GcmDeviceXrandr data
 **/
struct _GcmDeviceXrandrPrivate
{
	gchar				*native_device;
	gchar				*eisa_id;
	gchar				*edid_md5;
	guint				 gamma_size;
	GcmEdid				*edid;
	GcmDmi				*dmi;
	GSettings			*settings;
	GcmX11Screen			*screen;
	gboolean			 remove_atom;
	gboolean			 randr_13;
};

enum {
	PROP_0,
	PROP_NATIVE_DEVICE,
	PROP_EISA_ID,
	PROP_EDID_MD5,
	PROP_LAST
};

G_DEFINE_TYPE (GcmDeviceXrandr, gcm_device_xrandr, GCM_TYPE_DEVICE)

#define GCM_ICC_PROFILE_IN_X_VERSION_MAJOR	0
#define GCM_ICC_PROFILE_IN_X_VERSION_MINOR	3

/**
 * gcm_device_xrandr_get_native_device:
 **/
const gchar *
gcm_device_xrandr_get_native_device (GcmDeviceXrandr *device_xrandr)
{
	return device_xrandr->priv->native_device;
}

/**
 * gcm_device_xrandr_get_eisa_id:
 **/
const gchar *
gcm_device_xrandr_get_eisa_id (GcmDeviceXrandr *device_xrandr)
{
	return device_xrandr->priv->eisa_id;
}

/**
 * gcm_device_xrandr_get_edid_md5:
 **/
const gchar *
gcm_device_xrandr_get_edid_md5 (GcmDeviceXrandr *device_xrandr)
{
	return device_xrandr->priv->edid_md5;
}

/**
 * gcm_device_xrandr_get_output_name:
 *
 * Return value: the output name, free with g_free().
 **/
static gchar *
gcm_device_xrandr_get_output_name (GcmDeviceXrandr *device_xrandr, GcmX11Output *output)
{
	const gchar *output_name;
	const gchar *name;
	const gchar *vendor;
	GString *string;
	gboolean ret;
	guint width = 0;
	guint height = 0;
	GcmDeviceXrandrPrivate *priv = device_xrandr->priv;

	/* blank */
	string = g_string_new ("");

	/* if nothing connected then fall back to the connector name */
	ret = gcm_x11_output_get_connected (output);
	if (!ret)
		goto out;

	/* this is an internal panel, use the DMI data */
	output_name = gcm_x11_output_get_name (output);
	ret = gcm_utils_output_is_lcd_internal (output_name);
	if (ret) {
		/* find the machine details */
		name = gcm_dmi_get_name (priv->dmi);
		vendor = gcm_dmi_get_vendor (priv->dmi);

		/* TRANSLATORS: this is the name of the internal panel */
		output_name = _("Laptop LCD");
	} else {
		/* find the panel details */
		name = gcm_edid_get_monitor_name (priv->edid);
		vendor = gcm_edid_get_vendor_name (priv->edid);
	}

	/* prepend vendor if available */
	if (vendor != NULL && name != NULL)
		g_string_append_printf (string, "%s - %s", vendor, name);
	else if (name != NULL)
		g_string_append (string, name);
	else
		g_string_append (string, output_name);

	/* don't show 'default' even if the nvidia binary blog is craptastic */
	if (g_strcmp0 (string->str, "default") == 0)
		g_string_assign (string, "Unknown Monitor");

out:
	/* append size if available */
	width = gcm_edid_get_width (priv->edid);
	height = gcm_edid_get_height (priv->edid);
	if (width != 0 && height != 0) {
		gfloat diag;

		/* calculate the dialgonal using Pythagorean theorem */
		diag = sqrtf ((powf (width,2)) + (powf (height, 2)));

		/* print it in inches */
		g_string_append_printf (string, " - %i\"", (guint) ((diag * 0.393700787f) + 0.5f));
	}

	return g_string_free (string, FALSE);
}

/**
 * gcm_device_xrandr_get_id_for_xrandr_device:
 **/
static gchar *
gcm_device_xrandr_get_id_for_xrandr_device (GcmDeviceXrandr *device_xrandr, GcmX11Output *output)
{
	const gchar *output_name;
	const gchar *name;
	const gchar *vendor;
	const gchar *ascii;
	const gchar *serial;
	GString *string;
	gboolean ret;
	GcmDeviceXrandrPrivate *priv = device_xrandr->priv;

	/* blank */
	string = g_string_new ("xrandr");

	/* if nothing connected then fall back to the connector name */
	ret = gcm_x11_output_get_connected (output);
	if (!ret)
		goto out;

	/* append data if available */
	vendor = gcm_edid_get_vendor_name (priv->edid);
	if (vendor != NULL)
		g_string_append_printf (string, "_%s", vendor);
	name = gcm_edid_get_monitor_name (priv->edid);
	if (name != NULL)
		g_string_append_printf (string, "_%s", name);
	ascii = gcm_edid_get_eisa_id (priv->edid);
	if (ascii != NULL)
		g_string_append_printf (string, "_%s", ascii);
	serial = gcm_edid_get_serial_number (priv->edid);
	if (serial != NULL)
		g_string_append_printf (string, "_%s", serial);
out:
	/* fallback to the output name */
	if (string->len == 6) {
		output_name = gcm_x11_output_get_name (output);
		ret = gcm_utils_output_is_lcd_internal (output_name);
		if (ret)
			output_name = "LVDS";
		g_string_append_printf (string, "_%s", output_name);
	}

	/* replace unsafe chars */
	gcm_utils_alphanum_lcase (string->str);
	return g_string_free (string, FALSE);
}

/**
 * gcm_device_xrandr_set_from_output:
 **/
gboolean
gcm_device_xrandr_set_from_output (GcmDevice *device, GcmX11Output *output, GError **error)
{
	gchar *title = NULL;
	gchar *id = NULL;
	gboolean ret = TRUE;
	gboolean lcd_internal;
	const gchar *output_name;
	const gchar *serial;
	const gchar *manufacturer;
	const gchar *model;
	guint8 *data = NULL;
	gsize length;
	guint major, minor;
	GcmDeviceXrandrPrivate *priv = GCM_DEVICE_XRANDR(device)->priv;

	/* parse the EDID to get a output specific name */
	ret = gcm_x11_output_get_edid_data (output, &data, &length, NULL);
	if (ret) {
		ret = gcm_edid_parse (priv->edid, data, length, NULL);
		if (!ret) {
			g_set_error (error, 1, 0, "failed to parse edid");
			goto out;
		}
	} else {
		/* reset, as not available */
		gcm_edid_reset (priv->edid);
	}

	/* get details */
	id = gcm_device_xrandr_get_id_for_xrandr_device (GCM_DEVICE_XRANDR(device), output);
	g_debug ("asking to add %s", id);

	/* get data about the display */
	model = gcm_edid_get_monitor_name (priv->edid);
	manufacturer = gcm_edid_get_vendor_name (priv->edid);
	serial = gcm_edid_get_serial_number (priv->edid);
	priv->eisa_id = g_strdup (gcm_edid_get_eisa_id (priv->edid));
	priv->edid_md5 = g_strdup (gcm_edid_get_checksum (priv->edid));

	/* refine data if it's missing */
	output_name = gcm_x11_output_get_name (output);
	lcd_internal = gcm_utils_output_is_lcd_internal (output_name);
	if (lcd_internal && model == NULL)
		model = gcm_dmi_get_version (priv->dmi);

	/* add new device */
	title = gcm_device_xrandr_get_output_name (GCM_DEVICE_XRANDR(device), output);
	g_object_set (device,
		      "kind", GCM_DEVICE_KIND_DISPLAY,
		      "colorspace", GCM_COLORSPACE_RGB,
		      "id", id,
		      "connected", TRUE,
		      "serial", serial,
		      "model", model,
		      "manufacturer", manufacturer,
		      "title", title,
		      "native-device", output_name,
		      NULL);

	/* is XRandR 1.3 compatible */
	gcm_x11_screen_get_randr_version (priv->screen, &major, &minor);
	priv->randr_13 = (major >= 1 && minor >= 3);

	/* success */
	ret = TRUE;
out:
	g_free (data);
	g_free (id);
	g_free (title);
	return ret;
}

/**
 * gcm_device_xrandr_get_xrandr13:
 * @device_xrandr: a valid #GcmDeviceXrandr instance
 *
 * Return value: %TRUE if the display supports XRandr 1.3;
 **/
gboolean
gcm_device_xrandr_get_xrandr13 (GcmDeviceXrandr *device_xrandr)
{
	return device_xrandr->priv->randr_13;
}

/**
 * gcm_device_xrandr_apply_for_output:
 *
 * Return value: %TRUE for success;
 **/
static gboolean
gcm_device_xrandr_apply_for_output (GcmDeviceXrandr *device_xrandr, GcmX11Output *output, GcmClut *clut, GError **error)
{
	gboolean ret = TRUE;
	GPtrArray *array = NULL;
	guint16 *red = NULL;
	guint16 *green = NULL;
	guint16 *blue = NULL;
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
	red = g_new (guint16, array->len);
	green = g_new (guint16, array->len);
	blue = g_new (guint16, array->len);
	for (i=0; i<array->len; i++) {
		data = g_ptr_array_index (array, i);
		red[i] = data->red;
		green[i] = data->green;
		blue[i] = data->blue;
	}

	/* send to LUT */
	ret = gcm_x11_output_set_gamma (output, array->len, red, green, blue, error);
	if (!ret)
		goto out;
out:
	g_free (red);
	g_free (green);
	g_free (blue);
	if (array != NULL)
		g_ptr_array_unref (array);
	return ret;
}

/**
 * gcm_device_xrandr_set_remove_atom:
 *
 * This is set to FALSE at login time when we are sure there are going to be
 * no atoms previously set that have to be removed.
 **/
void
gcm_device_xrandr_set_remove_atom (GcmDeviceXrandr *device_xrandr, gboolean remove_atom)
{
	g_return_if_fail (GCM_IS_DEVICE_XRANDR (device_xrandr));
	device_xrandr->priv->remove_atom = remove_atom;
}

/**
 * gcm_device_xrandr_get_config_data:
 **/
static gchar *
gcm_device_xrandr_get_config_data (GcmDevice *device)
{
	GcmDeviceXrandr *device_xrandr = GCM_DEVICE_XRANDR (device);
	GcmDeviceXrandrPrivate *priv = device_xrandr->priv;
	return g_strdup_printf ("edid-hash=%s", priv->edid_md5);
}

/**
 * gcm_device_xrandr_generate_profile:
 **/
static GcmProfile *
gcm_device_xrandr_generate_profile (GcmDevice *device, GError **error)
{
	gboolean ret;
	const gchar *data;
	gchar *title = NULL;
	GcmProfile *profile;
	GcmDeviceXrandr *device_xrandr = GCM_DEVICE_XRANDR (device);
	GcmDeviceXrandrPrivate *priv = device_xrandr->priv;

	/* create new profile */
	profile = gcm_profile_new ();
	gcm_profile_set_colorspace (profile, 0);
	gcm_profile_set_copyright (profile, "No copyright");
	gcm_profile_set_kind (profile, GCM_PROFILE_KIND_DISPLAY_DEVICE);

	/* get manufacturer */
	data = gcm_edid_get_vendor_name (priv->edid);
	if (data == NULL)
		data = "Unknown vendor";
	gcm_profile_set_manufacturer (profile, data);

	/* get model */
	data = gcm_edid_get_monitor_name (priv->edid);
	if (data == NULL)
		data = "Unknown monitor";
	gcm_profile_set_model (profile, data);

	/* TRANSLATORS: this is prepended to the device title to let the use know it was generated by us automatically */
	title = g_strdup_printf ("%s, %s", _("Automatic"), gcm_device_get_title (device));
	gcm_profile_set_description (profile, title);

	/* generate a profile from the chroma data */
	ret = gcm_profile_create_from_chroma (profile,
					      gcm_edid_get_gamma (priv->edid),
					      gcm_edid_get_red (priv->edid),
					      gcm_edid_get_green (priv->edid),
					      gcm_edid_get_blue (priv->edid),
					      gcm_edid_get_white (priv->edid),
					      error);
	if (!ret) {
		g_object_unref (profile);
		profile = NULL;
		goto out;
	}
out:
	g_free (title);
	return profile;
}

/**
 * gcm_device_xrandr_is_primary:
 *
 * Return value: %TRUE is the monitor is left-most
 **/
gboolean
gcm_device_xrandr_is_primary (GcmDeviceXrandr *device_xrandr)
{
	guint x, y;
	gboolean ret = FALSE;
	GcmX11Output *output;

	/* check we have an output */
	output = gcm_x11_screen_get_output_by_name (device_xrandr->priv->screen,
						    device_xrandr->priv->native_device, NULL);
	if (output == NULL)
		goto out;

	/* is the monitor our primary monitor */
	gcm_x11_output_get_position (output, &x, &y);
	ret = (x == 0 && y == 0);
out:
	return ret;
}

/**
 * gcm_device_xrandr_reset:
 *
 * Clears any VCGT table, so we're ready for profiling
 *
 * Return value: %TRUE for success;
 **/
gboolean
gcm_device_xrandr_reset (GcmDeviceXrandr *device_xrandr, GError **error)
{
	const gchar *id;
	const gchar *output_name;
	gboolean ret = FALSE;
	GcmClut *clut = NULL;
	GcmDeviceKind kind;
	GcmX11Output *output;
	guint size;
	GcmDeviceXrandrPrivate *priv = device_xrandr->priv;

	/* do no set the gamma for non-display types */
	id = gcm_device_get_id (GCM_DEVICE (device_xrandr));
	kind = gcm_device_get_kind (GCM_DEVICE (device_xrandr));
	if (kind != GCM_DEVICE_KIND_DISPLAY) {
		g_set_error (error, 1, 0, "not a display: %s", id);
		goto out;
	}

	/* should be set for display types */
	output_name = gcm_device_xrandr_get_native_device (device_xrandr);
	if (output_name == NULL || output_name[0] == '\0') {
		g_set_error (error, 1, 0, "no output name for display: %s", id);
		goto out;
	}

	/* get GcmX11Output for this device */
	output = gcm_x11_screen_get_output_by_name (priv->screen, output_name, error);
	if (output == NULL)
		goto out;
	size = gcm_x11_output_get_gamma_size (output);

	/* create a linear ramp */
	clut = gcm_clut_new ();
	g_object_set (clut,
		      "size", size,
		      NULL);

	/* actually set the gamma */
	ret = gcm_device_xrandr_apply_for_output (device_xrandr, output, clut, error);
	if (!ret)
		goto out;
out:
	return ret;
}

/**
 * gcm_device_xrandr_apply:
 *
 * Return value: %TRUE for success;
 **/
static gboolean
gcm_device_xrandr_apply (GcmDevice *device, GError **error)
{
	gboolean ret = FALSE;
	GcmClut *clut = NULL;
	GcmProfile *profile = NULL;
	GcmX11Output *output;
	guint x, y;
	const gchar *filename;
	gchar *filename_systemwide = NULL;
	gfloat gamma_adjust;
	gfloat brightness;
	gfloat contrast;
	const gchar *output_name;
	const gchar *id;
	guint size;
	gboolean saved;
	gboolean use_global;
	gboolean use_atom;
	gboolean leftmost_screen = FALSE;
	GFile *file = NULL;
	GcmDeviceKind kind;
	GcmDeviceXrandr *device_xrandr = GCM_DEVICE_XRANDR (device);
	GcmDeviceXrandrPrivate *priv = device_xrandr->priv;

	/* do no set the gamma for non-display types */
	id = gcm_device_get_id (device);
	kind = gcm_device_get_kind (device);
	if (kind != GCM_DEVICE_KIND_DISPLAY) {
		g_set_error (error, 1, 0, "not a display: %s", id);
		goto out;
	}

	/* should be set for display types */
	output_name = gcm_device_xrandr_get_native_device (device_xrandr);
	if (output_name == NULL || output_name[0] == '\0') {
		g_set_error (error, 1, 0, "no output name for display: %s", id);
		goto out;
	}

	/* if not saved, try to find default profile */
	saved = gcm_device_get_saved (device);
	profile = gcm_device_get_default_profile (device);
	if (profile != NULL)
		g_object_ref (profile);
	if (!saved && profile == NULL) {
		filename_systemwide = g_strdup_printf ("%s/icc/%s.icc", GCM_SYSTEM_PROFILES_DIR, id);
		ret = g_file_test (filename_systemwide, G_FILE_TEST_EXISTS);
		if (ret) {
			g_debug ("using systemwide %s as profile", filename_systemwide);
			profile = gcm_profile_new ();
			file = g_file_new_for_path (filename_systemwide);
			ret = gcm_profile_parse (profile, file, error);
			g_object_unref (file);
			if (!ret)
				goto out;
		}
	}

	/* check we have an output */
	output = gcm_x11_screen_get_output_by_name (priv->screen, output_name, error);
	if (output == NULL)
		goto out;
	priv->gamma_size = gcm_x11_output_get_gamma_size (output);
	size = priv->gamma_size;

	/* only set the CLUT if we're not seting the atom */
	use_global = g_settings_get_boolean (priv->settings, GCM_SETTINGS_GLOBAL_DISPLAY_CORRECTION);
	if (use_global && profile != NULL)
		clut = gcm_profile_generate_vcgt (profile, size);

	/* create dummy CLUT if we failed */
	if (clut == NULL) {
		clut = gcm_clut_new ();
		g_object_set (clut,
			      "size", size,
			      NULL);
	}

	/* do fine adjustment */
	if (use_global) {
		gamma_adjust = gcm_device_get_gamma (device);
		brightness = gcm_device_get_brightness (device);
		contrast = gcm_device_get_contrast (device);
		g_object_set (clut,
			      "gamma", gamma_adjust,
			      "brightness", brightness,
			      "contrast", contrast,
			      NULL);
	}

	/* actually set the gamma */
	ret = gcm_device_xrandr_apply_for_output (device_xrandr, output, clut, error);
	if (!ret)
		goto out;

	/* is the monitor our primary monitor */
	gcm_x11_output_get_position (output, &x, &y);
	leftmost_screen = (x == 0 && y == 0);

	/* either remove the atoms or set them */
	use_atom = g_settings_get_boolean (priv->settings, GCM_SETTINGS_SET_ICC_PROFILE_ATOM);
	if (!use_atom || profile == NULL) {

		/* at login we don't need to remove any previously set options */
		if (!priv->remove_atom)
			goto out;

		/* remove the output atom if there's nothing to show */
		ret = gcm_x11_output_remove_profile (output, error);
		if (!ret)
			goto out;

		/* primary screen */
		if (leftmost_screen) {
			ret = gcm_x11_screen_remove_profile (priv->screen, error);
			if (!ret)
				goto out;
			ret = gcm_x11_screen_remove_protocol_version (priv->screen, error);
			if (!ret)
				goto out;
		}
	} else {
		/* set the per-output and per screen profile atoms */
		filename = gcm_profile_get_filename (profile);
		if (filename == NULL) {
			ret = FALSE;
			g_set_error_literal (error, 1, 0, "no filename for profile");
			goto out;
		}
		ret = gcm_x11_output_set_profile (output, filename, error);
		if (!ret)
			goto out;

		/* primary screen */
		if (leftmost_screen) {
			ret = gcm_x11_screen_set_profile (priv->screen, filename, error);
			if (!ret)
				goto out;
			ret = gcm_x11_screen_set_protocol_version (priv->screen,
								GCM_ICC_PROFILE_IN_X_VERSION_MAJOR,
								GCM_ICC_PROFILE_IN_X_VERSION_MINOR,
								error);
			if (!ret)
				goto out;
		}
	}
out:
	g_free (filename_systemwide);
	if (clut != NULL)
		g_object_unref (clut);
	if (profile != NULL)
		g_object_unref (profile);
	return ret;
}

/**
 * gcm_device_xrandr_get_property:
 **/
static void
gcm_device_xrandr_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GcmDeviceXrandr *device_xrandr = GCM_DEVICE_XRANDR (object);
	GcmDeviceXrandrPrivate *priv = device_xrandr->priv;

	switch (prop_id) {
	case PROP_NATIVE_DEVICE:
		g_value_set_string (value, priv->native_device);
		break;
	case PROP_EISA_ID:
		g_value_set_string (value, priv->eisa_id);
		break;
	case PROP_EDID_MD5:
		g_value_set_string (value, priv->edid_md5);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gcm_device_xrandr_set_property:
 **/
static void
gcm_device_xrandr_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	GcmDeviceXrandr *device_xrandr = GCM_DEVICE_XRANDR (object);
	GcmDeviceXrandrPrivate *priv = device_xrandr->priv;

	switch (prop_id) {
	case PROP_NATIVE_DEVICE:
		g_free (priv->native_device);
		priv->native_device = g_strdup (g_value_get_string (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gcm_device_xrandr_class_init:
 **/
static void
gcm_device_xrandr_class_init (GcmDeviceXrandrClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GcmDeviceClass *device_class = GCM_DEVICE_CLASS (klass);

	object_class->finalize = gcm_device_xrandr_finalize;
	object_class->get_property = gcm_device_xrandr_get_property;
	object_class->set_property = gcm_device_xrandr_set_property;

	device_class->apply = gcm_device_xrandr_apply;
	device_class->get_config_data = gcm_device_xrandr_get_config_data;
	device_class->generate_profile = gcm_device_xrandr_generate_profile;

	/**
	 * GcmDeviceXrandr:native-device:
	 */
	pspec = g_param_spec_string ("native-device", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_NATIVE_DEVICE, pspec);

	/**
	 * GcmDeviceXrandr:eisa-id:
	 */
	pspec = g_param_spec_string ("eisa-id", NULL, NULL,
				     NULL,
				     G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_EISA_ID, pspec);

	/**
	 * GcmDeviceXrandr:edid-md5:
	 */
	pspec = g_param_spec_string ("edid-md", NULL, NULL,
				     NULL,
				     G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_EDID_MD5, pspec);

	g_type_class_add_private (klass, sizeof (GcmDeviceXrandrPrivate));
}

/**
 * gcm_device_xrandr_init:
 **/
static void
gcm_device_xrandr_init (GcmDeviceXrandr *device_xrandr)
{
	device_xrandr->priv = GCM_DEVICE_XRANDR_GET_PRIVATE (device_xrandr);
	device_xrandr->priv->native_device = NULL;
	device_xrandr->priv->eisa_id = NULL;
	device_xrandr->priv->edid_md5 = NULL;
	device_xrandr->priv->remove_atom = TRUE;
	device_xrandr->priv->gamma_size = 0;
	device_xrandr->priv->edid = gcm_edid_new ();
	device_xrandr->priv->dmi = gcm_dmi_new ();
	device_xrandr->priv->settings = g_settings_new (GCM_SETTINGS_SCHEMA);
	device_xrandr->priv->screen = gcm_x11_screen_new ();
}

/**
 * gcm_device_xrandr_finalize:
 **/
static void
gcm_device_xrandr_finalize (GObject *object)
{
	GcmDeviceXrandr *device_xrandr = GCM_DEVICE_XRANDR (object);
	GcmDeviceXrandrPrivate *priv = device_xrandr->priv;

	g_free (priv->native_device);
	g_free (priv->eisa_id);
	g_free (priv->edid_md5);
	g_object_unref (priv->edid);
	g_object_unref (priv->dmi);
	g_object_unref (priv->settings);
	g_object_unref (priv->screen);

	G_OBJECT_CLASS (gcm_device_xrandr_parent_class)->finalize (object);
}

/**
 * gcm_device_xrandr_new:
 *
 * Return value: a new #GcmDevice object.
 **/
GcmDevice *
gcm_device_xrandr_new (void)
{
	GcmDevice *device;
	device = g_object_new (GCM_TYPE_DEVICE_XRANDR, NULL);
	return GCM_DEVICE (device);
}


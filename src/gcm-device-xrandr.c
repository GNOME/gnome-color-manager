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
#include <math.h>
#include <libgnomeui/gnome-rr.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/xf86vmode.h>
#include <gdk/gdkx.h>

#include "gcm-device-xrandr.h"
#include "gcm-edid.h"
#include "gcm-dmi.h"
#include "gcm-utils.h"
#include "gcm-xserver.h"
#include "gcm-screen.h"
#include "gcm-clut.h"

#include "egg-debug.h"

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
	guint				 gamma_size;
	GcmEdid				*edid;
	GcmDmi				*dmi;
	GSettings			*settings;
	GcmXserver			*xserver;
	GcmScreen			*screen;
	gboolean			 xrandr_fallback;
};

enum {
	PROP_0,
	PROP_NATIVE_DEVICE,
	PROP_XRANDR_FALLBACK,
	PROP_EISA_ID,
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
 * gcm_device_xrandr_get_fallback:
 **/
gboolean
gcm_device_xrandr_get_fallback (GcmDeviceXrandr *device_xrandr)
{
	return device_xrandr->priv->xrandr_fallback;
}

/**
 * gcm_device_xrandr_get_output_name:
 *
 * Return value: the output name, free with g_free().
 **/
static gchar *
gcm_device_xrandr_get_output_name (GcmDeviceXrandr *device_xrandr, GnomeRROutput *output)
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
	ret = gnome_rr_output_is_connected (output);
	if (!ret)
		goto out;

	/* this is an internal panel, use the DMI data */
	output_name = gnome_rr_output_get_name (output);
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
gcm_device_xrandr_get_id_for_xrandr_device (GcmDeviceXrandr *device_xrandr, GnomeRROutput *output)
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
	ret = gnome_rr_output_is_connected (output);
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
		output_name = gnome_rr_output_get_name (output);
		ret = gcm_utils_output_is_lcd_internal (output_name);
		if (ret)
			output_name = "LVDS";
		g_string_append (string, output_name);
	}

	/* replace unsafe chars */
	gcm_utils_alphanum_lcase (string->str);
	return g_string_free (string, FALSE);
}

/**
 * gcm_device_xrandr_set_from_output:
 **/
gboolean
gcm_device_xrandr_set_from_output (GcmDevice *device, GnomeRROutput *output, GError **error)
{
	gchar *title = NULL;
	gchar *id = NULL;
	gboolean ret = TRUE;
	gboolean lcd_internal;
	const gchar *output_name;
	const gchar *serial;
	const gchar *manufacturer;
	const gchar *model;
	const guint8 *data;
	GcmDeviceXrandrPrivate *priv = GCM_DEVICE_XRANDR(device)->priv;

	/* parse the EDID to get a crtc-specific name, not an output specific name */
	data = gnome_rr_output_get_edid_data (output);
	if (data != NULL) {
		ret = gcm_edid_parse (priv->edid, data, NULL);
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
	egg_debug ("asking to add %s", id);

	/* get data about the display */
	model = gcm_edid_get_monitor_name (priv->edid);
	manufacturer = gcm_edid_get_vendor_name (priv->edid);
	serial = gcm_edid_get_serial_number (priv->edid);
	priv->eisa_id = g_strdup (gcm_edid_get_eisa_id (priv->edid));

	/* refine data if it's missing */
	output_name = gnome_rr_output_get_name (output);
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
out:
	g_free (id);
	g_free (title);
	return ret;
}


/**
 * gcm_device_xrandr_get_gamma_size_fallback:
 **/
static guint
gcm_device_xrandr_get_gamma_size_fallback (void)
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
 * gcm_device_xrandr_get_gamma_size:
 *
 * Return value: the gamma size, or 0 if error;
 **/
static guint
gcm_device_xrandr_get_gamma_size (GcmDeviceXrandr *device_xrandr, GnomeRRCrtc *crtc, GError **error)
{
	guint id;
	guint size;
	GcmDeviceXrandrPrivate *priv = device_xrandr->priv;

	/* use cached value */
	if (priv->gamma_size > 0)
		return priv->gamma_size;

	/* get id that X recognizes */
	id = gnome_rr_crtc_get_id (crtc);

	/* get the value, and catch errors */
	gdk_error_trap_push ();
	size = XRRGetCrtcGammaSize (GDK_DISPLAY(), id);
	if (gdk_error_trap_pop ())
		size = 0;

	/* some drivers support Xrandr 1.2, not 1.3 */
	if (size == 0) {
		priv->xrandr_fallback = TRUE;
		size = gcm_device_xrandr_get_gamma_size_fallback ();
	} else {
		priv->xrandr_fallback = FALSE;
	}

	/* no size, or X popped an error */
	if (size == 0) {
		g_set_error_literal (error, 1, 0, "failed to get gamma size");
		goto out;
	}

	/* save value as this will not change */
	priv->gamma_size = size;
out:
	return size;
}

/**
 * gcm_device_xrandr_apply_fallback:
 **/
static gboolean
gcm_device_xrandr_apply_fallback (XRRCrtcGamma *crtc_gamma, guint size)
{
	Bool rc;

	/* this is per-screen, not per output which is less than ideal */
	gdk_error_trap_push ();
	egg_warning ("using PER-SCREEN gamma tables as driver is not XRANDR 1.3 compliant");
	rc = XF86VidModeSetGammaRamp (GDK_DISPLAY(), gdk_x11_get_default_screen (), size, crtc_gamma->red, crtc_gamma->green, crtc_gamma->blue);
	gdk_error_trap_pop ();

	return rc;
}

/**
 * gcm_device_xrandr_apply_for_crtc:
 *
 * Return value: %TRUE for success;
 **/
static gboolean
gcm_device_xrandr_apply_for_crtc (GcmDeviceXrandr *device_xrandr, GnomeRRCrtc *crtc, GcmClut *clut, GError **error)
{
	guint id;
	gboolean ret = TRUE;
	GPtrArray *array = NULL;
	XRRCrtcGamma *crtc_gamma = NULL;
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
	crtc_gamma = XRRAllocGamma (array->len);
	for (i=0; i<array->len; i++) {
		data = g_ptr_array_index (array, i);
		crtc_gamma->red[i] = data->red;
		crtc_gamma->green[i] = data->green;
		crtc_gamma->blue[i] = data->blue;
	}

	/* get id that X recognizes */
	id = gnome_rr_crtc_get_id (crtc);

	/* get the value, and catch errors */
	gdk_error_trap_push ();
	XRRSetCrtcGamma (GDK_DISPLAY(), id, crtc_gamma);
	gdk_flush ();
	if (gdk_error_trap_pop ()) {
		/* some drivers support Xrandr 1.2, not 1.3 */
		ret = gcm_device_xrandr_apply_fallback (crtc_gamma, array->len);
		if (!ret) {
			g_set_error (error, 1, 0, "failed to set crtc gamma %p (%i) on %i", crtc_gamma, array->len, id);
			goto out;
		}
	}
out:
	if (crtc_gamma != NULL)
		XRRFreeGamma (crtc_gamma);
	if (array != NULL)
		g_ptr_array_unref (array);
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
	GnomeRRCrtc *crtc;
	GnomeRROutput *output;
	gint x, y;
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

	/* if not saved, try to find default filename */
	saved = gcm_device_get_saved (device);
	filename = gcm_device_get_profile_filename (device);
	if (!saved && filename == NULL) {
		filename_systemwide = g_strdup_printf ("%s/%s.icc", GCM_SYSTEM_PROFILES_DIR, id);
		ret = g_file_test (filename_systemwide, G_FILE_TEST_EXISTS);
		if (ret) {
			egg_error ("using systemwide %s as profile", filename_systemwide);
			filename = g_strdup (filename_systemwide);
		}
	}

	/* check we have an output */
	output = gcm_screen_get_output_by_name (priv->screen, output_name, error);
	if (output == NULL)
		goto out;

	/* get crtc size */
	crtc = gnome_rr_output_get_crtc (output);
	if (crtc == NULL) {
		g_set_error (error, 1, 0, "failed to get crtc for device: %s", id);
		goto out;
	}

	/* get gamma table size */
	size = gcm_device_xrandr_get_gamma_size (device_xrandr, crtc, error);
	if (size == 0)
		goto out;

	/* only set the CLUT if we're not seting the atom */
	use_global = g_settings_get_boolean (priv->settings, GCM_SETTINGS_GLOBAL_DISPLAY_CORRECTION);
	if (use_global && filename != NULL) {
		/* create CLUT */
		profile = gcm_profile_default_new ();
		file = g_file_new_for_path (filename);
		ret = gcm_profile_parse (profile, file, error);
		g_object_unref (file);
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
	ret = gcm_device_xrandr_apply_for_crtc (device_xrandr, crtc, clut, error);
	if (!ret)
		goto out;

	/* is the monitor our primary monitor */
	gnome_rr_output_get_position (output, &x, &y);
	leftmost_screen = (x == 0 && y == 0);

	/* either remove the atoms or set them */
	use_atom = g_settings_get_boolean (priv->settings, GCM_SETTINGS_SET_ICC_PROFILE_ATOM);
	if (!use_atom || filename == NULL) {

		/* remove the output atom if there's nothing to show */
		ret = gcm_xserver_remove_output_profile (priv->xserver, output_name, error);
		if (!ret)
			goto out;

		/* primary screen */
		if (leftmost_screen) {
			ret = gcm_xserver_remove_root_window_profile (priv->xserver, error);
			if (!ret)
				goto out;
			ret = gcm_xserver_remove_protocol_version (priv->xserver, error);
			if (!ret)
				goto out;
		}
	} else {
		/* set the per-output and per screen profile atoms */
		ret = gcm_xserver_set_output_profile (priv->xserver, output_name, filename, error);
		if (!ret)
			goto out;

		/* primary screen */
		if (leftmost_screen) {
			ret = gcm_xserver_set_root_window_profile (priv->xserver, filename, error);
			if (!ret)
				goto out;
			ret = gcm_xserver_set_protocol_version (priv->xserver,
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
	case PROP_XRANDR_FALLBACK:
		g_value_set_boolean (value, priv->xrandr_fallback);
		break;
	case PROP_EISA_ID:
		g_value_set_string (value, priv->eisa_id);
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

	/**
	 * GcmDeviceXrandr:native-device:
	 */
	pspec = g_param_spec_string ("native-device", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_NATIVE_DEVICE, pspec);

	/**
	 * GcmDeviceXrandr:xrandr-fallback:
	 */
	pspec = g_param_spec_boolean ("xrandr-fallback", NULL, NULL,
				      FALSE,
				      G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_XRANDR_FALLBACK, pspec);

	/**
	 * GcmDeviceXrandr:eisa-id:
	 */
	pspec = g_param_spec_string ("eisa-id", NULL, NULL,
				     NULL,
				     G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_EISA_ID, pspec);

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
	device_xrandr->priv->xrandr_fallback = FALSE;
	device_xrandr->priv->gamma_size = 0;
	device_xrandr->priv->edid = gcm_edid_new ();
	device_xrandr->priv->dmi = gcm_dmi_new ();
	device_xrandr->priv->settings = g_settings_new (GCM_SETTINGS_SCHEMA);
	device_xrandr->priv->screen = gcm_screen_new ();
	device_xrandr->priv->xserver = gcm_xserver_new ();
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
	g_object_unref (priv->edid);
	g_object_unref (priv->dmi);
	g_object_unref (priv->settings);
	g_object_unref (priv->screen);
	g_object_unref (priv->xserver);

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


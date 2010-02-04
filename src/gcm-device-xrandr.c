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

#include "gcm-device-xrandr.h"
#include "gcm-edid.h"
#include "gcm-dmi.h"
#include "gcm-utils.h"

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
	GcmEdid				*edid;
	GcmDmi				*dmi;
};

enum {
	PROP_0,
	PROP_NATIVE_DEVICE,
	PROP_LAST
};

G_DEFINE_TYPE (GcmDeviceXrandr, gcm_device_xrandr, GCM_TYPE_DEVICE)


/**
 * gcm_device_xrandr_get_output_name:
 *
 * Return value: the output name, free with g_free().
 **/
static gchar *
gcm_device_xrandr_get_output_name (GcmDeviceXrandr *device_xrandr, GnomeRROutput *output)
{
	const gchar *output_name;
	gchar *name = NULL;
	gchar *vendor = NULL;
	GString *string;
	gboolean ret;
	guint width = 0;
	guint height = 0;
	const guint8 *data;
	GcmDeviceXrandrPrivate *priv = device_xrandr->priv;

	/* blank */
	string = g_string_new ("");

	/* if nothing connected then fall back to the connector name */
	ret = gnome_rr_output_is_connected (output);
	if (!ret)
		goto out;

	/* parse the EDID to get a crtc-specific name, not an output specific name */
	data = gnome_rr_output_get_edid_data (output);
	if (data != NULL) {
		ret = gcm_edid_parse (priv->edid, data, NULL);
		if (!ret) {
			egg_warning ("failed to parse edid");
			goto out;
		}
	} else {
		/* reset, as not available */
		gcm_edid_reset (priv->edid);
	}

	/* this is an internal panel, use the DMI data */
	output_name = gnome_rr_output_get_name (output);
	ret = gcm_utils_output_is_lcd_internal (output_name);
	if (ret) {
		/* find the machine details */
		g_object_get (priv->dmi,
			      "name", &name,
			      "vendor", &vendor,
			      NULL);

		/* TRANSLATORS: this is the name of the internal panel */
		output_name = _("Laptop LCD");
	} else {
		/* find the panel details */
		g_object_get (priv->edid,
			      "monitor-name", &name,
			      "vendor-name", &vendor,
			      NULL);
	}

	/* prepend vendor if available */
	if (vendor != NULL && name != NULL)
		g_string_append_printf (string, "%s - %s", vendor, name);
	else if (name != NULL)
		g_string_append (string, name);
	else
		g_string_append (string, output_name);

out:
	/* find the best option */
	g_object_get (priv->edid,
		      "width", &width,
		      "height", &height,
		      NULL);

	/* append size if available */
	if (width != 0 && height != 0) {
		gfloat diag;

		/* calculate the dialgonal using Pythagorean theorem */
		diag = sqrtf ((powf (width,2)) + (powf (height, 2)));

		/* print it in inches */
		g_string_append_printf (string, " - %i\"", (guint) ((diag * 0.393700787f) + 0.5f));
	}

	g_free (name);
	g_free (vendor);
	return g_string_free (string, FALSE);
}

/**
 * gcm_device_xrandr_get_id_for_xrandr_device:
 **/
static gchar *
gcm_device_xrandr_get_id_for_xrandr_device (GcmDeviceXrandr *device_xrandr, GnomeRROutput *output)
{
	const gchar *output_name;
	gchar *name = NULL;
	gchar *vendor = NULL;
	gchar *ascii = NULL;
	gchar *serial = NULL;
	const guint8 *data;
	GString *string;
	gboolean ret;
	GcmDeviceXrandrPrivate *priv = device_xrandr->priv;

	/* blank */
	string = g_string_new ("xrandr");

	/* if nothing connected then fall back to the connector name */
	ret = gnome_rr_output_is_connected (output);
	if (!ret)
		goto out;

	/* parse the EDID to get a crtc-specific name, not an output specific name */
	data = gnome_rr_output_get_edid_data (output);
	if (data != NULL) {
		ret = gcm_edid_parse (priv->edid, data, NULL);
		if (!ret) {
			egg_warning ("failed to parse edid");
			goto out;
		}
	} else {
		/* reset, as not available */
		gcm_edid_reset (priv->edid);
	}

	/* find the best option */
	g_object_get (priv->edid,
		      "monitor-name", &name,
		      "vendor-name", &vendor,
		      "ascii-string", &ascii,
		      "serial-number", &serial,
		      NULL);

	/* append data if available */
	if (vendor != NULL)
		g_string_append_printf (string, "_%s", vendor);
	if (name != NULL)
		g_string_append_printf (string, "_%s", name);
	if (ascii != NULL)
		g_string_append_printf (string, "_%s", ascii);
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

	g_free (name);
	g_free (vendor);
	g_free (ascii);
	g_free (serial);
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
	gchar *serial = NULL;
	gchar *manufacturer = NULL;
	gchar *model = NULL;
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
	g_object_get (priv->edid,
		      "monitor-name", &model,
		      "vendor-name", &manufacturer,
		      "serial-number", &serial,
		      NULL);

	/* refine data if it's missing */
	output_name = gnome_rr_output_get_name (output);
	lcd_internal = gcm_utils_output_is_lcd_internal (output_name);
	if (lcd_internal && model == NULL) {
		g_object_get (priv->dmi,
			      "version", &model,
			      NULL);
	}

	/* add new device */
	title = gcm_device_xrandr_get_output_name (GCM_DEVICE_XRANDR(device), output);
	g_object_set (device,
		      "type", GCM_DEVICE_TYPE_ENUM_DISPLAY,
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
	g_free (serial);
	g_free (manufacturer);
	g_free (model);
	g_free (title);
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
	object_class->finalize = gcm_device_xrandr_finalize;
	object_class->get_property = gcm_device_xrandr_get_property;
	object_class->set_property = gcm_device_xrandr_set_property;


	/**
	 * GcmDeviceXrandr:native-device:
	 */
	pspec = g_param_spec_string ("native-device", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_NATIVE_DEVICE, pspec);

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
	device_xrandr->priv->edid = gcm_edid_new ();
	device_xrandr->priv->dmi = gcm_dmi_new ();
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
	g_object_unref (priv->edid);
	g_object_unref (priv->dmi);

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


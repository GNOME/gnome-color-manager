/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
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

/**
 * SECTION:gcm-ddc-control
 * @short_description: For managing different i2c controls
 *
 * A GObject to use for accessing controls.
 */

#include "config.h"

#include <stdlib.h>
#include <glib-object.h>

#include <gcm-ddc-device.h>
#include <gcm-ddc-control.h>

static void     gcm_ddc_control_finalize	(GObject     *object);

#define GCM_DDC_CONTROL_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GCM_TYPE_DDC_CONTROL, GcmDdcControlPrivate))

/**
 * GcmDdcControlPrivate:
 *
 * Private #GcmDdcControl data
 **/
struct _GcmDdcControlPrivate
{
	guchar			 id;
	gboolean		 supported;
	GcmDdcDevice		*device;
	GcmVerbose		 verbose;
	GArray			*values;
};

enum {
	PROP_0,
	PROP_SUPPORTED,
	PROP_LAST
};

G_DEFINE_TYPE (GcmDdcControl, gcm_ddc_control, G_TYPE_OBJECT)

#define GCM_VCP_SET_DELAY_USECS   		50000

/**
 * gcm_ddc_control_get_description:
 * @control: A valid #GcmDdcControl
 *
 * Gets the string description for the control.
 *
 * Return value: A string value, or %NULL
 *
 * Since: 2.91.1
 **/
const gchar *
gcm_ddc_control_get_description (GcmDdcControl *control)
{
	g_return_val_if_fail (GCM_IS_DDC_CONTROL(control), NULL);
	g_return_val_if_fail (control->priv->id != GCM_VCP_ID_INVALID, NULL);

	return gcm_get_vcp_description_from_index (control->priv->id);
}

/**
 * gcm_ddc_control_is_value_valid:
 **/
static gboolean
gcm_ddc_control_is_value_valid (GcmDdcControl *control, guint16 value, GError **error)
{
	guint i;
	gboolean ret = TRUE;
	GArray *array;
	GString *possible;

	/* no data */
	array = control->priv->values;
	if (array->len == 0)
		goto out;

	/* see if it is present in the description */
	for (i=0; i<array->len; i++) {
		ret = (g_array_index (array, guint16, i) == value);
		if (ret)
			goto out;
	}

	/* not found */
	if (!ret) {
		possible = g_string_new ("");
		for (i=0; i<array->len; i++)
			g_string_append_printf (possible, "%i ", g_array_index (array, guint16, i));
		g_set_error (error, GCM_DDC_CONTROL_ERROR, GCM_DDC_CONTROL_ERROR_FAILED,
			     "%i is not an allowed value for 0x%02x, possible values include %s",
			     value, control->priv->id, possible->str);
		g_string_free (possible, TRUE);
	}
out:
	return ret;
}

/**
 * gcm_ddc_control_set:
 * @control: A valid #GcmDdcControl
 * @value: the value to write
 * @error: a #GError, or %NULL
 *
 * Sets the control value.
 *
 * Return value: %TRUE for success
 *
 * Since: 2.91.1
 **/
gboolean
gcm_ddc_control_set (GcmDdcControl *control, guint16 value, GError **error)
{
	gboolean ret = FALSE;
	guchar buf[4];

	g_return_val_if_fail (GCM_IS_DDC_CONTROL(control), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* check this value is allowed */
	ret = gcm_ddc_control_is_value_valid (control, value, error);
	if (!ret)
		goto out;

	buf[0] = GCM_VCP_SET;
	buf[1] = control->priv->id;
	buf[2] = (value >> 8);
	buf[3] = (value & 255);

	ret = gcm_ddc_device_write (control->priv->device, buf, sizeof(buf), error);
	if (!ret)
		goto out;

	/* Do the delay */
	g_usleep (GCM_VCP_SET_DELAY_USECS);
out:
	return ret;
}

/**
 * gcm_ddc_control_reset:
 * @control: A valid #GcmDdcControl
 * @error: a #GError, or %NULL
 *
 * Resets the control to it's default value.
 *
 * Return value: %TRUE for success
 *
 * Since: 2.91.1
 **/
gboolean
gcm_ddc_control_reset (GcmDdcControl *control, GError **error)
{
	gboolean ret;
	guchar buf[2];

	g_return_val_if_fail (GCM_IS_DDC_CONTROL(control), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	buf[0] = GCM_VCP_RESET;
	buf[1] = control->priv->id;

	ret = gcm_ddc_device_write (control->priv->device, buf, sizeof(buf), error);
	if (!ret)
		goto out;

	/* Do the delay */
	g_usleep (GCM_VCP_SET_DELAY_USECS);
out:
	return ret;
}

/**
 * gcm_ddc_control_request:
 * @control: A valid #GcmDdcControl
 * @value: the value location to write into
 * @maximum: the value maximum location to write into
 * @error: a #GError, or %NULL
 *
 * Get the value of this control.
 *
 * Return value: %TRUE for success
 *
 * Since: 2.91.1
 **/
gboolean
gcm_ddc_control_request (GcmDdcControl *control, guint16 *value, guint16 *maximum, GError **error)
{
	gboolean ret = FALSE;
	guchar buf[8];
	gsize len;

	g_return_val_if_fail (GCM_IS_DDC_CONTROL(control), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* request data */
	buf[0] = GCM_VCP_REQUEST;
	buf[1] = control->priv->id;
	if (!gcm_ddc_device_write (control->priv->device, buf, 2, error))
		goto out;

	/* get data */
	ret = gcm_ddc_device_read (control->priv->device, buf, 8, &len, error);
	if (!ret)
		goto out;

	/* check we got enough data */
	if (len != sizeof(buf)) {
		g_set_error (error, GCM_DDC_CONTROL_ERROR, GCM_DDC_CONTROL_ERROR_FAILED,
			     "Failed to parse control 0x%02x as incorrect length", control->priv->id);
		ret = FALSE;
		goto out;
	}

	/* message type incorrect */
	if (buf[0] != GCM_VCP_REPLY) {
		g_set_error (error, GCM_DDC_CONTROL_ERROR, GCM_DDC_CONTROL_ERROR_FAILED,
			     "Failed to parse control 0x%02x as incorrect command returned", control->priv->id);
		ret = FALSE;
		goto out;
	}

	/* ensure the control is supported by the display */
	if (buf[1] != 0) {
		g_set_error (error, GCM_DDC_CONTROL_ERROR, GCM_DDC_CONTROL_ERROR_FAILED,
			     "Failed to parse control 0x%02x as unsupported", control->priv->id);
		ret = FALSE;
		goto out;
	}

	/* check we are getting the correct control */
	if (buf[2] != control->priv->id) {
		g_set_error (error, GCM_DDC_CONTROL_ERROR, GCM_DDC_CONTROL_ERROR_FAILED,
			     "Failed to parse control 0x%02x as incorrect id returned", control->priv->id);
		ret = FALSE;
		goto out;
	}

	if (value != NULL)
		*value = buf[6] * 256 + buf[7];
	if (maximum != NULL)
		*maximum = buf[4] * 256 + buf[5];
out:
	return ret;
}

/**
 * gcm_ddc_control_run:
 * @control: A valid #GcmDdcControl
 * @error: a #GError, or %NULL
 *
 * Runs the control. Note, this only makes sense for true controls like 'degauss'
 * rather than other VCP values such as 'contrast'.
 *
 * Return value: %TRUE for success
 *
 * Since: 2.91.1
 **/
gboolean
gcm_ddc_control_run (GcmDdcControl *control, GError **error)
{
	guchar buf[1];

	g_return_val_if_fail (GCM_IS_DDC_CONTROL(control), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	buf[0] = control->priv->id;
	return gcm_ddc_device_write (control->priv->device, buf, sizeof(buf), error);
}

/**
 * gcm_ddc_control_parse:
 * @control: A valid #GcmDdcControl
 * @id: the control ID, e.g. %GCM_DDC_CONTROL_ID_BRIGHTNESS
 * @values: a string of permissible values, e.g. "1 3 5 7 8 9" or %NULL
 *
 * Parses a control string for permissable values.
 *
 * Since: 2.91.1
 **/
void
gcm_ddc_control_parse (GcmDdcControl *control, guchar id, const gchar *values)
{
	guint i;
	guint16 value;
	gchar **split = NULL;

	g_return_if_fail (GCM_IS_DDC_CONTROL(control));

	/* just save this */
	control->priv->id = id;
	if (control->priv->verbose == GCM_VERBOSE_OVERVIEW)
		g_debug ("add control 0x%02x (%s)", id, gcm_ddc_control_get_description (control));

	/* do we have any values to parse */
	if (values == NULL)
		goto out;

	/* tokenize */
	split = g_strsplit (values, " ", -1);
	for (i=0; split[i] != NULL; i++) {
		value = atoi (split[i]);
		if (control->priv->verbose == GCM_VERBOSE_OVERVIEW)
			g_debug ("add value %i to control 0x%02x", value, id);
		g_array_append_val (control->priv->values, value);
	}
out:
	g_strfreev (split);
}

/**
 * gcm_ddc_control_set_device:
 * @control: A valid #GcmDdcControl
 * @device: the device that owns this control.
 *
 * Set the device that this control belongs to.
 *
 * Since: 2.91.1
 **/
void
gcm_ddc_control_set_device (GcmDdcControl *control, GcmDdcDevice *device)
{
	g_return_if_fail (GCM_IS_DDC_CONTROL(control));
	g_return_if_fail (GCM_IS_DDC_DEVICE(device));
	control->priv->device = g_object_ref (device);
}

/**
 * gcm_ddc_control_get_id:
 * @control: A valid #GcmDdcControl
 *
 * Gets the ID for this control.
 *
 * Return value: The control ID, e.g. GCM_DDC_CONTROL_ID_BRIGHTNESS
 *
 * Since: 2.91.1
 **/
guchar
gcm_ddc_control_get_id (GcmDdcControl *control)
{
	g_return_val_if_fail (GCM_IS_DDC_CONTROL(control), 0);

	return control->priv->id;
}

/**
 * gcm_ddc_control_get_values:
 * @control: A valid #GcmDdcControl
 *
 * Gets the permissible values of this control.
 *
 * Return value: A GArray of guint16 values, free with g_array_unref().
 *
 * Since: 2.91.1
 **/
GArray *
gcm_ddc_control_get_values (GcmDdcControl *control)
{
	g_return_val_if_fail (GCM_IS_DDC_CONTROL(control), NULL);

	return g_array_ref (control->priv->values);
}

/**
 * gcm_ddc_control_set_verbose:
 * @control: A valid #GcmDdcControl
 * @verbose: if the control should log to stderr.
 *
 * Set the control verbosity.
 *
 * Since: 2.91.1
 **/
void
gcm_ddc_control_set_verbose (GcmDdcControl *control, GcmVerbose verbose)
{
	g_return_if_fail (GCM_IS_DDC_CONTROL(control));
	control->priv->verbose = verbose;
}

/**
 * gcm_ddc_control_error_quark:
 *
 * Return value: Our personal error quark.
 *
 * Since: 2.91.1
 **/
GQuark
gcm_ddc_control_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("gcm_ddc_control_error");
	return quark;
}

/**
 * gcm_ddc_control_get_property:
 **/
static void
gcm_ddc_control_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GcmDdcControl *control = GCM_DDC_CONTROL (object);
	GcmDdcControlPrivate *priv = control->priv;

	switch (prop_id) {
	case PROP_SUPPORTED:
		g_value_set_boolean (value, priv->supported);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gcm_ddc_control_set_property:
 **/
static void
gcm_ddc_control_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gcm_ddc_control_class_init:
 **/
static void
gcm_ddc_control_class_init (GcmDdcControlClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gcm_ddc_control_finalize;
	object_class->get_property = gcm_ddc_control_get_property;
	object_class->set_property = gcm_ddc_control_set_property;

	/**
	 * GcmDdcControl:supported:
	 *
	 * Since: 2.91.1
	 */
	pspec = g_param_spec_boolean ("supported", NULL, "if there are no transactions in progress on this control",
				      TRUE,
				      G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_SUPPORTED, pspec);

	g_type_class_add_private (klass, sizeof (GcmDdcControlPrivate));
}

/**
 * gcm_ddc_control_init:
 **/
static void
gcm_ddc_control_init (GcmDdcControl *control)
{
	control->priv = GCM_DDC_CONTROL_GET_PRIVATE (control);
	control->priv->id = 0xff;
	control->priv->values = g_array_new (FALSE, FALSE, sizeof(guint16));
}

/**
 * gcm_ddc_control_finalize:
 **/
static void
gcm_ddc_control_finalize (GObject *object)
{
	GcmDdcControl *control = GCM_DDC_CONTROL (object);
	GcmDdcControlPrivate *priv = control->priv;

	g_return_if_fail (GCM_IS_DDC_CONTROL(control));

	g_array_free (priv->values, TRUE);
	if (priv->device != NULL)
		g_object_unref (priv->device);

	G_OBJECT_CLASS (gcm_ddc_control_parent_class)->finalize (object);
}

/**
 * gcm_ddc_control_new:
 *
 * Get a control objects.
 *
 * Return value: A new %GcmDdcControl instance
 *
 * Since: 2.91.1
 **/
GcmDdcControl *
gcm_ddc_control_new (void)
{
	GcmDdcControl *control;
	control = g_object_new (GCM_TYPE_DDC_CONTROL, NULL);
	return GCM_DDC_CONTROL (control);
}

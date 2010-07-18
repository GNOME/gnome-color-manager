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
 * SECTION:gcm-sensor
 * @short_description: Sensor object
 *
 * This object allows abstract handling of color sensors.
 */

#include "config.h"

#include <glib-object.h>

#include "gcm-sensor.h"

static void     gcm_sensor_finalize	(GObject     *object);

#define GCM_SENSOR_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GCM_TYPE_SENSOR, GcmSensorPrivate))

/**
 * GcmSensorPrivate:
 *
 * Private #GcmSensor data
 **/
struct _GcmSensorPrivate
{
	GcmSensorKind			 kind;
	GcmSensorOutputType		 output_type;
	gboolean			 supports_display;
	gboolean			 supports_projector;
	gboolean			 supports_printer;
	gboolean			 supports_spot;
	gchar				*vendor;
	gchar				*model;
	gchar				*device;
};

enum {
	PROP_0,
	PROP_VENDOR,
	PROP_MODEL,
	PROP_KIND,
	PROP_SUPPORTS_DISPLAY,
	PROP_SUPPORTS_PROJECTOR,
	PROP_SUPPORTS_PRINTER,
	PROP_SUPPORTS_SPOT,
	PROP_DEVICE,
	PROP_LAST
};

G_DEFINE_TYPE (GcmSensor, gcm_sensor, G_TYPE_OBJECT)

/**
 * gcm_sensor_get_model:
 **/
const gchar *
gcm_sensor_get_model (GcmSensor *sensor)
{
	return sensor->priv->model;
}

/**
 * gcm_sensor_get_vendor:
 **/
const gchar *
gcm_sensor_get_vendor (GcmSensor *sensor)
{
	return sensor->priv->vendor;
}

/**
 * gcm_sensor_supports_display:
 **/
gboolean
gcm_sensor_supports_display (GcmSensor *sensor)
{
	return sensor->priv->supports_display;
}

/**
 * gcm_sensor_supports_projector:
 **/
gboolean
gcm_sensor_supports_projector (GcmSensor *sensor)
{
	return sensor->priv->supports_projector;
}

/**
 * gcm_sensor_supports_printer:
 **/
gboolean
gcm_sensor_supports_printer (GcmSensor *sensor)
{
	return sensor->priv->supports_printer;
}

/**
 * gcm_sensor_supports_spot:
 **/
gboolean
gcm_sensor_supports_spot (GcmSensor *sensor)
{
	return sensor->priv->supports_spot;
}

/**
 * gcm_sensor_get_kind:
 **/
GcmSensorKind
gcm_sensor_get_kind (GcmSensor *sensor)
{
	return sensor->priv->kind;
}

/**
 * gcm_sensor_set_output_type:
 **/
void
gcm_sensor_set_output_type (GcmSensor *sensor, GcmSensorOutputType output_type)
{
	sensor->priv->output_type = output_type;
}

/**
 * gcm_sensor_get_output_type:
 **/
GcmSensorOutputType
gcm_sensor_get_output_type (GcmSensor *sensor)
{
	return sensor->priv->output_type;
}

/**
 * gcm_sensor_startup:
 **/
gboolean
gcm_sensor_startup (GcmSensor *sensor, GError **error)
{
	GcmSensorClass *klass = GCM_SENSOR_GET_CLASS (sensor);
	gboolean ret = FALSE;

	/* coldplug source */
	if (klass->startup == NULL) {
		g_set_error_literal (error,
				     GCM_SENSOR_ERROR,
				     GCM_SENSOR_ERROR_INTERNAL,
				     "no klass support");
		goto out;
	}

	/* proxy */
	ret = klass->startup (sensor, error);
out:
	return ret;
}

/**
 * gcm_sensor_get_ambient:
 **/
gboolean
gcm_sensor_get_ambient (GcmSensor *sensor, gdouble *value, GError **error)
{
	GcmSensorClass *klass = GCM_SENSOR_GET_CLASS (sensor);
	gboolean ret = FALSE;

	/* coldplug source */
	if (klass->get_ambient == NULL) {
		g_set_error_literal (error,
				     GCM_SENSOR_ERROR,
				     GCM_SENSOR_ERROR_INTERNAL,
				     "no klass support");
		goto out;
	}

	/* proxy */
	ret = klass->get_ambient (sensor, value, error);
out:
	return ret;
}

/**
 * gcm_sensor_sample:
 **/
gboolean
gcm_sensor_sample (GcmSensor *sensor, GcmColorXYZ *value, GError **error)
{
	GcmSensorClass *klass = GCM_SENSOR_GET_CLASS (sensor);
	gboolean ret = FALSE;

	/* coldplug source */
	if (klass->sample == NULL) {
		g_set_error_literal (error,
				     GCM_SENSOR_ERROR,
				     GCM_SENSOR_ERROR_INTERNAL,
				     "no klass support");
		goto out;
	}

	/* proxy */
	ret = klass->sample (sensor, value, error);
out:
	return ret;
}

/**
 * gcm_sensor_set_leds:
 **/
gboolean
gcm_sensor_set_leds (GcmSensor *sensor, guint8 value, GError **error)
{
	GcmSensorClass *klass = GCM_SENSOR_GET_CLASS (sensor);
	gboolean ret = FALSE;

	/* coldplug source */
	if (klass->set_leds == NULL) {
		g_set_error_literal (error,
				     GCM_SENSOR_ERROR,
				     GCM_SENSOR_ERROR_INTERNAL,
				     "no klass support");
		goto out;
	}

	/* proxy */
	ret = klass->set_leds (sensor, value, error);
out:
	return ret;
}

/**
 * gcm_sensor_kind_to_string:
 **/
const gchar *
gcm_sensor_kind_to_string (GcmSensorKind sensor_kind)
{
	if (sensor_kind == GCM_SENSOR_KIND_HUEY)
		return "huey";
	if (sensor_kind == GCM_SENSOR_KIND_COLOR_MUNKI)
		return "color-munki";
	if (sensor_kind == GCM_SENSOR_KIND_SPYDER)
		return "spyder";
	if (sensor_kind == GCM_SENSOR_KIND_DTP20)
		return "dtp20";
	if (sensor_kind == GCM_SENSOR_KIND_DTP22)
		return "dtp22";
	if (sensor_kind == GCM_SENSOR_KIND_DTP41)
		return "dtp41";
	if (sensor_kind == GCM_SENSOR_KIND_DTP51)
		return "dtp51";
	if (sensor_kind == GCM_SENSOR_KIND_SPECTRO_SCAN)
		return "spectro-scan";
	if (sensor_kind == GCM_SENSOR_KIND_I1_PRO)
		return "i1-pro";
	if (sensor_kind == GCM_SENSOR_KIND_COLORIMTRE_HCFR)
		return "colorimtre-hcfr";
	return "unknown";
}

/**
 * gcm_sensor_kind_from_string:
 **/
GcmSensorKind
gcm_sensor_kind_from_string (const gchar *sensor_kind)
{
	if (g_strcmp0 (sensor_kind, "huey") == 0)
		return GCM_SENSOR_KIND_HUEY;
	if (g_strcmp0 (sensor_kind, "color-munki") == 0)
		return GCM_SENSOR_KIND_COLOR_MUNKI;
	if (g_strcmp0 (sensor_kind, "spyder") == 0)
		return GCM_SENSOR_KIND_SPYDER;
	if (g_strcmp0 (sensor_kind, "dtp20") == 0)
		return GCM_SENSOR_KIND_DTP20;
	if (g_strcmp0 (sensor_kind, "dtp22") == 0)
		return GCM_SENSOR_KIND_DTP22;
	if (g_strcmp0 (sensor_kind, "dtp41") == 0)
		return GCM_SENSOR_KIND_DTP41;
	if (g_strcmp0 (sensor_kind, "dtp51") == 0)
		return GCM_SENSOR_KIND_DTP51;
	if (g_strcmp0 (sensor_kind, "spectro-scan") == 0)
		return GCM_SENSOR_KIND_SPECTRO_SCAN;
	if (g_strcmp0 (sensor_kind, "i1-pro") == 0)
		return GCM_SENSOR_KIND_I1_PRO;
	if (g_strcmp0 (sensor_kind, "colorimtre-hcfr") == 0)
		return GCM_SENSOR_KIND_COLORIMTRE_HCFR;
	return GCM_SENSOR_KIND_UNKNOWN;
}

/**
 * gcm_sensor_get_property:
 **/
static void
gcm_sensor_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GcmSensor *sensor = GCM_SENSOR (object);
	GcmSensorPrivate *priv = sensor->priv;

	switch (prop_id) {
	case PROP_VENDOR:
		g_value_set_string (value, priv->vendor);
		break;
	case PROP_MODEL:
		g_value_set_string (value, priv->model);
		break;
	case PROP_KIND:
		g_value_set_boolean (value, priv->kind);
		break;
	case PROP_SUPPORTS_DISPLAY:
		g_value_set_boolean (value, priv->supports_display);
		break;
	case PROP_SUPPORTS_PROJECTOR:
		g_value_set_boolean (value, priv->supports_projector);
		break;
	case PROP_SUPPORTS_PRINTER:
		g_value_set_boolean (value, priv->supports_printer);
		break;
	case PROP_SUPPORTS_SPOT:
		g_value_set_boolean (value, priv->supports_spot);
		break;
	case PROP_DEVICE:
		g_value_set_string (value, priv->device);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gcm_sensor_set_property:
 **/
static void
gcm_sensor_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	GcmSensor *sensor = GCM_SENSOR (object);
	GcmSensorPrivate *priv = sensor->priv;

	switch (prop_id) {
	case PROP_DEVICE:
		g_free (priv->device);
		priv->device = g_value_dup_string (value);
		break;
	case PROP_VENDOR:
		g_free (priv->vendor);
		priv->vendor = g_value_dup_string (value);
		break;
	case PROP_MODEL:
		g_free (priv->model);
		priv->model = g_value_dup_string (value);
		break;
	case PROP_KIND:
		priv->kind = g_value_get_uint (value);
		break;
	case PROP_SUPPORTS_DISPLAY:
		priv->supports_display = g_value_get_boolean (value);
		break;
	case PROP_SUPPORTS_PROJECTOR:
		priv->supports_projector = g_value_get_boolean (value);
		break;
	case PROP_SUPPORTS_PRINTER:
		priv->supports_printer = g_value_get_boolean (value);
		break;
	case PROP_SUPPORTS_SPOT:
		priv->supports_spot = g_value_get_boolean (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gcm_sensor_class_init:
 **/
static void
gcm_sensor_class_init (GcmSensorClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gcm_sensor_finalize;
	object_class->get_property = gcm_sensor_get_property;
	object_class->set_property = gcm_sensor_set_property;

	/**
	 * GcmSensor:vendor:
	 */
	pspec = g_param_spec_string ("vendor", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_VENDOR, pspec);

	/**
	 * GcmSensor:model:
	 */
	pspec = g_param_spec_string ("model", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_MODEL, pspec);

	/**
	 * GcmSensor:kind:
	 */
	pspec = g_param_spec_uint ("kind", NULL, NULL,
				   0, G_MAXUINT, GCM_SENSOR_KIND_UNKNOWN,
				   G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_KIND, pspec);

	/**
	 * GcmSensor:supports-display:
	 */
	pspec = g_param_spec_boolean ("supports-display", NULL, NULL,
				      FALSE,
				      G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_SUPPORTS_DISPLAY, pspec);

	/**
	 * GcmSensor:supports-projector:
	 */
	pspec = g_param_spec_boolean ("supports-projector", NULL, NULL,
				      FALSE,
				      G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_SUPPORTS_PROJECTOR, pspec);


	/**
	 * GcmSensor:supports-printer:
	 */
	pspec = g_param_spec_boolean ("supports-printer", NULL, NULL,
				      FALSE,
				      G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_SUPPORTS_PRINTER, pspec);

	/**
	 * GcmSensor:supports-spot:
	 */
	pspec = g_param_spec_boolean ("supports-spot", NULL, NULL,
				      FALSE,
				      G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_SUPPORTS_SPOT, pspec);

	/**
	 * GcmSensor:device:
	 */
	pspec = g_param_spec_string ("device", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_DEVICE, pspec);

	g_type_class_add_private (klass, sizeof (GcmSensorPrivate));
}

/**
 * gcm_sensor_init:
 **/
static void
gcm_sensor_init (GcmSensor *sensor)
{
	sensor->priv = GCM_SENSOR_GET_PRIVATE (sensor);
	sensor->priv->device = NULL;
	sensor->priv->kind = GCM_SENSOR_KIND_UNKNOWN;
}

/**
 * gcm_sensor_finalize:
 **/
static void
gcm_sensor_finalize (GObject *object)
{
	GcmSensor *sensor = GCM_SENSOR (object);
	GcmSensorPrivate *priv = sensor->priv;

	g_free (priv->device);
	g_free (priv->vendor);
	g_free (priv->model);

	G_OBJECT_CLASS (gcm_sensor_parent_class)->finalize (object);
}

/**
 * gcm_sensor_new:
 *
 * Return value: a new GcmSensor object.
 **/
GcmSensor *
gcm_sensor_new (void)
{
	GcmSensor *sensor;
	sensor = g_object_new (GCM_TYPE_SENSOR, NULL);
	return GCM_SENSOR (sensor);
}


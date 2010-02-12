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
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/**
 * SECTION:gcm-calibrate
 * @short_description: Calibration object
 *
 * This object allows calibration functionality using CMS.
 */

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "gcm-calibrate.h"
#include "gcm-utils.h"
#include "gcm-brightness.h"
#include "gcm-color-device.h"

#include "egg-debug.h"

static void     gcm_calibrate_finalize	(GObject     *object);

#define GCM_CALIBRATE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GCM_TYPE_CALIBRATE, GcmCalibratePrivate))

/**
 * GcmCalibratePrivate:
 *
 * Private #GcmCalibrate data
 **/
struct _GcmCalibratePrivate
{
	GcmColorDevice			*color_device;
	gboolean			 is_lcd;
	gboolean			 is_crt;
	gchar				*output_name;
	gchar				*filename_source;
	gchar				*filename_reference;
	gchar				*filename_result;
	gchar				*basename;
	gchar				*manufacturer;
	gchar				*model;
	gchar				*description;
	gchar				*device;
};

enum {
	PROP_0,
	PROP_BASENAME,
	PROP_MODEL,
	PROP_DESCRIPTION,
	PROP_DEVICE,
	PROP_MANUFACTURER,
	PROP_IS_LCD,
	PROP_IS_CRT,
	PROP_OUTPUT_NAME,
	PROP_FILENAME_SOURCE,
	PROP_FILENAME_REFERENCE,
	PROP_FILENAME_RESULT,
	PROP_LAST
};

G_DEFINE_TYPE (GcmCalibrate, gcm_calibrate, G_TYPE_OBJECT)


/**
 * gcm_calibrate_get_time:
 **/
static gchar *
gcm_calibrate_get_time (void)
{
	gchar *text;
	time_t c_time;

	/* get the time now */
	time (&c_time);
	text = g_new0 (gchar, 255);

	/* format text */
	strftime (text, 254, "%H-%M-%S", localtime (&c_time));
	return text;
}

/**
 * gcm_calibrate_get_basename_for_device:
 **/
static gchar *
gcm_calibrate_get_basename_for_device (GcmDevice *device)
{
	gchar *serial = NULL;
	gchar *manufacturer = NULL;
	gchar *model = NULL;
	gchar *timespec = NULL;
	GDate *date = NULL;
	GString *basename;

	/* get device properties */
	g_object_get (device,
		      "serial", &serial,
		      "manufacturer", &manufacturer,
		      "model", &model,
		      NULL);

	/* create date and set it to now */
	date = g_date_new ();
	g_date_set_time_t (date, time (NULL));
	timespec = gcm_calibrate_get_time ();

	/* form basename */
	basename = g_string_new ("GCM");
	if (manufacturer != NULL)
		g_string_append_printf (basename, " - %s", manufacturer);
	if (model != NULL)
		g_string_append_printf (basename, " - %s", model);
	if (serial != NULL)
		g_string_append_printf (basename, " - %s", serial);
	g_string_append_printf (basename, " (%04i-%02i-%02i)", date->year, date->month, date->day);

	/* maybe configure in GConf? */
	if (0)
		g_string_append_printf (basename, " [%s]", timespec);

	g_date_free (date);
	g_free (serial);
	g_free (manufacturer);
	g_free (model);
	g_free (timespec);
	return g_string_free (basename, FALSE);
}

/**
 * gcm_calibrate_set_from_device:
 **/
gboolean
gcm_calibrate_set_from_device (GcmCalibrate *calibrate, GcmDevice *device, GError **error)
{
	gboolean ret = TRUE;
	gchar *native_device = NULL;
	gchar *basename = NULL;
	gchar *manufacturer = NULL;
	gchar *model = NULL;
	gchar *description = NULL;
	gchar *hardware_device = NULL;
	GcmDeviceTypeEnum type;
	GcmCalibratePrivate *priv = calibrate->priv;

	/* get the device */
	g_object_get (device,
		      "native-device", &native_device,
		      "type", &type,
//		      "serial", &basename,
		      "model", &model,
		      "title", &description,
		      "manufacturer", &manufacturer,
		      NULL);
	if (native_device == NULL) {
		g_set_error (error, 1, 0, "failed to get output");
		ret = FALSE;
		goto out;
	}

	/* get a filename based on the serial number */
	basename = gcm_calibrate_get_basename_for_device (device);

	/* get model */
	if (model == NULL) {
		/* TRANSLATORS: this is saved in the profile */
		model = g_strdup (_("Unknown model"));
	}

	/* get description */
	if (description == NULL) {
		/* TRANSLATORS: this is saved in the profile */
		description = g_strdup (_("Unknown device"));
	}

	/* get manufacturer */
	if (manufacturer == NULL) {
		/* TRANSLATORS: this is saved in the profile */
		manufacturer = g_strdup (_("Unknown manufacturer"));
	}

	/* set the proper output name */
	g_object_set (calibrate,
		      "basename", basename,
		      "model", model,
		      "description", description,
		      "manufacturer", manufacturer,
		      NULL);

	/* display specific properties */
	if (type == GCM_DEVICE_TYPE_ENUM_DISPLAY) {

		/* get calibration device model */
		g_object_get (priv->color_device,
			      "model", &hardware_device,
			      NULL);

		/* get device, harder */
		if (hardware_device == NULL) {
			/* TRANSLATORS: this is the formattted custom profile description. "Custom" refers to the fact that it's user generated */
			hardware_device = g_strdup (_("Custom"));
		}

		g_object_set (calibrate,
			      "output-name", native_device,
			      "device", hardware_device,
			      NULL);
	}

out:
	g_free (device);
	g_free (native_device);
	g_free (basename);
	g_free (manufacturer);
	g_free (model);
	g_free (description);
	return ret;
}

/**
 * gcm_calibrate_display:
 **/
gboolean
gcm_calibrate_display (GcmCalibrate *calibrate, GtkWindow *window, GError **error)
{
	GcmCalibrateClass *klass = GCM_CALIBRATE_GET_CLASS (calibrate);
	gboolean ret = TRUE;
	gboolean ret_tmp;
	GtkWidget *dialog;
	GtkResponseType response;
	GString *string = NULL;
	GcmBrightness *brightness = NULL;
	guint percentage = G_MAXUINT;
	GError *error_tmp = NULL;
	GcmCalibratePrivate *priv = calibrate->priv;

	/* coldplug source */
	if (priv->output_name == NULL) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0, "no output name set");
		goto out;
	}

	/* coldplug source */
	if (klass->calibrate_display == NULL) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0, "no support");
		goto out;
	}

	/* this wasn't previously set */
	if (!priv->is_lcd && !priv->is_crt) {
		dialog = gtk_message_dialog_new (window, GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_QUESTION, GTK_BUTTONS_CANCEL,
						 /* TRANSLATORS: title, usually we can tell based on the EDID data or output name */
						 _("Could not auto-detect CRT or LCD"));
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
							  /* TRANSLATORS: dialog message */
							  _("Please indicate if the screen you are trying to profile is a CRT (old type) or a LCD (digital flat panel)."));
		gtk_window_set_icon_name (GTK_WINDOW (dialog), GCM_STOCK_ICON);
		/* TRANSLATORS: button, Liquid Crystal Display */
		gtk_dialog_add_button (GTK_DIALOG (dialog), _("LCD"), GTK_RESPONSE_YES);
		/* TRANSLATORS: button, Cathode Ray Tube */
		gtk_dialog_add_button (GTK_DIALOG (dialog), _("CRT"), GTK_RESPONSE_NO);
		response = gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		if (response == GTK_RESPONSE_YES) {
			g_object_set (calibrate,
				      "is-lcd", TRUE,
				      NULL);
		} else if (response == GTK_RESPONSE_NO) {
			g_object_set (calibrate,
				      "is-crt", TRUE,
				      NULL);
		} else {
			g_set_error_literal (error, 1, 0, "user did not choose crt or lcd");
			ret = FALSE;
			goto out;
		}
	}

	/* show a warning for external monitors */
	ret = gcm_utils_output_is_lcd_internal (priv->output_name);
	if (!ret) {
		string = g_string_new ("");

		/* TRANSLATORS: dialog message, preface */
		g_string_append_printf (string, "%s\n", _("Before calibrating the display, it is recommended to configure your display with the following settings to get optimal results."));

		/* TRANSLATORS: dialog message, preface */
		g_string_append_printf (string, "%s\n\n", _("You may want to consult the owner's manual for your display on how to achieve these settings."));

		/* TRANSLATORS: dialog message, bullet item */
		g_string_append_printf (string, "• %s\n", _("Reset your display to the factory defaults."));

		/* TRANSLATORS: dialog message, bullet item */
		g_string_append_printf (string, "• %s\n", _("Disable dynamic contrast if your display has this feature."));

		/* TRANSLATORS: dialog message, bullet item */
		g_string_append_printf (string, "• %s", _("Configure your display with custom color settings and ensure the RGB channels are set to the same values."));

		/* TRANSLATORS: dialog message, addition to bullet item */
		g_string_append_printf (string, " %s\n", _("If custom color is not available then use a 6500K color temperature."));

		/* TRANSLATORS: dialog message, bullet item */
		g_string_append_printf (string, "• %s\n", _("Adjust the display brightness to a comfortable level for prolonged viewing."));

		/* TRANSLATORS: dialog message, suffix */
		g_string_append_printf (string, "\n%s\n", _("For best results, the display should have been powered for at least 15 minutes before starting the calibration."));


		dialog = gtk_message_dialog_new (window, GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL,
						 /* TRANSLATORS: window title */
						 _("Display setup"));
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", string->str);
		gtk_window_set_icon_name (GTK_WINDOW (dialog), GCM_STOCK_ICON);
		response = gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		if (response != GTK_RESPONSE_OK) {
			g_set_error_literal (error, 1, 0, "user did not follow calibration steps");
			ret = FALSE;
			goto out;
		}
	}

	/* create new helper objects */
	brightness = gcm_brightness_new ();

	/* if we are an internal LCD, we need to set the brightness to maximum */
	ret = gcm_utils_output_is_lcd_internal (priv->output_name);
	if (ret) {
		/* get the old brightness so we can restore state */
		ret = gcm_brightness_get_percentage (brightness, &percentage, &error_tmp);
		if (!ret) {
			egg_warning ("failed to get brightness: %s", error_tmp->message);
			g_error_free (error_tmp);
			/* not fatal */
			error_tmp = NULL;
		}

		/* set the new brightness */
		ret = gcm_brightness_set_percentage (brightness, 100, &error_tmp);
		if (!ret) {
			egg_warning ("failed to set brightness: %s", error_tmp->message);
			g_error_free (error_tmp);
			/* not fatal */
			error_tmp = NULL;
		}
	}

	/* proxy */
	ret = klass->calibrate_display (calibrate, window, error);
out:
	/* restore brightness */
	if (percentage != G_MAXUINT) {
		/* set the new brightness */
		ret_tmp = gcm_brightness_set_percentage (brightness, percentage, &error_tmp);
		if (!ret_tmp) {
			egg_warning ("failed to restore brightness: %s", error_tmp->message);
			g_error_free (error_tmp);
			/* not fatal */
			error = NULL;
		}
	}

	if (brightness != NULL)
		g_object_unref (brightness);
	if (string != NULL)
		g_string_free (string, TRUE);
	return ret;
}

/**
 * gcm_calibrate_device:
 **/
gboolean
gcm_calibrate_device (GcmCalibrate *calibrate, GtkWindow *window, GError **error)
{
	gboolean ret = FALSE;
	GcmCalibrateClass *klass = GCM_CALIBRATE_GET_CLASS (calibrate);

	/* coldplug source */
	if (klass->calibrate_device == NULL) {
		g_set_error_literal (error, 1, 0, "no support");
		goto out;
	}

	/* proxy */
	ret = klass->calibrate_device (calibrate, window, error);
out:
	return ret;
}

/**
 * gcm_calibrate_get_property:
 **/
static void
gcm_calibrate_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GcmCalibrate *calibrate = GCM_CALIBRATE (object);
	GcmCalibratePrivate *priv = calibrate->priv;

	switch (prop_id) {
	case PROP_IS_LCD:
		g_value_set_boolean (value, priv->is_lcd);
		break;
	case PROP_IS_CRT:
		g_value_set_boolean (value, priv->is_crt);
		break;
	case PROP_OUTPUT_NAME:
		g_value_set_string (value, priv->output_name);
		break;
	case PROP_FILENAME_SOURCE:
		g_value_set_string (value, priv->filename_source);
		break;
	case PROP_FILENAME_REFERENCE:
		g_value_set_string (value, priv->filename_reference);
		break;
	case PROP_FILENAME_RESULT:
		g_value_set_string (value, priv->filename_result);
		break;
	case PROP_BASENAME:
		g_value_set_string (value, priv->basename);
		break;
	case PROP_MODEL:
		g_value_set_string (value, priv->model);
		break;
	case PROP_DESCRIPTION:
		g_value_set_string (value, priv->description);
		break;
	case PROP_DEVICE:
		g_value_set_string (value, priv->device);
		break;
	case PROP_MANUFACTURER:
		g_value_set_string (value, priv->manufacturer);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gcm_calibrate_guess_type:
 **/
static void
gcm_calibrate_guess_type (GcmCalibrate *calibrate)
{
	gboolean ret;
	GcmCalibratePrivate *priv = calibrate->priv;

	/* guess based on the output name */
	ret = gcm_utils_output_is_lcd (priv->output_name);
	if (ret) {
		priv->is_lcd = TRUE;
		priv->is_crt = FALSE;
	} else {
		priv->is_lcd = FALSE;
		priv->is_crt = FALSE;
	}
}

/**
 * gcm_calibrate_set_property:
 **/
static void
gcm_calibrate_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	GcmCalibrate *calibrate = GCM_CALIBRATE (object);
	GcmCalibratePrivate *priv = calibrate->priv;

	switch (prop_id) {
	case PROP_IS_LCD:
		priv->is_lcd = g_value_get_boolean (value);
		break;
	case PROP_IS_CRT:
		priv->is_crt = g_value_get_boolean (value);
		break;
	case PROP_OUTPUT_NAME:
		g_free (priv->output_name);
		priv->output_name = g_strdup (g_value_get_string (value));
		gcm_calibrate_guess_type (calibrate);
		break;
	case PROP_FILENAME_SOURCE:
		g_free (priv->filename_source);
		priv->filename_source = g_strdup (g_value_get_string (value));
		break;
	case PROP_FILENAME_REFERENCE:
		g_free (priv->filename_reference);
		priv->filename_reference = g_strdup (g_value_get_string (value));
		break;
	case PROP_FILENAME_RESULT:
		g_free (priv->filename_result);
		priv->filename_result = g_strdup (g_value_get_string (value));
		break;
	case PROP_BASENAME:
		g_free (priv->basename);
		priv->basename = g_strdup (g_value_get_string (value));
		gcm_utils_ensure_sensible_filename (priv->basename);
		break;
	case PROP_MODEL:
		g_free (priv->model);
		priv->model = g_strdup (g_value_get_string (value));
		break;
	case PROP_DESCRIPTION:
		g_free (priv->description);
		priv->description = g_strdup (g_value_get_string (value));
		break;
	case PROP_DEVICE:
		g_free (priv->device);
		priv->device = g_strdup (g_value_get_string (value));
		break;
	case PROP_MANUFACTURER:
		g_free (priv->manufacturer);
		priv->manufacturer = g_strdup (g_value_get_string (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gcm_calibrate_class_init:
 **/
static void
gcm_calibrate_class_init (GcmCalibrateClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gcm_calibrate_finalize;
	object_class->get_property = gcm_calibrate_get_property;
	object_class->set_property = gcm_calibrate_set_property;

	/**
	 * GcmCalibrate:is-lcd:
	 */
	pspec = g_param_spec_boolean ("is-lcd", NULL, NULL,
				      FALSE,
				      G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_IS_LCD, pspec);

	/**
	 * GcmCalibrate:is-crt:
	 */
	pspec = g_param_spec_boolean ("is-crt", NULL, NULL,
				      FALSE,
				      G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_IS_CRT, pspec);

	/**
	 * GcmCalibrate:output-name:
	 */
	pspec = g_param_spec_string ("output-name", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_OUTPUT_NAME, pspec);

	/**
	 * GcmCalibrate:filename-source:
	 */
	pspec = g_param_spec_string ("filename-source", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_FILENAME_SOURCE, pspec);

	/**
	 * GcmCalibrate:filename-reference:
	 */
	pspec = g_param_spec_string ("filename-reference", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_FILENAME_REFERENCE, pspec);

	/**
	 * GcmCalibrate:filename-result:
	 */
	pspec = g_param_spec_string ("filename-result", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_FILENAME_RESULT, pspec);

	/**
	 * GcmCalibrate:basename:
	 */
	pspec = g_param_spec_string ("basename", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_BASENAME, pspec);

	/**
	 * GcmCalibrate:model:
	 */
	pspec = g_param_spec_string ("model", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_MODEL, pspec);

	/**
	 * GcmCalibrate:description:
	 */
	pspec = g_param_spec_string ("description", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_DESCRIPTION, pspec);

	/**
	 * GcmCalibrate:device:
	 */
	pspec = g_param_spec_string ("device", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_DEVICE, pspec);

	/**
	 * GcmCalibrate:manufacturer:
	 */
	pspec = g_param_spec_string ("manufacturer", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_MANUFACTURER, pspec);

	g_type_class_add_private (klass, sizeof (GcmCalibratePrivate));
}

/**
 * gcm_calibrate_init:
 **/
static void
gcm_calibrate_init (GcmCalibrate *calibrate)
{
	calibrate->priv = GCM_CALIBRATE_GET_PRIVATE (calibrate);
	calibrate->priv->output_name = NULL;
	calibrate->priv->filename_source = NULL;
	calibrate->priv->filename_reference = NULL;
	calibrate->priv->filename_result = NULL;
	calibrate->priv->basename = NULL;
	calibrate->priv->manufacturer = NULL;
	calibrate->priv->model = NULL;
	calibrate->priv->description = NULL;
	calibrate->priv->device = NULL;
	calibrate->priv->color_device = gcm_color_device_new ();
}

/**
 * gcm_calibrate_finalize:
 **/
static void
gcm_calibrate_finalize (GObject *object)
{
	GcmCalibrate *calibrate = GCM_CALIBRATE (object);
	GcmCalibratePrivate *priv = calibrate->priv;

	g_free (priv->filename_source);
	g_free (priv->filename_reference);
	g_free (priv->filename_result);
	g_free (priv->output_name);
	g_free (priv->basename);
	g_free (priv->manufacturer);
	g_free (priv->model);
	g_free (priv->description);
	g_free (priv->device);
	g_object_unref (priv->color_device);

	G_OBJECT_CLASS (gcm_calibrate_parent_class)->finalize (object);
}

/**
 * gcm_calibrate_new:
 *
 * Return value: a new GcmCalibrate object.
 **/
GcmCalibrate *
gcm_calibrate_new (void)
{
	GcmCalibrate *calibrate;
	calibrate = g_object_new (GCM_TYPE_CALIBRATE, NULL);
	return GCM_CALIBRATE (calibrate);
}


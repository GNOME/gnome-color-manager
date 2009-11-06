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
 * GNU General Public License for more calibrate.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:gcm-calibrate
 * @short_description: Calibration object
 *
 * This object allows calibration functionality using ArgyllCMS.
 */

#include "config.h"

#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <math.h>
#include <string.h>
#include <gio/gio.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <vte/vte.h>
#include <libgnomeui/gnome-rr.h>

#include "gcm-calibrate.h"
#include "gcm-edid.h"
#include "gcm-utils.h"

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
	gboolean			 is_lcd;
	gboolean			 is_crt;
	guint				 display;
	gchar				*output_name;
	gchar				*basename;
	GMainLoop			*loop;
	GtkWidget			*terminal;
	GtkBuilder			*builder;
	pid_t				 child_pid;
	GtkResponseType			 response;
	GnomeRRScreen			*rr_screen;
	GcmEdid				*edid;
};

enum {
	PROP_0,
	PROP_IS_LCD,
	PROP_IS_CRT,
	PROP_OUTPUT_NAME,
	PROP_LAST
};

G_DEFINE_TYPE (GcmCalibrate, gcm_calibrate, G_TYPE_OBJECT)

/* assume this is writable by users */
#define GCM_CALIBRATE_TEMP_DIR		"/tmp"

/**
 * gcm_calibrate_get_display:
 **/
static guint
gcm_calibrate_get_display (const gchar *output_name, GError **error)
{
	gboolean ret;
	gchar *data = NULL;
	gchar **split = NULL;
	gint exit_status;
	guint display = G_MAXUINT;
	guint i;
	gchar *name;

	/* execute it and capture stderr */
	ret = g_spawn_command_line_sync ("dispcal", NULL, &data, &exit_status, error);
	if (!ret)
		goto out;

	/* split it into lines */
	split = g_strsplit (data, "\n", -1);
	for (i=0; split[i] != NULL; i++) {
		name = g_strdup (split[i]);
		g_strdelimit (name, " ", '\0');
		if (g_strcmp0 (output_name, &name[26]) == 0) {
			display = atoi (&name[4]);
			egg_debug ("found %s mapped to %i", output_name, display);
		}
		g_free (name);
	}

	/* nothing found */
	if (display == G_MAXUINT) {
		if (error != NULL)
			*error = g_error_new (1, 0, "failed to match display");
	}
out:
	g_free (data);
	g_strfreev (split);
	return display;
}

/**
 * gcm_calibrate_get_display_type:
 **/
static gchar
gcm_calibrate_get_display_type (GcmCalibrate *calibrate)
{
	GcmCalibratePrivate *priv = calibrate->priv;
	if (priv->is_lcd)
		return 'l';
	if (priv->is_crt)
		return 'c';
	return '\0';
}

/**
 * gcm_calibrate_set_title:
 **/
static void
gcm_calibrate_set_title (GcmCalibrate *calibrate, const gchar *title)
{
	GcmCalibratePrivate *priv = calibrate->priv;
	GtkWidget *widget;
	gchar *text;

	/* set the text */
	text = g_strdup_printf ("<big><b>%s</b></big>", title);
	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "label_title"));
	gtk_label_set_markup (GTK_LABEL(widget), text);
	g_free (text);
}

/**
 * gcm_calibrate_set_message:
 **/
static void
gcm_calibrate_set_message (GcmCalibrate *calibrate, const gchar *title)
{
	GcmCalibratePrivate *priv = calibrate->priv;
	GtkWidget *widget;
	gchar *text;

	/* set the text */
	text = g_strdup_printf ("%s", title);
	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "label_message"));
	gtk_label_set_markup (GTK_LABEL(widget), text);
	g_free (text);
}

/**
 * gcm_calibrate_setup:
 **/
gboolean
gcm_calibrate_setup (GcmCalibrate *calibrate, GtkWindow *window, GError **error)
{
	GtkWidget *widget;
	GcmCalibratePrivate *priv = calibrate->priv;
	GtkWidget *dialog;
	GtkResponseType response;
	gboolean ret = TRUE;
	GString *string = NULL;

	/* show main UI */
	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "dialog_calibrate"));
	gtk_widget_show_all (widget);
	if (window != NULL)
		gdk_window_set_transient_for (gtk_widget_get_window (widget), gtk_widget_get_window (GTK_WIDGET(window)));

	/* TRANSLATORS: title, hardware refers to a calibration device */
	gcm_calibrate_set_title (calibrate, _("Setup hardware"));
	/* TRANSLATORS: dialog message */
	gcm_calibrate_set_message (calibrate, _("Setting up hardware device for use..."));

	/* this wasn't previously set */
	if (!priv->is_lcd && !priv->is_crt) {
		dialog = gtk_message_dialog_new (GTK_WINDOW(widget), GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_QUESTION, GTK_BUTTONS_CANCEL,
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
			if (error != NULL)
				*error = g_error_new (1, 0, "user did not choose crt or lcd");
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
		g_string_append_printf (string, "%s\n\n", _("You may want to consult the owners manual for your display on how to achieve these settings."));

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

		/* set the message */
		gcm_calibrate_set_message (calibrate, string->str);

		/* wait until user selects okay or closes window */
		g_main_loop_run (priv->loop);

		/* get result */
		if (priv->response != GTK_RESPONSE_OK) {
			if (error != NULL)
				*error = g_error_new (1, 0, "user follow calibration steps");
			ret = FALSE;
			goto out;
		}
	}

	/* success */
	ret = TRUE;
out:
	if (string != NULL)
		g_string_free (string, TRUE);
	return ret;
}

/**
 * gcm_calibrate_debug_argv:
 **/
static void
gcm_calibrate_debug_argv (const gchar *program, gchar **argv)
{
	gchar *join;
	join = g_strjoinv ("  ", argv);
	egg_debug ("running %s  %s", program, join);
	g_free (join);
}

/**
 * gcm_calibrate_task_neutralise:
 **/
static gboolean
gcm_calibrate_task_neutralise (GcmCalibrate *calibrate, GError **error)
{
	gboolean ret = FALSE;
	GcmCalibratePrivate *priv = calibrate->priv;
	gchar type;
	gchar **argv = NULL;
	GtkWidget *widget;
	GnomeRROutput *output;
	const guint8 *data;
	GPtrArray *array = NULL;
	gint x, y;

	/* match up the output name with the device number defined by dispcal */
	priv->display = gcm_calibrate_get_display (priv->output_name, error);
	if (priv->display == G_MAXUINT)
		goto out;

	/* get the device */
	output = gnome_rr_screen_get_output_by_name (priv->rr_screen, priv->output_name);
	if (output == NULL) {
		if (error != NULL)
			*error = g_error_new (1, 0, "failed to get output for %s", priv->output_name);
		goto out;
	}

	/* get edid */
	data = gnome_rr_output_get_edid_data (output);
	if (data == NULL) {
		if (error != NULL)
			*error = g_error_new (1, 0, "failed to get EDID for %s", priv->output_name);
		goto out;
	}

	/* parse edid */
	ret = gcm_edid_parse (priv->edid, data, error);
	if (!ret)
		goto out;

	/* get l-cd or c-rt */
	type = gcm_calibrate_get_display_type (calibrate);

	/* get a filename based on the serial number */
	g_object_get (priv->edid, "serial-number", &priv->basename, NULL);
	if (priv->basename == NULL)
		g_object_get (priv->edid, "monitor-name", &priv->basename, NULL);
	if (priv->basename == NULL)
		g_object_get (priv->edid, "ascii-string", &priv->basename, NULL);
	if (priv->basename == NULL)
		g_object_get (priv->edid, "vendor-name", &priv->basename, NULL);
	if (priv->basename == NULL)
		priv->basename = g_strdup ("custom");

	/* make a suitable filename */
	gcm_utils_alphanum_lcase (priv->basename);
	egg_debug ("using filename basename of %s", priv->basename);

	/* argument array */
	array = g_ptr_array_new_with_free_func (g_free);

	/* setup the command */
	g_ptr_array_add (array, g_strdup ("-v"));
	g_ptr_array_add (array, g_strdup ("-ql"));
	g_ptr_array_add (array, g_strdup ("-m"));
	g_ptr_array_add (array, g_strdup_printf ("-d%i", priv->display));
	g_ptr_array_add (array, g_strdup_printf ("-y%c", type));
//	g_ptr_array_add (array, g_strdup ("-p 0.8,0.5,1.0"));
	g_ptr_array_add (array, g_strdup (priv->basename));
	argv = gcm_utils_ptr_array_to_strv (array);
	gcm_calibrate_debug_argv ("dispcal", argv);

	/* start up the command */
	vte_terminal_reset (VTE_TERMINAL(priv->terminal), TRUE, FALSE);
	priv->child_pid = vte_terminal_fork_command (VTE_TERMINAL(priv->terminal), "dispcal", argv, NULL, GCM_CALIBRATE_TEMP_DIR, FALSE, FALSE, FALSE);

	/* move the dialog out of the way, so the grey square doesn't cover it */
	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "dialog_calibrate"));
	gtk_window_get_position (GTK_WINDOW(widget), &x, &y);
	egg_debug ("currently at %i,%i, moving left", x, y);
	gtk_window_move (GTK_WINDOW(widget), 10, y);

	/* TRANSLATORS: title, device is a hardware color calibration sensor */
	gcm_calibrate_set_title (calibrate, _("Please attach device"));
	/* TRANSLATORS: dialog message, ask user to attach device */
	gcm_calibrate_set_message (calibrate, _("Please attach the hardware device to the center of the screen on the gray square."));

	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "button_ok"));
	gtk_widget_show (widget);

	/* wait until user selects okay or closes window */
	g_main_loop_run (priv->loop);

	/* get result */
	if (priv->response == GTK_RESPONSE_CANCEL) {
		vte_terminal_feed_child (VTE_TERMINAL(priv->terminal), "Q", 1);
		if (error != NULL)
			*error = g_error_new (1, 0, "user did not attach hardware device");
		ret = FALSE;
		goto out;
	}

	/* send the terminal an okay */
	vte_terminal_feed_child (VTE_TERMINAL(priv->terminal), " ", 1);

	/* TRANSLATORS: title, default paramters needed to calibrate */
	gcm_calibrate_set_title (calibrate, _("Getting default parameters"));
	/* TRANSLATORS: dialog message */
	gcm_calibrate_set_message (calibrate, _("This pre-calibrates the screen by sending colored and gray patches to your screen and measuring them with the hardware device."));

	/* wait until finished */
	g_main_loop_run (priv->loop);

	/* get result */
	if (priv->response == GTK_RESPONSE_CANCEL) {
		if (error != NULL)
			*error = g_error_new (1, 0, "calibration was cancelled");
		ret = FALSE;
		goto out;
	}
	if (priv->response == GTK_RESPONSE_REJECT) {
		if (error != NULL)
			*error = g_error_new (1, 0, "command failed to run successfully");
		ret = FALSE;
		goto out;
	}
out:
	if (array != NULL)
		g_ptr_array_unref (array);
	g_strfreev (argv);
	return ret;
}

/**
 * gcm_calibrate_timeout_cb:
 **/
static gboolean
gcm_calibrate_timeout_cb (GcmCalibrate *calibrate)
{
	vte_terminal_feed_child (VTE_TERMINAL(calibrate->priv->terminal), " ", 1);
	return FALSE;
}

/**
 * gcm_calibrate_task_generate_patches:
 **/
static gboolean
gcm_calibrate_task_generate_patches (GcmCalibrate *calibrate, GError **error)
{
	gboolean ret = TRUE;
	GcmCalibratePrivate *priv = calibrate->priv;
	gchar **argv = NULL;
	GPtrArray *array = NULL;

	/* argument array */
	array = g_ptr_array_new_with_free_func (g_free);

	/* setup the command */
	g_ptr_array_add (array, g_strdup ("-v"));
	g_ptr_array_add (array, g_strdup ("-d3"));
	g_ptr_array_add (array, g_strdup ("-f250"));
	g_ptr_array_add (array, g_strdup (priv->basename));
	argv = gcm_utils_ptr_array_to_strv (array);
	gcm_calibrate_debug_argv ("targen", argv);

	/* start up the command */
	vte_terminal_reset (VTE_TERMINAL(priv->terminal), TRUE, FALSE);
	priv->child_pid = vte_terminal_fork_command (VTE_TERMINAL(priv->terminal), "targen", argv, NULL, GCM_CALIBRATE_TEMP_DIR, FALSE, FALSE, FALSE);
	g_timeout_add_seconds (3, (GSourceFunc) gcm_calibrate_timeout_cb, calibrate);

	/* TRANSLATORS: title, patches are specific colours used in calibration */
	gcm_calibrate_set_title (calibrate, _("Generating the patches"));
	/* TRANSLATORS: dialog message */
	gcm_calibrate_set_message (calibrate, _("Generating the patches that will be measured with the hardware device."));

	/* wait until finished */
	g_main_loop_run (priv->loop);

	/* get result */
	if (priv->response == GTK_RESPONSE_CANCEL) {
		if (error != NULL)
			*error = g_error_new (1, 0, "calibration was cancelled");
		ret = FALSE;
		goto out;
	}
	if (priv->response == GTK_RESPONSE_REJECT) {
		if (error != NULL)
			*error = g_error_new (1, 0, "command failed to run successfully");
		ret = FALSE;
		goto out;
	}
out:
	if (array != NULL)
		g_ptr_array_unref (array);
	g_strfreev (argv);
	return ret;
}

/**
 * gcm_calibrate_task_draw_and_measure:
 **/
static gboolean
gcm_calibrate_task_draw_and_measure (GcmCalibrate *calibrate, GError **error)
{
	gboolean ret = TRUE;
	GcmCalibratePrivate *priv = calibrate->priv;
	gchar type;
	gchar **argv = NULL;
	GPtrArray *array = NULL;

	/* get l-cd or c-rt */
	type = gcm_calibrate_get_display_type (calibrate);

	/* argument array */
	array = g_ptr_array_new_with_free_func (g_free);

	/* setup the command */
	g_ptr_array_add (array, g_strdup ("-v"));
	g_ptr_array_add (array, g_strdup_printf ("-d%i", priv->display));
	g_ptr_array_add (array, g_strdup_printf ("-y%c", type));
	g_ptr_array_add (array, g_strdup ("-k"));
	g_ptr_array_add (array, g_strdup_printf ("%s.cal", priv->basename));
	g_ptr_array_add (array, g_strdup (priv->basename));
	argv = gcm_utils_ptr_array_to_strv (array);
	gcm_calibrate_debug_argv ("dispread", argv);

	/* start up the command */
	vte_terminal_reset (VTE_TERMINAL(priv->terminal), TRUE, FALSE);
	priv->child_pid = vte_terminal_fork_command (VTE_TERMINAL(priv->terminal), "dispread", argv, NULL, GCM_CALIBRATE_TEMP_DIR, FALSE, FALSE, FALSE);
	g_timeout_add_seconds (3, (GSourceFunc) gcm_calibrate_timeout_cb, calibrate);

	/* TRANSLATORS: title, drawing means painting to the screen */
	gcm_calibrate_set_title (calibrate, _("Drawing the patches"));
	/* TRANSLATORS: dialog message */
	gcm_calibrate_set_message (calibrate, _("Drawing the generated patches to the screen, which will then be measured by the hardware device."));

	/* wait until finished */
	g_main_loop_run (priv->loop);

	/* get result */
	if (priv->response == GTK_RESPONSE_CANCEL) {
		if (error != NULL)
			*error = g_error_new (1, 0, "calibration was cancelled");
		ret = FALSE;
		goto out;
	}
	if (priv->response == GTK_RESPONSE_REJECT) {
		if (error != NULL)
			*error = g_error_new (1, 0, "command failed to run successfully");
		ret = FALSE;
		goto out;
	}
out:
	if (array != NULL)
		g_ptr_array_unref (array);
	g_strfreev (argv);
	return ret;
}

/**
 * gcm_calibrate_task_generate_profile:
 **/
static gboolean
gcm_calibrate_task_generate_profile (GcmCalibrate *calibrate, GError **error)
{
	gboolean ret = TRUE;
	GcmCalibratePrivate *priv = calibrate->priv;
	gchar **argv = NULL;
	GDate *date;
	gchar *manufacturer = NULL;
	gchar *model = NULL;
	gchar *description_tmp = NULL;
	gchar *description = NULL;
	gchar *copyright = NULL;
	GPtrArray *array = NULL;

	/* create date and set it to now */
	date = g_date_new ();
	g_date_set_time_t (date, time (NULL));

	/* get model */
	g_object_get (priv->edid, "monitor-name", &model, NULL);
	if (model == NULL)
		g_object_get (priv->edid, "ascii-string", &model, NULL);
	if (model == NULL)
		model = g_strdup ("unknown model");

	/* get description */
	g_object_get (priv->edid, "ascii-string", &description_tmp, NULL);
	if (description_tmp == NULL)
		g_object_get (priv->edid, "monitor-name", &description_tmp, NULL);
	if (description_tmp == NULL)
		description_tmp = g_strdup ("calibrated monitor");

        /* TRANSLATORS: this is the formattted custom profile description. "Custom" refers to the fact that it's user generated" */
	description = g_strdup_printf ("%s, %s (%04i-%02i-%02i)", _("Custom"), description_tmp, date->year, date->month, date->day);

	/* get manufacturer */
	g_object_get (priv->edid, "vendor-name", &manufacturer, NULL);
	if (manufacturer == NULL)
		g_object_get (priv->edid, "ascii-string", &manufacturer, NULL);
	if (manufacturer == NULL)
		manufacturer = g_strdup ("unknown manufacturer");

	/* TRANSLATORS: this is the copyright string, where it might be "Copyright (c) 2009 Edward Scissorhands" */
	copyright = g_strdup_printf ("%s %04i %s", _("Copyright (c)"), date->year, g_get_real_name ());

	/* argument array */
	array = g_ptr_array_new_with_free_func (g_free);

	/* setup the command */
	g_ptr_array_add (array, g_strdup ("-v"));
	g_ptr_array_add (array, g_strdup_printf ("-A%s", manufacturer));
	g_ptr_array_add (array, g_strdup_printf ("-M%s", model));
	g_ptr_array_add (array, g_strdup_printf ("-D%s", description));
	g_ptr_array_add (array, g_strdup_printf ("-C%s", copyright));
	g_ptr_array_add (array, g_strdup ("-qm"));
	g_ptr_array_add (array, g_strdup ("-as"));
	g_ptr_array_add (array, g_strdup (priv->basename));
	argv = gcm_utils_ptr_array_to_strv (array);
	gcm_calibrate_debug_argv ("colprof", argv);

	/* start up the command */
	vte_terminal_reset (VTE_TERMINAL(priv->terminal), TRUE, FALSE);
	priv->child_pid = vte_terminal_fork_command (VTE_TERMINAL(priv->terminal), "colprof", argv, NULL, GCM_CALIBRATE_TEMP_DIR, FALSE, FALSE, FALSE);

	/* TRANSLATORS: title, a profile is a ICC file */
	gcm_calibrate_set_title (calibrate, _("Generating the profile"));
	/* TRANSLATORS: dialog message */
	gcm_calibrate_set_message (calibrate, _("Generating the ICC color profile that can be used with this screen."));

	/* wait until finished */
	g_main_loop_run (priv->loop);

	/* get result */
	if (priv->response == GTK_RESPONSE_CANCEL) {
		if (error != NULL)
			*error = g_error_new (1, 0, "calibration was cancelled");
		ret = FALSE;
		goto out;
	}
	if (priv->response == GTK_RESPONSE_REJECT) {
		if (error != NULL)
			*error = g_error_new (1, 0, "command failed to run successfully");
		ret = FALSE;
		goto out;
	}
out:
	if (array != NULL)
		g_ptr_array_unref (array);
	g_date_free (date);
	g_free (manufacturer);
	g_free (model);
	g_free (description_tmp);
	g_free (description);
	g_free (copyright);
	g_strfreev (argv);
	return ret;
}

/**
 * gcm_calibrate_task:
 **/
gboolean
gcm_calibrate_task (GcmCalibrate *calibrate, GcmCalibrateTask task, GError **error)
{
	gboolean ret = FALSE;

	g_return_val_if_fail (GCM_IS_CALIBRATE (calibrate), FALSE);

	/* each option */
	if (task == GCM_CALIBRATE_TASK_NEUTRALISE) {
		ret = gcm_calibrate_task_neutralise (calibrate, error);
		goto out;
	}
	if (task == GCM_CALIBRATE_TASK_GENERATE_PATCHES) {
		ret = gcm_calibrate_task_generate_patches (calibrate, error);
		goto out;
	}
	if (task == GCM_CALIBRATE_TASK_DRAW_AND_MEASURE) {
		ret = gcm_calibrate_task_draw_and_measure (calibrate, error);
		goto out;
	}
	if (task == GCM_CALIBRATE_TASK_GENERATE_PROFILE) {
		ret = gcm_calibrate_task_generate_profile (calibrate, error);
		goto out;
	}
out:
	return ret;
}

/**
 * gcm_calibrate_finish:
 **/
gchar *
gcm_calibrate_finish (GcmCalibrate *calibrate, GError **error)
{
	gchar *filename;
	guint i;
	const gchar *exts[] = {"cal", "ti1", "ti3", NULL};

	/* remove all the temp files */
	for (i=0; exts[i] != NULL; i++) {
		filename = g_strdup_printf ("/tmp/%s.%s", calibrate->priv->basename, exts[i]);
		egg_debug ("removing %s", filename);
		g_unlink (filename);
		g_free (filename);
	}

	/* get the finished icc file */
	filename = g_strdup_printf ("/tmp/%s.icc", calibrate->priv->basename);

	/* we never finished all the steps */
	if (!g_file_test (filename, G_FILE_TEST_EXISTS)) {
		g_free (filename);
		filename = NULL;
		if (error != NULL)
			*error = g_error_new (1, 0, "Could not find completed profile");
	}

	return filename;
}

/**
 * gcm_calibrate_exit_cb:
 **/
static void
gcm_calibrate_exit_cb (VteTerminal *terminal, GcmCalibrate *calibrate)
{
	gint exit_status;

	/* get the child exit status */
	exit_status = vte_terminal_get_child_exit_status (terminal);
	egg_debug ("child exit-status is %i", exit_status);
	if (exit_status == 0)
		calibrate->priv->response = GTK_RESPONSE_ACCEPT;
	else
		calibrate->priv->response = GTK_RESPONSE_REJECT;

	calibrate->priv->child_pid = -1;
	if (g_main_loop_is_running (calibrate->priv->loop))
		g_main_loop_quit (calibrate->priv->loop);
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
 * gcm_calibrate_cancel_cb:
 **/
static void
gcm_calibrate_cancel_cb (GtkWidget *widget, GcmCalibrate *calibrate)
{
	calibrate->priv->response = GTK_RESPONSE_CANCEL;
	if (g_main_loop_is_running (calibrate->priv->loop))
		g_main_loop_quit (calibrate->priv->loop);
}

/**
 * gcm_calibrate_ok_cb:
 **/
static void
gcm_calibrate_ok_cb (GtkWidget *widget, GcmCalibrate *calibrate)
{
	calibrate->priv->response = GTK_RESPONSE_OK;
	if (g_main_loop_is_running (calibrate->priv->loop))
		g_main_loop_quit (calibrate->priv->loop);
	gtk_widget_hide (widget);
}

/**
 * gcm_calibrate_delete_event_cb:
 **/
static gboolean
gcm_calibrate_delete_event_cb (GtkWidget *widget, GdkEvent *event, GcmCalibrate *calibrate)
{
	gcm_calibrate_cancel_cb (widget, calibrate);
	return FALSE;
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

	g_type_class_add_private (klass, sizeof (GcmCalibratePrivate));
}

/**
 * gcm_calibrate_init:
 **/
static void
gcm_calibrate_init (GcmCalibrate *calibrate)
{
	gint retval;
	GError *error = NULL;
	GtkWidget *widget;
	GtkWidget *main_window;

	calibrate->priv = GCM_CALIBRATE_GET_PRIVATE (calibrate);
	calibrate->priv->output_name = NULL;
	calibrate->priv->basename = NULL;
	calibrate->priv->child_pid = -1;
	calibrate->priv->loop = g_main_loop_new (NULL, FALSE);

	/* get UI */
	calibrate->priv->builder = gtk_builder_new ();
	retval = gtk_builder_add_from_file (calibrate->priv->builder, GCM_DATA "/gcm-calibrate.ui", &error);
	if (retval == 0) {
		egg_warning ("failed to load ui: %s", error->message);
		g_error_free (error);
	}

	/* get screen */
	calibrate->priv->rr_screen = gnome_rr_screen_new (gdk_screen_get_default (), NULL, NULL, &error);
	if (calibrate->priv->rr_screen == NULL) {
		egg_warning ("failed to get rr screen: %s", error->message);
		g_error_free (error);
	}

	/* get edid parser */
	calibrate->priv->edid = gcm_edid_new ();

	/* set icon */
	main_window = GTK_WIDGET (gtk_builder_get_object (calibrate->priv->builder, "dialog_calibrate"));
	gtk_window_set_icon_name (GTK_WINDOW (main_window), GCM_STOCK_ICON);
	g_signal_connect (main_window, "delete_event",
			  G_CALLBACK (gcm_calibrate_delete_event_cb), calibrate);

	widget = GTK_WIDGET (gtk_builder_get_object (calibrate->priv->builder, "button_cancel"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gcm_calibrate_cancel_cb), calibrate);
	widget = GTK_WIDGET (gtk_builder_get_object (calibrate->priv->builder, "button_ok"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gcm_calibrate_ok_cb), calibrate);

	/* add vte widget */
	calibrate->priv->terminal = vte_terminal_new ();
	vte_terminal_set_size (VTE_TERMINAL(calibrate->priv->terminal), 40, 10);
	g_signal_connect (calibrate->priv->terminal, "child-exited",
			  G_CALLBACK (gcm_calibrate_exit_cb), calibrate);
	widget = GTK_WIDGET (gtk_builder_get_object (calibrate->priv->builder, "vbox_details"));
	gtk_box_pack_end (GTK_BOX(widget), calibrate->priv->terminal, TRUE, TRUE, 6);
}

/**
 * gcm_calibrate_finalize:
 **/
static void
gcm_calibrate_finalize (GObject *object)
{
	GtkWidget *widget;
	GcmCalibrate *calibrate = GCM_CALIBRATE (object);
	GcmCalibratePrivate *priv = calibrate->priv;

	/* process running */
	if (priv->child_pid != -1) {
		egg_debug ("killing child");
		kill (priv->child_pid, SIGKILL);

		/* wait until child has quit */
		g_main_loop_run (priv->loop);
	}

	/* hide window */
	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "dialog_calibrate"));
	gtk_widget_hide (widget);

	g_free (priv->output_name);
	g_free (priv->basename);
	g_main_loop_unref (priv->loop);
	g_object_unref (priv->builder);
	g_object_unref (priv->edid);
	gnome_rr_screen_destroy (priv->rr_screen);

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


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
	gchar				*filename_source;
	gchar				*filename_reference;
	gchar				*basename;
	gchar				*manufacturer;
	gchar				*model;
	gchar				*description;
	GMainLoop			*loop;
	GtkWidget			*terminal;
	GtkBuilder			*builder;
	pid_t				 child_pid;
	GtkResponseType			 response;
	GnomeRRScreen			*rr_screen;
};

enum {
	PROP_0,
	PROP_BASENAME,
	PROP_MODEL,
	PROP_DESCRIPTION,
	PROP_MANUFACTURER,
	PROP_IS_LCD,
	PROP_IS_CRT,
	PROP_OUTPUT_NAME,
	PROP_FILENAME_SOURCE,
	PROP_FILENAME_REFERENCE,
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
	gboolean ret = TRUE;

	/* show main UI */
	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "dialog_calibrate"));
	gtk_widget_show_all (widget);
	if (window != NULL)
		gdk_window_set_transient_for (gtk_widget_get_window (widget), gtk_widget_get_window (GTK_WIDGET(window)));

	/* TRANSLATORS: title, hardware refers to a calibration device */
	gcm_calibrate_set_title (calibrate, _("Setup hardware"));
	/* TRANSLATORS: dialog message */
	gcm_calibrate_set_message (calibrate, _("Setting up hardware device for use..."));

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
 * gcm_calibrate_display_setup:
 **/
static gboolean
gcm_calibrate_display_setup (GcmCalibrate *calibrate, GError **error)
{
	gboolean ret = TRUE;
	GtkWidget *dialog;
	GtkResponseType response;
	GString *string = NULL;
	GcmCalibratePrivate *priv = calibrate->priv;
	GtkWidget *widget;

	/* this wasn't previously set */
	if (!priv->is_lcd && !priv->is_crt) {
		widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "dialog_calibrate"));
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
 * gcm_calibrate_get_tool_filename:
 **/
static gchar *
gcm_calibrate_get_tool_filename (const gchar *command, GError **error)
{
	gboolean ret;
	gchar *filename;

	/* try the debian filename */
	filename = g_strdup_printf ("/usr/bin/argyll-%s", command);
	ret = g_file_test (filename, G_FILE_TEST_EXISTS);
	if (ret)
		goto out;

	/* try the original argyllcms filename */
	g_free (filename);
	filename = g_strdup_printf ("/usr/bin/%s", command);
	ret = g_file_test (filename, G_FILE_TEST_EXISTS);
	if (ret)
		goto out;

	/* eek */
	g_free (filename);
	filename = NULL;
	if (error != NULL)
		*error = g_error_new (1, 0, "failed to get filename for %s", command);
out:
	return filename;
}

/**
 * gcm_calibrate_display_neutralise:
 **/
static gboolean
gcm_calibrate_display_neutralise (GcmCalibrate *calibrate, GError **error)
{
	gboolean ret = TRUE;
	GcmCalibratePrivate *priv = calibrate->priv;
	gchar type;
	gchar *command = NULL;
	gchar **argv = NULL;
	GtkWidget *widget;
	GnomeRROutput *output;
	GPtrArray *array = NULL;
	gint x, y;

	g_return_val_if_fail (priv->basename != NULL, FALSE);
	g_return_val_if_fail (priv->output_name != NULL, FALSE);

	/* get correct name of the command */
	command = gcm_calibrate_get_tool_filename ("dispcal", error);
	if (command == NULL) {
		ret = FALSE;
		goto out;
	}

	/* match up the output name with the device number defined by dispcal */
	priv->display = gcm_calibrate_get_display (priv->output_name, error);
	if (priv->display == G_MAXUINT) {
		ret = FALSE;
		goto out;
	}

	/* get the device */
	output = gnome_rr_screen_get_output_by_name (priv->rr_screen, priv->output_name);
	if (output == NULL) {
		if (error != NULL)
			*error = g_error_new (1, 0, "failed to get output for %s", priv->output_name);
		ret = FALSE;
		goto out;
	}

	/* get l-cd or c-rt */
	type = gcm_calibrate_get_display_type (calibrate);

	/* make a suitable filename */
	gcm_utils_ensure_sensible_filename (priv->basename);
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
	gcm_calibrate_debug_argv (command, argv);

	/* start up the command */
	vte_terminal_reset (VTE_TERMINAL(priv->terminal), TRUE, FALSE);
	priv->child_pid = vte_terminal_fork_command (VTE_TERMINAL(priv->terminal), command, argv, NULL, GCM_CALIBRATE_TEMP_DIR, FALSE, FALSE, FALSE);

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
	g_free (command);
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
 * gcm_calibrate_display_generate_patches:
 **/
static gboolean
gcm_calibrate_display_generate_patches (GcmCalibrate *calibrate, GError **error)
{
	gboolean ret = TRUE;
	GcmCalibratePrivate *priv = calibrate->priv;
	gchar *command = NULL;
	gchar **argv = NULL;
	GPtrArray *array = NULL;

	g_return_val_if_fail (priv->basename != NULL, FALSE);

	/* get correct name of the command */
	command = gcm_calibrate_get_tool_filename ("targen", error);
	if (command == NULL) {
		ret = FALSE;
		goto out;
	}

	/* argument array */
	array = g_ptr_array_new_with_free_func (g_free);

	/* setup the command */
	g_ptr_array_add (array, g_strdup ("-v"));
	g_ptr_array_add (array, g_strdup ("-d3"));
	g_ptr_array_add (array, g_strdup ("-f250"));
	g_ptr_array_add (array, g_strdup (priv->basename));
	argv = gcm_utils_ptr_array_to_strv (array);
	gcm_calibrate_debug_argv (command, argv);

	/* start up the command */
	vte_terminal_reset (VTE_TERMINAL(priv->terminal), TRUE, FALSE);
	priv->child_pid = vte_terminal_fork_command (VTE_TERMINAL(priv->terminal), command, argv, NULL, GCM_CALIBRATE_TEMP_DIR, FALSE, FALSE, FALSE);
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
	g_free (command);
	g_strfreev (argv);
	return ret;
}

/**
 * gcm_calibrate_display_draw_and_measure:
 **/
static gboolean
gcm_calibrate_display_draw_and_measure (GcmCalibrate *calibrate, GError **error)
{
	gboolean ret = TRUE;
	GcmCalibratePrivate *priv = calibrate->priv;
	gchar type;
	gchar *command = NULL;
	gchar **argv = NULL;
	GPtrArray *array = NULL;

	g_return_val_if_fail (priv->basename != NULL, FALSE);

	/* get correct name of the command */
	command = gcm_calibrate_get_tool_filename ("dispread", error);
	if (command == NULL) {
		ret = FALSE;
		goto out;
	}

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
	gcm_calibrate_debug_argv (command, argv);

	/* start up the command */
	vte_terminal_reset (VTE_TERMINAL(priv->terminal), TRUE, FALSE);
	priv->child_pid = vte_terminal_fork_command (VTE_TERMINAL(priv->terminal), command, argv, NULL, GCM_CALIBRATE_TEMP_DIR, FALSE, FALSE, FALSE);
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
	g_free (command);
	g_strfreev (argv);
	return ret;
}

/**
 * gcm_calibrate_display_generate_profile:
 **/
static gboolean
gcm_calibrate_display_generate_profile (GcmCalibrate *calibrate, GError **error)
{
	gboolean ret = TRUE;
	GcmCalibratePrivate *priv = calibrate->priv;
	gchar **argv = NULL;
	GDate *date = NULL;
	gchar *copyright = NULL;
	gchar *description = NULL;
	gchar *command = NULL;
	GPtrArray *array = NULL;
	GtkWidget *widget;

	g_return_val_if_fail (priv->basename != NULL, FALSE);
	g_return_val_if_fail (priv->description != NULL, FALSE);
	g_return_val_if_fail (priv->manufacturer != NULL, FALSE);
	g_return_val_if_fail (priv->model != NULL, FALSE);

	/* get correct name of the command */
	command = gcm_calibrate_get_tool_filename ("colprof", error);
	if (command == NULL) {
		ret = FALSE;
		goto out;
	}

	/* create date and set it to now */
	date = g_date_new ();
	g_date_set_time_t (date, time (NULL));

        /* TRANSLATORS: this is the formattted custom profile description. "Custom" refers to the fact that it's user generated" */
	description = g_strdup_printf ("%s, %s (%04i-%02i-%02i)", _("Custom"), priv->description, date->year, date->month, date->day);

	/* TRANSLATORS: this is the copyright string, where it might be "Copyright (c) 2009 Edward Scissorhands" */
	copyright = g_strdup_printf ("%s %04i %s", _("Copyright (c)"), date->year, g_get_real_name ());

	/* argument array */
	array = g_ptr_array_new_with_free_func (g_free);

	/* setup the command */
	g_ptr_array_add (array, g_strdup ("-v"));
	g_ptr_array_add (array, g_strdup_printf ("-A%s", g_get_real_name ()));
	g_ptr_array_add (array, g_strdup_printf ("-M%s", priv->model));
	g_ptr_array_add (array, g_strdup_printf ("-D%s", description));
	g_ptr_array_add (array, g_strdup_printf ("-C%s", copyright));
	g_ptr_array_add (array, g_strdup ("-qm"));
	g_ptr_array_add (array, g_strdup ("-as"));
	g_ptr_array_add (array, g_strdup (priv->basename));
	argv = gcm_utils_ptr_array_to_strv (array);
	gcm_calibrate_debug_argv (command, argv);

	/* start up the command */
	vte_terminal_reset (VTE_TERMINAL(priv->terminal), TRUE, FALSE);
	priv->child_pid = vte_terminal_fork_command (VTE_TERMINAL(priv->terminal), command, argv, NULL, GCM_CALIBRATE_TEMP_DIR, FALSE, FALSE, FALSE);

	/* no need for an okay button */
	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "button_ok"));
	gtk_widget_hide (widget);

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
	if (date != NULL)
		g_date_free (date);
	g_free (command);
	g_free (description);
	g_free (copyright);
	g_strfreev (argv);
	return ret;
}

/**
 * gcm_calibrate_device_setup:
 **/
static gboolean
gcm_calibrate_device_setup (GcmCalibrate *calibrate, GError **error)
{
	gboolean ret = TRUE;
	GString *string = NULL;
	GcmCalibratePrivate *priv = calibrate->priv;

	string = g_string_new ("");

	/* TRANSLATORS: title, a profile is a ICC file */
	gcm_calibrate_set_title (calibrate, _("Setting up device"));

	/* TRANSLATORS: dialog message, preface */
	g_string_append_printf (string, "%s\n", _("Before calibrating the device, you have to manually acquire a reference image and save it as a TIFF image file."));

	/* TRANSLATORS: dialog message, preface */
	g_string_append_printf (string, "%s\n", _("Ensure that the contrast and brightness is not changed and color correction profiles are not applied."));

	/* TRANSLATORS: dialog message, suffix */
	g_string_append_printf (string, "%s\n", _("The device sensor should have been cleaned prior to scanning and the output file resolution should be at least 200dpi."));

	/* TRANSLATORS: dialog message, suffix */
	g_string_append_printf (string, "\n%s\n", _("For best results, the reference image should also be less than two years old."));

	/* TRANSLATORS: dialog question */
	g_string_append_printf (string, "\n%s", _("Do you have a scanned TIFF file of a IT8.7/2 reference image?"));

	/* set the message */
	gcm_calibrate_set_message (calibrate, string->str);

	/* wait until user selects okay or closes window */
	g_main_loop_run (priv->loop);

	/* get result */
	if (priv->response != GTK_RESPONSE_OK) {
		if (error != NULL)
			*error = g_error_new (1, 0, "user did not follow calibration steps");
		ret = FALSE;
		goto out;
	}
out:
	if (string != NULL)
		g_string_free (string, TRUE);
	return ret;
}

/**
 * gcm_calibrate_device_copy:
 **/
static gboolean
gcm_calibrate_device_copy (GcmCalibrate *calibrate, GError **error)
{
	gboolean ret;
	gchar *device = NULL;
	gchar *it8cht = NULL;
	gchar *it8ref = NULL;
	gchar *filename = NULL;
	GcmCalibratePrivate *priv = calibrate->priv;

	g_return_val_if_fail (priv->basename != NULL, FALSE);

	/* TRANSLATORS: title, a profile is a ICC file */
	gcm_calibrate_set_title (calibrate, _("Copying files"));
	/* TRANSLATORS: dialog message */
	gcm_calibrate_set_message (calibrate, _("Copying source image, chart data and CIE reference values."));

	/* build filenames */
	filename = g_strdup_printf ("%s.tif", priv->basename);
	device = g_build_filename (GCM_CALIBRATE_TEMP_DIR, filename, NULL);
	it8cht = g_build_filename (GCM_CALIBRATE_TEMP_DIR, "it8.cht", NULL);
	it8ref = g_build_filename (GCM_CALIBRATE_TEMP_DIR, "it8ref.txt", NULL);

	/* copy all files to /tmp as argyllcms doesn't cope well with paths */
	ret = gcm_utils_mkdir_and_copy ("/usr/share/color/argyll/ref/it8.cht", it8cht, error);
	if (!ret)
		goto out;
	ret = gcm_utils_mkdir_and_copy (priv->filename_source, device, error);
	if (!ret)
		goto out;
	ret = gcm_utils_mkdir_and_copy (priv->filename_reference, it8ref, error);
	if (!ret)
		goto out;
out:
	g_free (filename);
	g_free (device);
	g_free (it8cht);
	g_free (it8ref);
	return ret;
}

/**
 * gcm_calibrate_device_measure:
 **/
static gboolean
gcm_calibrate_device_measure (GcmCalibrate *calibrate, GError **error)
{
	gboolean ret = TRUE;
	GcmCalibratePrivate *priv = calibrate->priv;
	gchar **argv = NULL;
	GPtrArray *array = NULL;
	gchar *filename = NULL;
	gchar *command = NULL;

	/* get correct name of the command */
	command = gcm_calibrate_get_tool_filename ("scanin", error);
	if (command == NULL) {
		ret = FALSE;
		goto out;
	}

	/* argument array */
	array = g_ptr_array_new_with_free_func (g_free);
	filename = g_strdup_printf ("%s.tif", priv->basename);

	/* setup the command */
	g_ptr_array_add (array, g_strdup ("-v"));
	g_ptr_array_add (array, g_strdup (filename));
	g_ptr_array_add (array, g_strdup ("it8.cht"));
	g_ptr_array_add (array, g_strdup ("it8ref.txt"));
	argv = gcm_utils_ptr_array_to_strv (array);
	gcm_calibrate_debug_argv (command, argv);

	/* start up the command */
	vte_terminal_reset (VTE_TERMINAL(priv->terminal), TRUE, FALSE);
	priv->child_pid = vte_terminal_fork_command (VTE_TERMINAL(priv->terminal), command, argv, NULL, GCM_CALIBRATE_TEMP_DIR, FALSE, FALSE, FALSE);

	/* TRANSLATORS: title, drawing means painting to the screen */
	gcm_calibrate_set_title (calibrate, _("Measuring the patches"));
	/* TRANSLATORS: dialog message */
	gcm_calibrate_set_message (calibrate, _("Detecting the reference patches and measuring them."));

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
	g_free (filename);
	g_free (command);
	if (array != NULL)
		g_ptr_array_unref (array);
	g_strfreev (argv);
	return ret;
}

/**
 * gcm_calibrate_device_generate_profile:
 **/
static gboolean
gcm_calibrate_device_generate_profile (GcmCalibrate *calibrate, GError **error)
{
	gboolean ret = TRUE;
	GcmCalibratePrivate *priv = calibrate->priv;
	gchar **argv = NULL;
	GDate *date = NULL;
	gchar *description_tmp = NULL;
	gchar *description = NULL;
	gchar *copyright = NULL;
	GPtrArray *array = NULL;
	gchar *command = NULL;

	g_return_val_if_fail (priv->basename != NULL, FALSE);
	g_return_val_if_fail (priv->description != NULL, FALSE);
	g_return_val_if_fail (priv->manufacturer != NULL, FALSE);
	g_return_val_if_fail (priv->model != NULL, FALSE);

	/* get correct name of the command */
	command = gcm_calibrate_get_tool_filename ("colprof", error);
	if (command == NULL) {
		ret = FALSE;
		goto out;
	}

	/* create date and set it to now */
	date = g_date_new ();
	g_date_set_time_t (date, time (NULL));

        /* TRANSLATORS: this is the formattted custom profile description. "Custom" refers to the fact that it's user generated" */
	description = g_strdup_printf ("%s, %s (%04i-%02i-%02i)", _("Custom"), priv->description, date->year, date->month, date->day);

	/* TRANSLATORS: this is the copyright string, where it might be "Copyright (c) 2009 Edward Scissorhands" */
	copyright = g_strdup_printf ("%s %04i %s", _("Copyright (c)"), date->year, g_get_real_name ());

	/* argument array */
	array = g_ptr_array_new_with_free_func (g_free);

	/* setup the command */
	g_ptr_array_add (array, g_strdup ("-v"));
	g_ptr_array_add (array, g_strdup_printf ("-A%s", priv->manufacturer));
	g_ptr_array_add (array, g_strdup_printf ("-M%s", priv->model));
	g_ptr_array_add (array, g_strdup_printf ("-D%s", description));
	g_ptr_array_add (array, g_strdup_printf ("-C%s", copyright));
	g_ptr_array_add (array, g_strdup ("-qm"));
//	g_ptr_array_add (array, g_strdup ("-as"));
	g_ptr_array_add (array, g_strdup (priv->basename));
	argv = gcm_utils_ptr_array_to_strv (array);
	gcm_calibrate_debug_argv (command, argv);

	/* start up the command */
	vte_terminal_reset (VTE_TERMINAL(priv->terminal), TRUE, FALSE);
	priv->child_pid = vte_terminal_fork_command (VTE_TERMINAL(priv->terminal), command, argv, NULL, GCM_CALIBRATE_TEMP_DIR, FALSE, FALSE, FALSE);

	/* TRANSLATORS: title, a profile is a ICC file */
	gcm_calibrate_set_title (calibrate, _("Generating the profile"));
	/* TRANSLATORS: dialog message */
	gcm_calibrate_set_message (calibrate, _("Generating the ICC color profile that can be used with this device."));

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
	if (date != NULL)
		g_date_free (date);
	g_free (description_tmp);
	g_free (copyright);
	g_free (command);
	g_strfreev (argv);
	return ret;
}

/**
 * gcm_calibrate_display:
 **/
gboolean
gcm_calibrate_task (GcmCalibrate *calibrate, GcmCalibrateTask task, GError **error)
{
	gboolean ret = FALSE;

	g_return_val_if_fail (GCM_IS_CALIBRATE (calibrate), FALSE);

	/* each option */
	if (task == GCM_CALIBRATE_TASK_DISPLAY_SETUP) {
		ret = gcm_calibrate_display_setup (calibrate, error);
		goto out;
	}
	if (task == GCM_CALIBRATE_TASK_DISPLAY_NEUTRALISE) {
		ret = gcm_calibrate_display_neutralise (calibrate, error);
		goto out;
	}
	if (task == GCM_CALIBRATE_TASK_DISPLAY_GENERATE_PATCHES) {
		ret = gcm_calibrate_display_generate_patches (calibrate, error);
		goto out;
	}
	if (task == GCM_CALIBRATE_TASK_DISPLAY_DRAW_AND_MEASURE) {
		ret = gcm_calibrate_display_draw_and_measure (calibrate, error);
		goto out;
	}
	if (task == GCM_CALIBRATE_TASK_DISPLAY_GENERATE_PROFILE) {
		ret = gcm_calibrate_display_generate_profile (calibrate, error);
		goto out;
	}
	if (task == GCM_CALIBRATE_TASK_DEVICE_SETUP) {
		ret = gcm_calibrate_device_setup (calibrate, error);
		goto out;
	}
	if (task == GCM_CALIBRATE_TASK_DEVICE_COPY) {
		ret = gcm_calibrate_device_copy (calibrate, error);
		goto out;
	}
	if (task == GCM_CALIBRATE_TASK_DEVICE_MEASURE) {
		ret = gcm_calibrate_device_measure (calibrate, error);
		goto out;
	}
	if (task == GCM_CALIBRATE_TASK_DEVICE_GENERATE_PROFILE) {
		ret = gcm_calibrate_device_generate_profile (calibrate, error);
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
	gboolean ret;
	const gchar *exts[] = {"cal", "ti1", "ti3", "tif", NULL};
	const gchar *filenames[] = {"it8.cht", "it8ref.txt", NULL};
	GcmCalibratePrivate *priv = calibrate->priv;

	/* remove all the temp files */
	if (priv->basename != NULL) {
		for (i=0; exts[i] != NULL; i++) {
			filename = g_strdup_printf ("%s/%s.%s", GCM_CALIBRATE_TEMP_DIR, priv->basename, exts[i]);
			ret = g_file_test (filename, G_FILE_TEST_IS_REGULAR);
			if (ret) {
				egg_debug ("removing %s", filename);
				g_unlink (filename);
			}
			g_free (filename);
		}
	}

	/* remove all the temp files */
	for (i=0; filenames[i] != NULL; i++) {
		filename = g_strdup_printf ("%s/%s", GCM_CALIBRATE_TEMP_DIR, filenames[i]);
		ret = g_file_test (filename, G_FILE_TEST_IS_REGULAR);
		if (ret) {
			egg_debug ("removing %s", filename);
			g_unlink (filename);
		}
		g_free (filename);
	}

	/* we can't have finished with success */
	if (priv->basename == NULL) {
		filename = NULL;
		if (error != NULL)
			*error = g_error_new (1, 0, "profile was not generated");
		goto out;
	}

	/* get the finished icc file */
	filename = g_strdup_printf ("%s/%s.icc", GCM_CALIBRATE_TEMP_DIR, priv->basename);

	/* we never finished all the steps */
	if (!g_file_test (filename, G_FILE_TEST_EXISTS)) {
		g_free (filename);
		filename = NULL;
		if (error != NULL)
			*error = g_error_new (1, 0, "could not find completed profile");
	}
out:
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
	case PROP_FILENAME_SOURCE:
		g_value_set_string (value, priv->filename_source);
		break;
	case PROP_FILENAME_REFERENCE:
		g_value_set_string (value, priv->filename_reference);
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
	case PROP_FILENAME_SOURCE:
		g_free (priv->filename_source);
		priv->filename_source = g_strdup (g_value_get_string (value));
		break;
	case PROP_FILENAME_REFERENCE:
		g_free (priv->filename_reference);
		priv->filename_reference = g_strdup (g_value_get_string (value));
		break;
	case PROP_BASENAME:
		g_free (priv->basename);
		priv->basename = g_strdup (g_value_get_string (value));
		break;
	case PROP_MODEL:
		g_free (priv->model);
		priv->model = g_strdup (g_value_get_string (value));
		break;
	case PROP_DESCRIPTION:
		g_free (priv->description);
		priv->description = g_strdup (g_value_get_string (value));
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
	gint retval;
	GError *error = NULL;
	GtkWidget *widget;
	GtkWidget *main_window;

	calibrate->priv = GCM_CALIBRATE_GET_PRIVATE (calibrate);
	calibrate->priv->output_name = NULL;
	calibrate->priv->filename_source = NULL;
	calibrate->priv->filename_reference = NULL;
	calibrate->priv->basename = NULL;
	calibrate->priv->manufacturer = NULL;
	calibrate->priv->model = NULL;
	calibrate->priv->description = NULL;
	calibrate->priv->child_pid = -1;
	calibrate->priv->loop = g_main_loop_new (NULL, FALSE);

	/* get UI */
	calibrate->priv->builder = gtk_builder_new ();
	retval = gtk_builder_add_from_file (calibrate->priv->builder, GCM_DATA "/gcm-calibrate.ui", &error);
	if (retval == 0) {
		egg_warning ("failed to load ui: %s", error->message);
		g_error_free (error);
		return;
	}

	/* get screen */
	calibrate->priv->rr_screen = gnome_rr_screen_new (gdk_screen_get_default (), NULL, NULL, &error);
	if (calibrate->priv->rr_screen == NULL) {
		egg_warning ("failed to get rr screen: %s", error->message);
		g_error_free (error);
		return;
	}

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

	g_free (priv->filename_source);
	g_free (priv->filename_reference);
	g_free (priv->output_name);
	g_free (priv->basename);
	g_free (priv->manufacturer);
	g_free (priv->model);
	g_free (priv->description);
	g_main_loop_unref (priv->loop);
	g_object_unref (priv->builder);
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


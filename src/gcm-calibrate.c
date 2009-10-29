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
#include <math.h>
#include <string.h>
#include <gio/gio.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <vte/vte.h>

#include "gcm-calibrate.h"

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
};

enum {
	PROP_0,
	PROP_IS_LCD,
	PROP_IS_CRT,
	PROP_OUTPUT_NAME,
	PROP_LAST
};

G_DEFINE_TYPE (GcmCalibrate, gcm_calibrate, G_TYPE_OBJECT)

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

	/* show main UI */
	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "dialog_calibrate"));
	gtk_widget_show_all (widget);
	if (window != NULL)
		gdk_window_set_transient_for (gtk_widget_get_window (widget), gtk_widget_get_window (GTK_WIDGET(window)));

	/* setup GUI */
	gcm_calibrate_set_title (calibrate, _("Setup hardware"));
	gcm_calibrate_set_message (calibrate, _("Setting up hardware device for use..."));

	/* this wasn't previously set */
	if (!priv->is_lcd && !priv->is_crt) {
		dialog = gtk_message_dialog_new (GTK_WINDOW(widget), GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_QUESTION, GTK_BUTTONS_CANCEL,
						 _("Could not auto-detect CRT or LCD"));
		gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), _("Please indicate if the screen you are trying to profile is a CRT (old type) or a LCD (digital flat panel)."));
		gtk_dialog_add_button (GTK_DIALOG (dialog), _("LCD"), GTK_RESPONSE_YES);
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
out:
	return ret;
}

/**
 * gcm_calibrate_task_neutralise:
 **/
static gboolean
gcm_calibrate_task_neutralise (GcmCalibrate *calibrate, GError **error)
{
	gboolean ret = TRUE;
	GcmCalibratePrivate *priv = calibrate->priv;
	gchar type;
	gchar *cmd = NULL;
	gchar **argc = NULL;
	GtkWidget *widget;
	GtkWidget *dialog;
	GtkResponseType response;

	/* match up the output name with the device number defined by dispcal */
	priv->display = gcm_calibrate_get_display (priv->output_name, error);
	if (priv->display == G_MAXUINT)
		goto out;

	/* get l-cd or c-rt */
	type = gcm_calibrate_get_display_type (calibrate);

	/* TODO: choose a better filename, maybe based on the monitor serial number */
	priv->basename = g_strdup ("basename");

	/* setup the command */
	cmd = g_strdup_printf ("dispcal -v -ql -m -d%i -y%c %s", priv->display, type, priv->basename);
	argc = g_strsplit (cmd, " ", -1);
	egg_debug ("running %s", cmd);

	/* start up the command */
	priv->child_pid = vte_terminal_fork_command (VTE_TERMINAL(priv->terminal), argc[0], &argc[1], NULL, "/tmp", FALSE, FALSE, FALSE);

	/* ask user to attach device */
	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "dialog_calibrate"));
	dialog = gtk_message_dialog_new (GTK_WINDOW(widget), GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_INFO, GTK_BUTTONS_OK_CANCEL,
					 _("Please attach device"));
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), _("Please attach the hardware device to the center of the screen on the grey square."));
	response = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
	if (response != GTK_RESPONSE_OK) {
		vte_terminal_feed_child (VTE_TERMINAL(priv->terminal), "Q", 1);
		if (error != NULL)
			*error = g_error_new (1, 0, "user did not attach hardware device");
		ret = FALSE;
		goto out;
	}

	/* send the terminal an okay */
	vte_terminal_feed_child (VTE_TERMINAL(priv->terminal), " ", 1);

	/* setup GUI */
	gcm_calibrate_set_title (calibrate, _("Resetting screen to neutral state"));
	gcm_calibrate_set_message (calibrate, _("This brings the screen to a neutral state by sending colored and gray patches to your screen and measuring them with the hardware device."));

	/* wait until finished */
	g_main_loop_run (priv->loop);
out:
	g_strfreev (argc);
	g_free (cmd);
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
	gchar type;
	gchar *cmd = NULL;
	gchar **argc = NULL;

	/* get l-cd or c-rt */
	type = gcm_calibrate_get_display_type (calibrate);

	/* setup the command */
	cmd = g_strdup_printf ("targen -v -d3 -f250 %s", priv->basename);
	argc = g_strsplit (cmd, " ", -1);
	egg_debug ("running %s", cmd);

	/* start up the command */
	priv->child_pid = vte_terminal_fork_command (VTE_TERMINAL(priv->terminal), argc[0], &argc[1], NULL, "/tmp", FALSE, FALSE, FALSE);
	g_timeout_add_seconds (3, (GSourceFunc) gcm_calibrate_timeout_cb, calibrate);

	/* setup GUI */
	gcm_calibrate_set_title (calibrate, _("Generating the patches"));
	gcm_calibrate_set_message (calibrate, _("Generating the patches that will be measured with the hardware device."));

	/* wait until finished */
	g_main_loop_run (priv->loop);

	g_strfreev (argc);
	g_free (cmd);
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
	gchar *cmd = NULL;
	gchar **argc = NULL;

	/* get l-cd or c-rt */
	type = gcm_calibrate_get_display_type (calibrate);

	/* setup the command */
	cmd = g_strdup_printf ("dispread -v -d%i -y%c -k %s.cal %s", priv->display, type, priv->basename, priv->basename);
	argc = g_strsplit (cmd, " ", -1);
	egg_debug ("running %s", cmd);

	/* start up the command */
	priv->child_pid = vte_terminal_fork_command (VTE_TERMINAL(priv->terminal), argc[0], &argc[1], NULL, "/tmp", FALSE, FALSE, FALSE);

	/* setup GUI */
	gcm_calibrate_set_title (calibrate, _("Drawing the patches"));
	gcm_calibrate_set_message (calibrate, _("Drawing the generated patches to the screen, which will then be measured by the hardware device."));

	/* wait until finished */
	g_main_loop_run (priv->loop);

	g_strfreev (argc);
	g_free (cmd);
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
	gchar type;
	gchar *cmd = NULL;
	gchar **argc = NULL;

	/* get l-cd or c-rt */
	type = gcm_calibrate_get_display_type (calibrate);

	/* setup the command */
	cmd = g_strdup_printf ("colprof -A \"LG\" -M \"LG Flatpanel\" -D \"October 29 2009\" -q m -as %s", priv->basename);

	argc = g_strsplit (cmd, " ", -1);
	egg_debug ("running %s", cmd);

	/* start up the command */
	priv->child_pid = vte_terminal_fork_command (VTE_TERMINAL(priv->terminal), argc[0], &argc[1], NULL, "/tmp", FALSE, FALSE, FALSE);

	/* setup GUI */
	gcm_calibrate_set_title (calibrate, _("Generating the profile"));
	gcm_calibrate_set_message (calibrate, _("Generating the ICC color profile that can be used with this screen."));

	/* wait until finished */
	g_main_loop_run (priv->loop);

	g_strfreev (argc);
	g_free (cmd);
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
 * gcm_calibrate_exit_cb:
 **/
static void
gcm_calibrate_exit_cb (VteTerminal *terminal, GcmCalibrate *calibrate)
{
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
	GcmCalibratePrivate *priv = calibrate->priv;

	/* guess based on the output name */
	if (strstr (priv->output_name, "DVI") != NULL ||
	    strstr (priv->output_name, "LVDS") != NULL) {
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

	calibrate->priv = GCM_CALIBRATE_GET_PRIVATE (calibrate);
	calibrate->priv->output_name = NULL;
	calibrate->priv->basename = NULL;
	calibrate->priv->loop = g_main_loop_new (NULL, FALSE);

	/* get UI */
	calibrate->priv->builder = gtk_builder_new ();
	retval = gtk_builder_add_from_file (calibrate->priv->builder, GCM_DATA "/gcm-import.ui", &error);
	if (retval == 0) {
		egg_warning ("failed to load ui: %s", error->message);
		g_error_free (error);
	}

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

	/* wait until finished */
	if (g_main_loop_is_running (priv->loop))
		g_main_loop_quit (priv->loop);

	/* hide window */
	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "dialog_calibrate"));
	gtk_widget_hide (widget);

	g_free (priv->output_name);
	g_free (priv->basename);
	g_main_loop_unref (priv->loop);
	g_object_unref (priv->builder);

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


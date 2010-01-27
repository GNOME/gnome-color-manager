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

/**
 * SECTION:gcm-calibrate-argyll
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
#include <gconf/gconf-client.h>

#include "gcm-calibrate-argyll.h"
#include "gcm-utils.h"
#include "gcm-screen.h"

#include "egg-debug.h"

static void     gcm_calibrate_argyll_finalize	(GObject     *object);

#define GCM_CALIBRATE_ARGYLL_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GCM_TYPE_CALIBRATE_ARGYLL, GcmCalibrateArgyllPrivate))

typedef enum {
	GCM_CALIBRATE_ARGYLL_PRECISION_SHORT,
	GCM_CALIBRATE_ARGYLL_PRECISION_NORMAL,
	GCM_CALIBRATE_ARGYLL_PRECISION_LONG,
	GCM_CALIBRATE_ARGYLL_PRECISION_LAST
} GcmCalibrateArgyllPrecision;

typedef enum {
	GCM_CALIBRATE_ARGYLL_STATE_IDLE,
	GCM_CALIBRATE_ARGYLL_STATE_WAITING_FOR_STDIN,
	GCM_CALIBRATE_ARGYLL_STATE_WAITING_FOR_LOOP,
	GCM_CALIBRATE_ARGYLL_STATE_RUNNING,
	GCM_CALIBRATE_ARGYLL_STATE_LAST
} GcmCalibrateArgyllState;

/**
 * GcmCalibrateArgyllPrivate:
 *
 * Private #GcmCalibrateArgyll data
 **/
struct _GcmCalibrateArgyllPrivate
{
	guint				 display;
	GConfClient			*gconf_client;
	GcmCalibrateArgyllPrecision	 precision;
	GMainLoop			*loop;
	GtkWidget			*terminal;
	GtkBuilder			*builder;
	pid_t				 child_pid;
	GtkResponseType			 response;
	GcmScreen			*screen;
	glong				 vte_previous_row;
	glong				 vte_previous_col;
	GPtrArray			*cached_titles;
	GPtrArray			*cached_messages;
	gboolean			 already_on_window;
	GcmCalibrateArgyllState		 state;
};

enum {
	PROP_0,
	PROP_LAST
};

G_DEFINE_TYPE (GcmCalibrateArgyll, gcm_calibrate_argyll, GCM_TYPE_CALIBRATE)

/* assume this is writable by users */
#define GCM_CALIBRATE_ARGYLL_TEMP_DIR		"/tmp"

/**
 * gcm_calibrate_argyll_precision_from_string:
 **/
static GcmCalibrateArgyllPrecision
gcm_calibrate_argyll_precision_from_string (const gchar *string)
{
	if (g_strcmp0 (string, "short") == 0)
		return GCM_CALIBRATE_ARGYLL_PRECISION_SHORT;
	if (g_strcmp0 (string, "normal") == 0)
		return GCM_CALIBRATE_ARGYLL_PRECISION_NORMAL;
	if (g_strcmp0 (string, "long") == 0)
		return GCM_CALIBRATE_ARGYLL_PRECISION_LONG;
	egg_warning ("failed to convert to precision: %s", string);
	return GCM_CALIBRATE_ARGYLL_PRECISION_NORMAL;
}

/**
 * gcm_calibrate_argyll_precision_to_quality_arg:
 **/
static const gchar *
gcm_calibrate_argyll_precision_to_quality_arg (GcmCalibrateArgyllPrecision precision)
{
	if (precision == GCM_CALIBRATE_ARGYLL_PRECISION_SHORT)
		return "-ql";
	if (precision == GCM_CALIBRATE_ARGYLL_PRECISION_NORMAL)
		return "-qm";
	if (precision == GCM_CALIBRATE_ARGYLL_PRECISION_LONG)
		return "-qh";
	return "-qm";
}

/**
 * gcm_calibrate_argyll_precision_to_patches_arg:
 **/
static const gchar *
gcm_calibrate_argyll_precision_to_patches_arg (GcmCalibrateArgyllPrecision precision)
{
	if (precision == GCM_CALIBRATE_ARGYLL_PRECISION_SHORT)
		return "-f100";
	if (precision == GCM_CALIBRATE_ARGYLL_PRECISION_NORMAL)
		return "-f250";
	if (precision == GCM_CALIBRATE_ARGYLL_PRECISION_LONG)
		return "-f500";
	return "-f250";
}

/**
 * gcm_calibrate_argyll_get_display:
 **/
static guint
gcm_calibrate_argyll_get_display (const gchar *output_name, GError **error)
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
		if (g_strstr_len (split[i], -1, "XRandR 1.2 is faulty") != NULL) {
			ret = FALSE;
			g_set_error_literal (error, 1, 0, "failed to match display as RandR is faulty");
			goto out;
		}
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
		ret = FALSE;
		g_set_error_literal (error, 1, 0, "failed to match display");
		goto out;
	}
out:
	g_free (data);
	g_strfreev (split);
	return display;
}

/**
 * gcm_calibrate_argyll_get_display_type:
 **/
static gchar
gcm_calibrate_argyll_get_display_type (GcmCalibrateArgyll *calibrate_argyll)
{
	gboolean is_lcd;
	gboolean is_crt;

	g_object_get (calibrate_argyll,
		      "is-lcd", &is_lcd,
		      "is-crt", &is_crt,
		      NULL);

	if (is_lcd)
		return 'l';
	if (is_crt)
		return 'c';
	return '\0';
}

/**
 * gcm_calibrate_argyll_set_dialog:
 **/
static void
gcm_calibrate_argyll_set_dialog (GcmCalibrateArgyll *calibrate_argyll, const gchar *title, const gchar *message)
{
	GcmCalibrateArgyllPrivate *priv = calibrate_argyll->priv;
	GtkWidget *widget;
	gchar *text;

	/* save in case we need to reuse */
	g_ptr_array_add (priv->cached_titles, g_strdup (title));
	g_ptr_array_add (priv->cached_messages, g_strdup (message));

	/* set the text */
	text = g_strdup_printf ("<big><b>%s</b></big>", title);
	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "label_title"));
	gtk_label_set_markup (GTK_LABEL(widget), text);
	g_free (text);

	/* set the text */
	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "label_message"));
	gtk_label_set_markup (GTK_LABEL(widget), message);
}

/**
 * gcm_calibrate_argyll_pop_dialog:
 **/
static void
gcm_calibrate_argyll_pop_dialog (GcmCalibrateArgyll *calibrate_argyll)
{
	GcmCalibrateArgyllPrivate *priv = calibrate_argyll->priv;
	GtkWidget *widget;
	gchar *text;
	const gchar *title;
	const gchar *message;
	guint len;

	/* save in case we need to reuse */
	len = priv->cached_titles->len;
	if (len < 2) {
		egg_warning ("cannot pop dialog as nothing to recover");
		return;
	}
	title = g_ptr_array_index (priv->cached_titles, len-2);
	message = g_ptr_array_index (priv->cached_messages, len-2);

	/* set the text */
	text = g_strdup_printf ("<big><b>%s</b></big>", title);
	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "label_title"));
	gtk_label_set_markup (GTK_LABEL(widget), text);
	g_free (text);

	/* set the text */
	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "label_message"));
	gtk_label_set_markup (GTK_LABEL(widget), message);

	/* remove from the stack */
	g_ptr_array_remove_index (priv->cached_titles, len-1);
	g_ptr_array_remove_index (priv->cached_messages, len-1);
}

/**
 * gcm_calibrate_argyll_debug_argv:
 **/
static void
gcm_calibrate_argyll_debug_argv (const gchar *program, gchar **argv)
{
	gchar *join;
	join = g_strjoinv ("  ", argv);
	egg_debug ("running %s  %s", program, join);
	g_free (join);
}

/**
 * gcm_calibrate_argyll_get_tool_filename:
 **/
static gchar *
gcm_calibrate_argyll_get_tool_filename (const gchar *command, GError **error)
{
	gboolean ret;
	gchar *filename;

	/* try the original argyllcms filename installed in /usr/local/bin */
	filename = g_strdup_printf ("/usr/local/bin/%s", command);
	ret = g_file_test (filename, G_FILE_TEST_EXISTS);
	if (ret)
		goto out;

	/* try the debian filename installed in /usr/bin */
	g_free (filename);
	filename = g_strdup_printf ("/usr/bin/argyll-%s", command);
	ret = g_file_test (filename, G_FILE_TEST_EXISTS);
	if (ret)
		goto out;

	/* try the original argyllcms filename installed in /usr/bin */
	g_free (filename);
	filename = g_strdup_printf ("/usr/bin/%s", command);
	ret = g_file_test (filename, G_FILE_TEST_EXISTS);
	if (ret)
		goto out;

	/* eek */
	g_free (filename);
	filename = NULL;
	g_set_error (error, 1, 0, "failed to get filename for %s", command);
out:
	return filename;
}

/**
 * gcm_calibrate_argyll_display_neutralise:
 **/
static gboolean
gcm_calibrate_argyll_display_neutralise (GcmCalibrateArgyll *calibrate_argyll, GError **error)
{
	gboolean ret = TRUE;
	GcmCalibrateArgyllPrivate *priv = calibrate_argyll->priv;
	gchar type;
	gchar *command = NULL;
	gchar **argv = NULL;
	GtkWidget *widget;
	GnomeRROutput *output;
	GPtrArray *array = NULL;
	gint x, y;
	gchar *basename = NULL;
	gchar *output_name = NULL;
	const gchar *title;
	const gchar *message;

	/* get shared data */
	g_object_get (calibrate_argyll,
		      "basename", &basename,
		      "output-name", &output_name,
		      NULL);

	/* get correct name of the command */
	command = gcm_calibrate_argyll_get_tool_filename ("dispcal", error);
	if (command == NULL) {
		ret = FALSE;
		goto out;
	}

	/* match up the output name with the device number defined by dispcal */
	priv->display = gcm_calibrate_argyll_get_display (output_name, error);
	if (priv->display == G_MAXUINT) {
		ret = FALSE;
		goto out;
	}

	/* get the device */
	output = gcm_screen_get_output_by_name (priv->screen, output_name, error);
	if (output == NULL) {
		ret = FALSE;
		goto out;
	}

	/* get l-cd or c-rt */
	type = gcm_calibrate_argyll_get_display_type (calibrate_argyll);

	/* move the dialog out of the way, so the grey square doesn't cover it */
	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "dialog_calibrate"));
	gtk_window_get_position (GTK_WINDOW(widget), &x, &y);
	egg_debug ("currently at %i,%i, moving left", x, y);
	gtk_window_move (GTK_WINDOW(widget), 10, y);

	/* TRANSLATORS: title, default paramters needed to calibrate_argyll */
	title = _("Getting default parameters");

	/* TRANSLATORS: dialog message */
	message = _("This pre-calibrates the screen by sending colored and gray patches to your screen and measuring them with the hardware device.");

	/* push new messages into the UI */
	gcm_calibrate_argyll_set_dialog (calibrate_argyll, title, message);

	/* argument array */
	array = g_ptr_array_new_with_free_func (g_free);

	/* setup the command */
	g_ptr_array_add (array, g_strdup ("-v9"));
	g_ptr_array_add (array, g_strdup ("-ql"));
	g_ptr_array_add (array, g_strdup ("-m"));
	g_ptr_array_add (array, g_strdup_printf ("-d%i", priv->display));
	g_ptr_array_add (array, g_strdup_printf ("-y%c", type));
//	g_ptr_array_add (array, g_strdup ("-p 0.8,0.5,1.0"));
	g_ptr_array_add (array, g_strdup (basename));
	argv = gcm_utils_ptr_array_to_strv (array);
	gcm_calibrate_argyll_debug_argv (command, argv);

	/* start up the command */
	priv->state = GCM_CALIBRATE_ARGYLL_STATE_RUNNING;
	vte_terminal_reset (VTE_TERMINAL(priv->terminal), TRUE, FALSE);
	priv->child_pid = vte_terminal_fork_command (VTE_TERMINAL(priv->terminal), command, argv, NULL, GCM_CALIBRATE_ARGYLL_TEMP_DIR, FALSE, FALSE, FALSE);

	/* wait until finished */
	g_main_loop_run (priv->loop);

	/* get result */
	if (priv->response == GTK_RESPONSE_CANCEL) {
		g_set_error_literal (error, 1, 0, "calibration was cancelled");
		ret = FALSE;
		goto out;
	}
	if (priv->response == GTK_RESPONSE_REJECT) {
		g_set_error_literal (error, 1, 0, "command failed to run successfully");
		ret = FALSE;
		goto out;
	}
out:
	if (array != NULL)
		g_ptr_array_unref (array);
	g_free (basename);
	g_free (output_name);
	g_free (command);
	g_strfreev (argv);
	return ret;
}

/**
 * gcm_calibrate_argyll_display_generate_patches:
 **/
static gboolean
gcm_calibrate_argyll_display_generate_patches (GcmCalibrateArgyll *calibrate_argyll, GError **error)
{
	gboolean ret = TRUE;
	GcmCalibrateArgyllPrivate *priv = calibrate_argyll->priv;
	gchar *command = NULL;
	gchar **argv = NULL;
	GPtrArray *array = NULL;
	gchar *basename = NULL;
	const gchar *title;
	const gchar *message;

	/* get shared data */
	g_object_get (calibrate_argyll,
		      "basename", &basename,
		      NULL);

	/* get correct name of the command */
	command = gcm_calibrate_argyll_get_tool_filename ("targen", error);
	if (command == NULL) {
		ret = FALSE;
		goto out;
	}

	/* TRANSLATORS: title, patches are specific colours used in calibration */
	title = _("Generating the patches");
	/* TRANSLATORS: dialog message */
	message = _("Generating the patches that will be measured with the hardware device.");

	/* push new messages into the UI */
	gcm_calibrate_argyll_set_dialog (calibrate_argyll, title, message);

	/* argument array */
	array = g_ptr_array_new_with_free_func (g_free);

	/* setup the command */
	g_ptr_array_add (array, g_strdup ("-v9"));
	g_ptr_array_add (array, g_strdup ("-d3"));
	g_ptr_array_add (array, g_strdup (gcm_calibrate_argyll_precision_to_patches_arg (priv->precision)));
	g_ptr_array_add (array, g_strdup (basename));
	argv = gcm_utils_ptr_array_to_strv (array);
	gcm_calibrate_argyll_debug_argv (command, argv);

	/* start up the command */
	priv->state = GCM_CALIBRATE_ARGYLL_STATE_RUNNING;
	vte_terminal_reset (VTE_TERMINAL(priv->terminal), TRUE, FALSE);
	priv->child_pid = vte_terminal_fork_command (VTE_TERMINAL(priv->terminal), command, argv, NULL, GCM_CALIBRATE_ARGYLL_TEMP_DIR, FALSE, FALSE, FALSE);

	/* wait until finished */
	g_main_loop_run (priv->loop);

	/* get result */
	if (priv->response == GTK_RESPONSE_CANCEL) {
		g_set_error_literal (error, 1, 0, "calibration was cancelled");
		ret = FALSE;
		goto out;
	}
	if (priv->response == GTK_RESPONSE_REJECT) {
		g_set_error_literal (error, 1, 0, "command failed to run successfully");
		ret = FALSE;
		goto out;
	}
out:
	if (array != NULL)
		g_ptr_array_unref (array);
	g_free (basename);
	g_free (command);
	g_strfreev (argv);
	return ret;
}

/**
 * gcm_calibrate_argyll_display_draw_and_measure:
 **/
static gboolean
gcm_calibrate_argyll_display_draw_and_measure (GcmCalibrateArgyll *calibrate_argyll, GError **error)
{
	gboolean ret = TRUE;
	GcmCalibrateArgyllPrivate *priv = calibrate_argyll->priv;
	gchar type;
	gchar *command = NULL;
	gchar **argv = NULL;
	GPtrArray *array = NULL;
	gchar *basename = NULL;
	const gchar *title;
	const gchar *message;

	/* get shared data */
	g_object_get (calibrate_argyll,
		      "basename", &basename,
		      NULL);

	/* get correct name of the command */
	command = gcm_calibrate_argyll_get_tool_filename ("dispread", error);
	if (command == NULL) {
		ret = FALSE;
		goto out;
	}

	/* get l-cd or c-rt */
	type = gcm_calibrate_argyll_get_display_type (calibrate_argyll);

	/* TRANSLATORS: title, drawing means painting to the screen */
	title = _("Drawing the patches");
	/* TRANSLATORS: dialog message */
	message = _("Drawing the generated patches to the screen, which will then be measured by the hardware device.");

	/* push new messages into the UI */
	gcm_calibrate_argyll_set_dialog (calibrate_argyll, title, message);

	/* argument array */
	array = g_ptr_array_new_with_free_func (g_free);

	/* setup the command */
	g_ptr_array_add (array, g_strdup ("-v9"));
	g_ptr_array_add (array, g_strdup_printf ("-d%i", priv->display));
	g_ptr_array_add (array, g_strdup_printf ("-y%c", type));
	g_ptr_array_add (array, g_strdup ("-k"));
	g_ptr_array_add (array, g_strdup_printf ("%s.cal", basename));
	g_ptr_array_add (array, g_strdup (basename));
	argv = gcm_utils_ptr_array_to_strv (array);
	gcm_calibrate_argyll_debug_argv (command, argv);

	/* start up the command */
	priv->state = GCM_CALIBRATE_ARGYLL_STATE_RUNNING;
	vte_terminal_reset (VTE_TERMINAL(priv->terminal), TRUE, FALSE);
	priv->child_pid = vte_terminal_fork_command (VTE_TERMINAL(priv->terminal), command, argv, NULL, GCM_CALIBRATE_ARGYLL_TEMP_DIR, FALSE, FALSE, FALSE);

	/* wait until finished */
	g_main_loop_run (priv->loop);

	/* get result */
	if (priv->response == GTK_RESPONSE_CANCEL) {
		g_set_error_literal (error, 1, 0, "calibration was cancelled");
		ret = FALSE;
		goto out;
	}
	if (priv->response == GTK_RESPONSE_REJECT) {
		g_set_error_literal (error, 1, 0, "command failed to run successfully");
		ret = FALSE;
		goto out;
	}
out:
	if (array != NULL)
		g_ptr_array_unref (array);
	g_free (basename);
	g_free (command);
	g_strfreev (argv);
	return ret;
}

/**
 * gcm_calibrate_argyll_display_generate_profile:
 **/
static gboolean
gcm_calibrate_argyll_display_generate_profile (GcmCalibrateArgyll *calibrate_argyll, GError **error)
{
	gboolean ret = TRUE;
	GcmCalibrateArgyllPrivate *priv = calibrate_argyll->priv;
	gchar **argv = NULL;
	GDate *date = NULL;
	gchar *copyright = NULL;
	gchar *description_new = NULL;
	gchar *command = NULL;
	gchar *basename = NULL;
	gchar *description = NULL;
	gchar *manufacturer = NULL;
	gchar *device = NULL;
	gchar *model = NULL;
	GPtrArray *array = NULL;
	GtkWidget *widget;
	const gchar *title;
	const gchar *message;

	/* get shared data */
	g_object_get (calibrate_argyll,
		      "basename", &basename,
		      "description", &description,
		      "manufacturer", &manufacturer,
		      "model", &model,
		      "device", &device,
		      NULL);

	/* get correct name of the command */
	command = gcm_calibrate_argyll_get_tool_filename ("colprof", error);
	if (command == NULL) {
		ret = FALSE;
		goto out;
	}

	/* create date and set it to now */
	date = g_date_new ();
	g_date_set_time_t (date, time (NULL));

        /* get description */
	description_new = g_strdup_printf ("%s, %s (%04i-%02i-%02i)", device, description, date->year, date->month, date->day);

	/* TRANSLATORS: this is the copyright string, where it might be "Copyright (c) 2009 Edward Scissorhands" */
	copyright = g_strdup_printf ("%s %04i %s", _("Copyright (c)"), date->year, g_get_real_name ());

	/* no need for an okay button */
	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "button_ok"));
	gtk_widget_hide (widget);

	/* TRANSLATORS: title, a profile is a ICC file */
	title = _("Generating the profile");
	/* TRANSLATORS: dialog message */
	message = _("Generating the ICC color profile that can be used with this screen.");

	/* push new messages into the UI */
	gcm_calibrate_argyll_set_dialog (calibrate_argyll, title, message);

	/* argument array */
	array = g_ptr_array_new_with_free_func (g_free);

	/* setup the command */
	g_ptr_array_add (array, g_strdup ("-v9"));
	g_ptr_array_add (array, g_strdup_printf ("-A%s", manufacturer));
	g_ptr_array_add (array, g_strdup_printf ("-M%s", model));
	g_ptr_array_add (array, g_strdup_printf ("-D%s", description_new));
	g_ptr_array_add (array, g_strdup_printf ("-C%s", copyright));
	g_ptr_array_add (array, g_strdup (gcm_calibrate_argyll_precision_to_quality_arg (priv->precision)));
	g_ptr_array_add (array, g_strdup ("-as"));
	g_ptr_array_add (array, g_strdup (basename));
	argv = gcm_utils_ptr_array_to_strv (array);
	gcm_calibrate_argyll_debug_argv (command, argv);

	/* start up the command */
	priv->state = GCM_CALIBRATE_ARGYLL_STATE_RUNNING;
	vte_terminal_reset (VTE_TERMINAL(priv->terminal), TRUE, FALSE);
	priv->child_pid = vte_terminal_fork_command (VTE_TERMINAL(priv->terminal), command, argv, NULL, GCM_CALIBRATE_ARGYLL_TEMP_DIR, FALSE, FALSE, FALSE);

	/* wait until finished */
	g_main_loop_run (priv->loop);

	/* get result */
	if (priv->response == GTK_RESPONSE_CANCEL) {
		g_set_error_literal (error, 1, 0, "calibration was cancelled");
		ret = FALSE;
		goto out;
	}
	if (priv->response == GTK_RESPONSE_REJECT) {
		g_set_error_literal (error, 1, 0, "command failed to run successfully");
		ret = FALSE;
		goto out;
	}
out:
	if (array != NULL)
		g_ptr_array_unref (array);
	if (date != NULL)
		g_date_free (date);
	g_free (basename);
	g_free (command);
	g_free (manufacturer);
	g_free (model);
	g_free (description);
	g_free (description_new);
	g_free (device);
	g_free (copyright);
	g_strfreev (argv);
	return ret;
}

/**
 * gcm_calibrate_argyll_device_setup:
 **/
static gboolean
gcm_calibrate_argyll_device_setup (GcmCalibrateArgyll *calibrate_argyll, GError **error)
{
	gboolean ret = TRUE;
	GString *string = NULL;
	GcmCalibrateArgyllPrivate *priv = calibrate_argyll->priv;
	const gchar *title;

	string = g_string_new ("");

	/* TRANSLATORS: title, we're setting up the device ready for calibration */
	title = _("Setting up device");

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

	/* push new messages into the UI */
	gcm_calibrate_argyll_set_dialog (calibrate_argyll, title, string->str);

	/* set state */
	priv->state = GCM_CALIBRATE_ARGYLL_STATE_WAITING_FOR_LOOP;

	/* wait until finished */
	g_main_loop_run (priv->loop);

	/* get result */
	if (priv->response != GTK_RESPONSE_OK) {
		g_set_error_literal (error, 1, 0, "user did not follow calibration steps");
		ret = FALSE;
		goto out;
	}
out:
	if (string != NULL)
		g_string_free (string, TRUE);
	return ret;
}

/**
 * gcm_calibrate_argyll_device_copy:
 **/
static gboolean
gcm_calibrate_argyll_device_copy (GcmCalibrateArgyll *calibrate_argyll, GError **error)
{
	gboolean ret;
	gchar *device = NULL;
	gchar *it8cht = NULL;
	gchar *it8ref = NULL;
	gchar *filename = NULL;
	gchar *basename = NULL;
	gchar *filename_source = NULL;
	gchar *filename_reference = NULL;
	const gchar *title;
	const gchar *message;

	/* get shared data */
	g_object_get (calibrate_argyll,
		      "basename", &basename,
		      "filename-source", &filename_source,
		      "filename-reference", &filename_reference,
		      NULL);

	/* TRANSLATORS: title, a profile is a ICC file */
	title = _("Copying files");
	/* TRANSLATORS: dialog message */
	message = _("Copying source image, chart data and CIE reference values.");

	/* push new messages into the UI */
	gcm_calibrate_argyll_set_dialog (calibrate_argyll, title, message);

	/* build filenames */
	filename = g_strdup_printf ("%s.tif", basename);
	device = g_build_filename (GCM_CALIBRATE_ARGYLL_TEMP_DIR, filename, NULL);
	it8cht = g_build_filename (GCM_CALIBRATE_ARGYLL_TEMP_DIR, "it8.cht", NULL);
	it8ref = g_build_filename (GCM_CALIBRATE_ARGYLL_TEMP_DIR, "it8ref.txt", NULL);

	/* copy all files to /tmp as argyllcms doesn't cope well with paths */
	ret = gcm_utils_mkdir_and_copy ("/usr/share/color/argyll/ref/it8.cht", it8cht, error);
	if (!ret)
		goto out;
	ret = gcm_utils_mkdir_and_copy (filename_source, device, error);
	if (!ret)
		goto out;
	ret = gcm_utils_mkdir_and_copy (filename_reference, it8ref, error);
	if (!ret)
		goto out;
out:
	g_free (basename);
	g_free (filename);
	g_free (filename_source);
	g_free (filename_reference);
	g_free (device);
	g_free (it8cht);
	g_free (it8ref);
	return ret;
}

/**
 * gcm_calibrate_argyll_device_measure:
 **/
static gboolean
gcm_calibrate_argyll_device_measure (GcmCalibrateArgyll *calibrate_argyll, GError **error)
{
	gboolean ret = TRUE;
	GcmCalibrateArgyllPrivate *priv = calibrate_argyll->priv;
	gchar **argv = NULL;
	GPtrArray *array = NULL;
	gchar *filename = NULL;
	gchar *command = NULL;
	gchar *basename = NULL;
	const gchar *title;
	const gchar *message;

	/* get shared data */
	g_object_get (calibrate_argyll,
		      "basename", &basename,
		      NULL);

	/* TRANSLATORS: title, drawing means painting to the screen */
	title = _("Measuring the patches");
	/* TRANSLATORS: dialog message */
	message = _("Detecting the reference patches and measuring them.");

	/* push new messages into the UI */
	gcm_calibrate_argyll_set_dialog (calibrate_argyll, title, message);

	/* get correct name of the command */
	command = gcm_calibrate_argyll_get_tool_filename ("scanin", error);
	if (command == NULL) {
		ret = FALSE;
		goto out;
	}

	/* argument array */
	array = g_ptr_array_new_with_free_func (g_free);
	filename = g_strdup_printf ("%s.tif", basename);

	/* setup the command */
	g_ptr_array_add (array, g_strdup ("-v9"));
	g_ptr_array_add (array, g_strdup (filename));
	g_ptr_array_add (array, g_strdup ("it8.cht"));
	g_ptr_array_add (array, g_strdup ("it8ref.txt"));
	argv = gcm_utils_ptr_array_to_strv (array);
	gcm_calibrate_argyll_debug_argv (command, argv);

	/* start up the command */
	priv->state = GCM_CALIBRATE_ARGYLL_STATE_RUNNING;
	vte_terminal_reset (VTE_TERMINAL(priv->terminal), TRUE, FALSE);
	priv->child_pid = vte_terminal_fork_command (VTE_TERMINAL(priv->terminal), command, argv, NULL, GCM_CALIBRATE_ARGYLL_TEMP_DIR, FALSE, FALSE, FALSE);

	/* wait until finished */
	g_main_loop_run (priv->loop);

	/* get result */
	if (priv->response == GTK_RESPONSE_CANCEL) {
		g_set_error_literal (error, 1, 0, "calibration was cancelled");
		ret = FALSE;
		goto out;
	}
	if (priv->response == GTK_RESPONSE_REJECT) {
		g_set_error_literal (error, 1, 0, "command failed to run successfully");
		ret = FALSE;
		goto out;
	}
out:
	g_free (filename);
	g_free (command);
	g_free (basename);
	if (array != NULL)
		g_ptr_array_unref (array);
	g_strfreev (argv);
	return ret;
}

/**
 * gcm_calibrate_argyll_device_generate_profile:
 **/
static gboolean
gcm_calibrate_argyll_device_generate_profile (GcmCalibrateArgyll *calibrate_argyll, GError **error)
{
	gboolean ret = TRUE;
	GcmCalibrateArgyllPrivate *priv = calibrate_argyll->priv;
	gchar **argv = NULL;
	GDate *date = NULL;
	gchar *description_tmp = NULL;
	gchar *description = NULL;
	gchar *copyright = NULL;
	GPtrArray *array = NULL;
	gchar *command = NULL;
	gchar *basename = NULL;
	gchar *manufacturer = NULL;
	gchar *model = NULL;
	gchar *device = NULL;
	const gchar *title;
	const gchar *message;

	/* get shared data */
	g_object_get (calibrate_argyll,
		      "basename", &basename,
		      "description", &description,
		      "manufacturer", &manufacturer,
		      "model", &model,
		      "device", &device,
		      NULL);

	/* get correct name of the command */
	command = gcm_calibrate_argyll_get_tool_filename ("colprof", error);
	if (command == NULL) {
		ret = FALSE;
		goto out;
	}

	/* create date and set it to now */
	date = g_date_new ();
	g_date_set_time_t (date, time (NULL));

        /* create a long description */
	description_tmp = g_strdup_printf ("%s, %s (%04i-%02i-%02i)", device, description, date->year, date->month, date->day);

	/* TRANSLATORS: this is the copyright string, where it might be "Copyright (c) 2009 Edward Scissorhands" */
	copyright = g_strdup_printf ("%s %04i %s", _("Copyright (c)"), date->year, g_get_real_name ());

	/* TRANSLATORS: title, a profile is a ICC file */
	title = _("Generating the profile");
	/* TRANSLATORS: dialog message */
	message = _("Generating the ICC color profile that can be used with this device.");

	/* push new messages into the UI */
	gcm_calibrate_argyll_set_dialog (calibrate_argyll, title, message);

	/* argument array */
	array = g_ptr_array_new_with_free_func (g_free);

	/* setup the command */
	g_ptr_array_add (array, g_strdup ("-v9"));
	g_ptr_array_add (array, g_strdup_printf ("-A%s", manufacturer));
	g_ptr_array_add (array, g_strdup_printf ("-M%s", model));
	g_ptr_array_add (array, g_strdup_printf ("-D%s", description_tmp));
	g_ptr_array_add (array, g_strdup_printf ("-C%s", copyright));
	g_ptr_array_add (array, g_strdup (gcm_calibrate_argyll_precision_to_quality_arg (priv->precision)));
//	g_ptr_array_add (array, g_strdup ("-as"));
	g_ptr_array_add (array, g_strdup (basename));
	argv = gcm_utils_ptr_array_to_strv (array);
	gcm_calibrate_argyll_debug_argv (command, argv);

	/* start up the command */
	priv->state = GCM_CALIBRATE_ARGYLL_STATE_RUNNING;
	vte_terminal_reset (VTE_TERMINAL(priv->terminal), TRUE, FALSE);
	priv->child_pid = vte_terminal_fork_command (VTE_TERMINAL(priv->terminal), command, argv, NULL, GCM_CALIBRATE_ARGYLL_TEMP_DIR, FALSE, FALSE, FALSE);

	/* wait until finished */
	g_main_loop_run (priv->loop);

	/* get result */
	if (priv->response == GTK_RESPONSE_CANCEL) {
		g_set_error_literal (error, 1, 0, "calibration was cancelled");
		ret = FALSE;
		goto out;
	}
	if (priv->response == GTK_RESPONSE_REJECT) {
		g_set_error_literal (error, 1, 0, "command failed to run successfully");
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
	g_free (basename);
	g_free (manufacturer);
	g_free (model);
	g_free (description);
	g_free (command);
	g_free (device);
	g_strfreev (argv);
	return ret;
}

/**
 * gcm_calibrate_argyll_finish:
 **/
static gboolean
gcm_calibrate_argyll_finish (GcmCalibrateArgyll *calibrate_argyll, GError **error)
{
	gchar *filename = NULL;
	gchar *filename_tmp;
	guint i;
	gboolean ret;
	const gchar *exts[] = {"cal", "ti1", "ti3", "tif", NULL};
	const gchar *filenames[] = {"it8.cht", "it8ref.txt", NULL};
	gchar *basename = NULL;

	/* get shared data */
	g_object_get (calibrate_argyll,
		      "basename", &basename,
		      NULL);

	/* remove all the temp files */
	if (basename != NULL) {
		for (i=0; exts[i] != NULL; i++) {
			filename_tmp = g_strdup_printf ("%s/%s.%s", GCM_CALIBRATE_ARGYLL_TEMP_DIR, basename, exts[i]);
			ret = g_file_test (filename_tmp, G_FILE_TEST_IS_REGULAR);
			if (ret) {
				egg_debug ("removing %s", filename_tmp);
				g_unlink (filename_tmp);
			}
			g_free (filename_tmp);
		}
	}

	/* remove all the temp files */
	for (i=0; filenames[i] != NULL; i++) {
		filename_tmp = g_strdup_printf ("%s/%s", GCM_CALIBRATE_ARGYLL_TEMP_DIR, filenames[i]);
		ret = g_file_test (filename_tmp, G_FILE_TEST_IS_REGULAR);
		if (ret) {
			egg_debug ("removing %s", filename_tmp);
			g_unlink (filename_tmp);
		}
		g_free (filename_tmp);
	}

	/* we can't have finished with success */
	if (basename == NULL) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0, "profile was not generated");
		goto out;
	}

	/* get the finished icc file */
	filename = g_strdup_printf ("%s/%s.icc", GCM_CALIBRATE_ARGYLL_TEMP_DIR, basename);

	/* we never finished all the steps */
	if (!g_file_test (filename, G_FILE_TEST_EXISTS)) {
		ret = FALSE;
		g_set_error_literal (error, 1, 0, "could not find completed profile");
		goto out;
	}

	/* success */
	g_object_set (calibrate_argyll,
		      "filename-result", filename,
		      NULL);
	ret = TRUE;
out:
	g_free (basename);
	g_free (filename);
	return ret;
}

/**
 * gcm_calibrate_argyll_display:
 **/
static gboolean
gcm_calibrate_argyll_display (GcmCalibrate *calibrate, GtkWindow *window, GError **error)
{
	GtkWidget *widget;
	GcmCalibrateArgyll *calibrate_argyll = GCM_CALIBRATE_ARGYLL(calibrate);
	GcmCalibrateArgyllPrivate *priv = calibrate_argyll->priv;
	gboolean ret;
	const gchar *title;
	const gchar *message;

	/* show main UI */
	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "dialog_calibrate"));
	gtk_widget_show_all (widget);
	if (window != NULL)
		gdk_window_set_transient_for (gtk_widget_get_window (widget), gtk_widget_get_window (GTK_WIDGET(window)));

	/* TRANSLATORS: title, hardware refers to a calibration device */
	title = _("Set up display");

	/* TRANSLATORS: dialog message */
	message = _("Setting up display device for use...");

	/* push new messages into the UI */
	gcm_calibrate_argyll_set_dialog (calibrate_argyll, title, message);

	/* step 1 */
	ret = gcm_calibrate_argyll_display_neutralise (calibrate_argyll, error);
	if (!ret)
		goto out;

	/* step 2 */
	ret = gcm_calibrate_argyll_display_generate_patches (calibrate_argyll, error);
	if (!ret)
		goto out;

	/* step 3 */
	ret = gcm_calibrate_argyll_display_draw_and_measure (calibrate_argyll, error);
	if (!ret)
		goto out;

	/* step 4 */
	ret = gcm_calibrate_argyll_display_generate_profile (calibrate_argyll, error);
	if (!ret)
		goto out;

	/* step 5 */
	ret = gcm_calibrate_argyll_finish (calibrate_argyll, error);
	if (!ret)
		goto out;
out:
	return ret;
}

/**
 * gcm_calibrate_argyll_device:
 **/
static gboolean
gcm_calibrate_argyll_device (GcmCalibrate *calibrate, GtkWindow *window, GError **error)
{
	GtkWidget *widget;
	GcmCalibrateArgyll *calibrate_argyll = GCM_CALIBRATE_ARGYLL(calibrate);
	GcmCalibrateArgyllPrivate *priv = calibrate_argyll->priv;
	gboolean ret;
	const gchar *title;
	const gchar *message;

	/* show main UI */
	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "dialog_calibrate"));
	gtk_widget_show_all (widget);
	if (window != NULL)
		gdk_window_set_transient_for (gtk_widget_get_window (widget), gtk_widget_get_window (GTK_WIDGET(window)));

	/* TRANSLATORS: title, hardware refers to a calibration device */
	title = _("Set up device");

	/* TRANSLATORS: dialog message */
	message = _("Setting up device for use...");

	/* push new messages into the UI */
	gcm_calibrate_argyll_set_dialog (calibrate_argyll, title, message);

	/* step 0 */
	ret = gcm_calibrate_argyll_device_setup (calibrate_argyll, error);
	if (!ret)
		goto out;

	/* step 1 */
	ret = gcm_calibrate_argyll_device_copy (calibrate_argyll, error);
	if (!ret)
		goto out;

	/* step 2 */
	ret = gcm_calibrate_argyll_device_measure (calibrate_argyll, error);
	if (!ret)
		goto out;

	/* step 3 */
	ret = gcm_calibrate_argyll_device_generate_profile (calibrate_argyll, error);
	if (!ret)
		goto out;

	/* step 4 */
	ret = gcm_calibrate_argyll_finish (calibrate_argyll, error);
	if (!ret)
		goto out;
out:
	return ret;
}

/**
 * gcm_calibrate_argyll_exit_cb:
 **/
static void
gcm_calibrate_argyll_exit_cb (VteTerminal *terminal, GcmCalibrateArgyll *calibrate_argyll)
{
	gint exit_status;
	GcmCalibrateArgyllPrivate *priv = calibrate_argyll->priv;

	/* get the child exit status */
	exit_status = vte_terminal_get_child_exit_status (terminal);
	egg_debug ("child exit-status is %i", exit_status);
	if (exit_status == 0)
		priv->response = GTK_RESPONSE_ACCEPT;
	else
		priv->response = GTK_RESPONSE_REJECT;

	priv->child_pid = -1;
	if (g_main_loop_is_running (priv->loop)) {
		priv->state = GCM_CALIBRATE_ARGYLL_STATE_IDLE;
		g_main_loop_quit (priv->loop);
	}
}

/**
 * gcm_calibrate_argyll_timeout_cb:
 **/
static gboolean
gcm_calibrate_argyll_timeout_cb (GcmCalibrateArgyll *calibrate_argyll)
{
	vte_terminal_feed_child (VTE_TERMINAL(calibrate_argyll->priv->terminal), " ", 1);
	return FALSE;
}

/**
 * gcm_calibrate_argyll_process_output_cmd:
 **/
static void
gcm_calibrate_argyll_process_output_cmd (GcmCalibrateArgyll *calibrate_argyll, const gchar *line)
{
	const gchar *title;
	const gchar *message;
	GcmCalibrateArgyllPrivate *priv = calibrate_argyll->priv;

	/* attach device */
	if (g_strcmp0 (line, "Place instrument on test window.") == 0) {
		egg_debug ("VTE: interaction required: %s", line);

		/* different tools assume the device is not on the screen */
		if (priv->already_on_window) {
			egg_debug ("VTE: already on screen so faking keypress");
			g_timeout_add_seconds (1, (GSourceFunc) gcm_calibrate_argyll_timeout_cb, calibrate_argyll);
			goto out;
		}

		/* TRANSLATORS: title, device is a hardware color calibration sensor */
		title = _("Please attach device");

		/* TRANSLATORS: dialog message, ask user to attach device */
		message = _("Please attach the hardware device to the center of the screen on the gray square.");

		/* block for a response */
		egg_debug ("blocking waiting for user input: %s", title);

		/* push new messages into the UI */
		gcm_calibrate_argyll_set_dialog (calibrate_argyll, title, message);

		/* set state */
		priv->state = GCM_CALIBRATE_ARGYLL_STATE_WAITING_FOR_STDIN,

		/* save as we know the device is on the screen now */
		priv->already_on_window = TRUE;
		goto out;
	}

	/* set to calibrate */
	if (g_strcmp0 (line, "Set instrument sensor to calibration position,") == 0) {
		egg_debug ("VTE: interaction required, set to calibrate");

		/* TRANSLATORS: title, device is a hardware color calibration sensor */
		title = _("Please configure device");

		/* TRANSLATORS: this is when the user has to change a setting on the sensor */
		message = _("Please set the device to calibration mode.");

		/* block for a response */
		egg_debug ("blocking waiting for user input: %s", title);

		/* push new messages into the UI */
		gcm_calibrate_argyll_set_dialog (calibrate_argyll, title, message);

		/* set state */
		priv->state = GCM_CALIBRATE_ARGYLL_STATE_WAITING_FOR_STDIN;
		goto out;
	}

	/* lines we're ignoring */
	if (g_strcmp0 (line, "Q") == 0 ||
	    g_strcmp0 (line, "Sample read stopped at user request!") == 0 ||
	    g_strcmp0 (line, "Hit Esc or Q to give up, any other key to retry:") == 0 ||
	    g_strcmp0 (line, "Calibration complete") == 0 ||
	    g_strcmp0 (line, "or hit Esc or Q to abort:") == 0 ||
	    g_strcmp0 (line, "The instrument can be removed from the screen.") == 0 ||
	    g_str_has_suffix (line, "key to continue:")) {
		egg_debug ("VTE: ignore: %s", line);
		goto out;
	}

	/* report a warning so friendly people report bugs */
	egg_warning ("VTE: could not screenscrape: %s", line);
out:
	return;
}

/**
 * gcm_calibrate_argyll_selection_func_cb:
 **/
static gboolean
gcm_calibrate_argyll_selection_func_cb (VteTerminal *terminal, glong column, glong row, gpointer data)
{
	/* just select everything */
	return TRUE;
}

/**
 * gcm_calibrate_argyll_cursor_moved_cb:
 **/
static void
gcm_calibrate_argyll_cursor_moved_cb (VteTerminal *terminal, GcmCalibrateArgyll *calibrate_argyll)
{
	gchar *output;
	gchar **split;
	guint i;
	glong row;
	glong col;
	GcmCalibrateArgyllPrivate *priv = calibrate_argyll->priv;

	/* select the text we've got since last time */
	vte_terminal_get_cursor_position (terminal, &col, &row);
	output = vte_terminal_get_text_range (terminal,
					      priv->vte_previous_row,
					      priv->vte_previous_col,
					      row, col,
					      gcm_calibrate_argyll_selection_func_cb,
					      calibrate_argyll,
					      NULL);
	split = g_strsplit (output, "\n", -1);
	for (i=0; split[i] != NULL; i++) {
		g_strchomp (split[i]);
		if (split[i][0] == '\0')
			continue;
		gcm_calibrate_argyll_process_output_cmd (calibrate_argyll, split[i]);
	}

	/* save, so we don't re-process old text */
	priv->vte_previous_row = row;
	priv->vte_previous_col = col;

	g_free (output);
	g_strfreev (split);
}

/**
 * gcm_calibrate_argyll_get_property:
 **/
static void
gcm_calibrate_argyll_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gcm_calibrate_argyll_cancel_cb:
 **/
static void
gcm_calibrate_argyll_cancel_cb (GtkWidget *widget, GcmCalibrateArgyll *calibrate_argyll)
{
	GcmCalibrateArgyllPrivate *priv = calibrate_argyll->priv;

	priv->response = GTK_RESPONSE_CANCEL;

	/* send input if waiting */
	if (priv->state == GCM_CALIBRATE_ARGYLL_STATE_WAITING_FOR_STDIN)
		vte_terminal_feed_child (VTE_TERMINAL(priv->terminal), "Q", 1);

	/* clear loop if waiting */
	if (priv->state == GCM_CALIBRATE_ARGYLL_STATE_WAITING_FOR_LOOP)
		g_main_loop_quit (priv->loop);

	/* stop loop */
	if (g_main_loop_is_running (priv->loop))
		g_main_loop_quit (priv->loop);
}

/**
 * gcm_calibrate_argyll_ok_cb:
 **/
static void
gcm_calibrate_argyll_ok_cb (GtkWidget *widget, GcmCalibrateArgyll *calibrate_argyll)
{
	GcmCalibrateArgyllPrivate *priv = calibrate_argyll->priv;

	/* send input if waiting */
	if (priv->state == GCM_CALIBRATE_ARGYLL_STATE_WAITING_FOR_STDIN) {
		vte_terminal_feed_child (VTE_TERMINAL(priv->terminal), " ", 1);
		gcm_calibrate_argyll_pop_dialog (calibrate_argyll);
	}

	/* clear loop if waiting */
	if (priv->state == GCM_CALIBRATE_ARGYLL_STATE_WAITING_FOR_LOOP)
		g_main_loop_quit (priv->loop);

	priv->response = GTK_RESPONSE_OK;
	gtk_widget_hide (widget);
}

/**
 * gcm_calibrate_argyll_delete_event_cb:
 **/
static gboolean
gcm_calibrate_argyll_delete_event_cb (GtkWidget *widget, GdkEvent *event, GcmCalibrateArgyll *calibrate_argyll)
{
	gcm_calibrate_argyll_cancel_cb (widget, calibrate_argyll);
	return FALSE;
}

/**
 * gcm_calibrate_argyll_set_property:
 **/
static void
gcm_calibrate_argyll_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gcm_calibrate_argyll_class_init:
 **/
static void
gcm_calibrate_argyll_class_init (GcmCalibrateArgyllClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GcmCalibrateClass *parent_class = GCM_CALIBRATE_CLASS (klass);
	object_class->finalize = gcm_calibrate_argyll_finalize;
	object_class->get_property = gcm_calibrate_argyll_get_property;
	object_class->set_property = gcm_calibrate_argyll_set_property;

	/* setup klass links */
	parent_class->calibrate_display = gcm_calibrate_argyll_display;
	parent_class->calibrate_device = gcm_calibrate_argyll_device;

	g_type_class_add_private (klass, sizeof (GcmCalibrateArgyllPrivate));
}

/**
 * gcm_calibrate_argyll_init:
 **/
static void
gcm_calibrate_argyll_init (GcmCalibrateArgyll *calibrate_argyll)
{
	gint retval;
	GError *error = NULL;
	GtkWidget *widget;
	GtkWidget *main_window;
	gchar *precision;

	calibrate_argyll->priv = GCM_CALIBRATE_ARGYLL_GET_PRIVATE (calibrate_argyll);
	calibrate_argyll->priv->child_pid = -1;
	calibrate_argyll->priv->loop = g_main_loop_new (NULL, FALSE);
	calibrate_argyll->priv->vte_previous_row = 0;
	calibrate_argyll->priv->vte_previous_col = 0;
	calibrate_argyll->priv->cached_titles = g_ptr_array_new_with_free_func (g_free);
	calibrate_argyll->priv->cached_messages = g_ptr_array_new_with_free_func (g_free);
	calibrate_argyll->priv->already_on_window = FALSE;
	calibrate_argyll->priv->state = GCM_CALIBRATE_ARGYLL_STATE_IDLE;

	/* get UI */
	calibrate_argyll->priv->builder = gtk_builder_new ();
	retval = gtk_builder_add_from_file (calibrate_argyll->priv->builder, GCM_DATA "/gcm-spawn.ui", &error);
	if (retval == 0) {
		egg_warning ("failed to load ui: %s", error->message);
		g_error_free (error);
		return;
	}

	/* use GConf to get defaults */
	calibrate_argyll->priv->gconf_client = gconf_client_get_default ();

	/* get screen */
	calibrate_argyll->priv->screen = gcm_screen_new ();

	/* set icon */
	main_window = GTK_WIDGET (gtk_builder_get_object (calibrate_argyll->priv->builder, "dialog_calibrate"));
	gtk_window_set_icon_name (GTK_WINDOW (main_window), GCM_STOCK_ICON);
	g_signal_connect (main_window, "delete_event",
			  G_CALLBACK (gcm_calibrate_argyll_delete_event_cb), calibrate_argyll);

	widget = GTK_WIDGET (gtk_builder_get_object (calibrate_argyll->priv->builder, "button_cancel"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gcm_calibrate_argyll_cancel_cb), calibrate_argyll);
	widget = GTK_WIDGET (gtk_builder_get_object (calibrate_argyll->priv->builder, "button_ok"));
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (gcm_calibrate_argyll_ok_cb), calibrate_argyll);

	/* add vte widget */
	calibrate_argyll->priv->terminal = vte_terminal_new ();
	vte_terminal_set_size (VTE_TERMINAL(calibrate_argyll->priv->terminal), 80, 10);
	g_signal_connect (calibrate_argyll->priv->terminal, "child-exited",
			  G_CALLBACK (gcm_calibrate_argyll_exit_cb), calibrate_argyll);
	g_signal_connect (calibrate_argyll->priv->terminal, "cursor-moved",
			  G_CALLBACK (gcm_calibrate_argyll_cursor_moved_cb), calibrate_argyll);

	widget = GTK_WIDGET (gtk_builder_get_object (calibrate_argyll->priv->builder, "vbox_details"));
	gtk_box_pack_end (GTK_BOX(widget), calibrate_argyll->priv->terminal, TRUE, TRUE, 6);

	/* get default precision */
	precision = gconf_client_get_string (calibrate_argyll->priv->gconf_client, GCM_SETTINGS_CALIBRATION_LENGTH, NULL);
	calibrate_argyll->priv->precision = gcm_calibrate_argyll_precision_from_string (precision);
	g_free (precision);
}

/**
 * gcm_calibrate_argyll_finalize:
 **/
static void
gcm_calibrate_argyll_finalize (GObject *object)
{
	GtkWidget *widget;
	GcmCalibrateArgyll *calibrate_argyll = GCM_CALIBRATE_ARGYLL (object);
	GcmCalibrateArgyllPrivate *priv = calibrate_argyll->priv;

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

	g_main_loop_unref (priv->loop);
	g_object_unref (priv->builder);
	g_object_unref (priv->screen);
	g_object_unref (priv->gconf_client);
	g_ptr_array_unref (priv->cached_titles);
	g_ptr_array_unref (priv->cached_messages);

	G_OBJECT_CLASS (gcm_calibrate_argyll_parent_class)->finalize (object);
}

/**
 * gcm_calibrate_argyll_new:
 *
 * Return value: a new GcmCalibrateArgyll object.
 **/
GcmCalibrateArgyll *
gcm_calibrate_argyll_new (void)
{
	GcmCalibrateArgyll *calibrate_argyll;
	calibrate_argyll = g_object_new (GCM_TYPE_CALIBRATE_ARGYLL, NULL);
	return GCM_CALIBRATE_ARGYLL (calibrate_argyll);
}


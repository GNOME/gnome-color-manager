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
#include <canberra-gtk.h>

#include "gcm-calibrate-argyll.h"
#include "gcm-colorimeter.h"
#include "gcm-utils.h"
#include "gcm-screen.h"
#include "gcm-print.h"
#include "gcm-xyz.h"
#include "gcm-calibrate-dialog.h"

#include "egg-debug.h"

static void     gcm_calibrate_argyll_finalize	(GObject     *object);

#define GCM_CALIBRATE_ARGYLL_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GCM_TYPE_CALIBRATE_ARGYLL, GcmCalibrateArgyllPrivate))

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
	GMainLoop			*loop;
	GtkWidget			*terminal;
	GcmCalibrateDialog		*calibrate_dialog;
	pid_t				 child_pid;
	GtkResponseType			 response;
	GcmScreen			*screen;
	glong				 vte_previous_row;
	glong				 vte_previous_col;
	gboolean			 already_on_window;
	gboolean			 done_calibrate;
	GcmCalibrateArgyllState		 state;
	GcmPrint			*print;
	const gchar			*argyllcms_ok;
	gboolean			 done_spot_read;
};

enum {
	PROP_0,
	PROP_LAST
};

G_DEFINE_TYPE (GcmCalibrateArgyll, gcm_calibrate_argyll, GCM_TYPE_CALIBRATE)

/**
 * gcm_calibrate_argyll_get_quality_arg:
 **/
static const gchar *
gcm_calibrate_argyll_get_quality_arg (GcmCalibrateArgyll *calibrate_argyll)
{
	GcmCalibrateReferenceKind reference_kind;
	GcmCalibratePrecision precision;

	/* get kind */
	g_object_get (calibrate_argyll,
		      "reference-kind", &reference_kind,
		      "precision", &precision,
		      NULL);

	/* these have such low patch count, we only can do low quality */
	if (reference_kind == GCM_CALIBRATE_REFERENCE_KIND_COLOR_CHECKER ||
	    reference_kind == GCM_CALIBRATE_REFERENCE_KIND_QPCARD_201)
		return "-ql";

	/* get the default precision */
	if (precision == GCM_CALIBRATE_PRECISION_SHORT)
		return "-ql";
	if (precision == GCM_CALIBRATE_PRECISION_NORMAL)
		return "-qm";
	if (precision == GCM_CALIBRATE_PRECISION_LONG)
		return "-qh";
	return "-qm";
}

/**
 * gcm_calibrate_argyll_display_get_patches:
 **/
static guint
gcm_calibrate_argyll_display_get_patches (GcmCalibrateArgyll *calibrate_argyll)
{
	guint patches = 250;
	GcmCalibratePrecision precision;

	/* get kind */
	g_object_get (calibrate_argyll,
		      "precision", &precision,
		      NULL);

	if (precision == GCM_CALIBRATE_PRECISION_SHORT)
		patches = 100;
	else if (precision == GCM_CALIBRATE_PRECISION_NORMAL)
		patches = 250;
	else if (precision == GCM_CALIBRATE_PRECISION_LONG)
		patches = 500;
	return patches;
}

/**
 * gcm_calibrate_argyll_printer_get_patches:
 **/
static guint
gcm_calibrate_argyll_printer_get_patches (GcmCalibrateArgyll *calibrate_argyll)
{
	guint patches = 180;
	GcmColorimeterKind colorimeter_kind;
	GcmCalibratePrecision precision;

	/* we care about the kind */
	g_object_get (calibrate_argyll,
		      "colorimeter-kind", &colorimeter_kind,
		      "precision", &precision,
		      NULL);

	if (precision == GCM_CALIBRATE_PRECISION_SHORT)
		patches = 90;
	else if (precision == GCM_CALIBRATE_PRECISION_NORMAL)
		patches = 180;
	else if (precision == GCM_CALIBRATE_PRECISION_LONG)
		patches = 360;

	/* using double density, so we can double the patch count */
	if (colorimeter_kind == GCM_COLORIMETER_KIND_COLOR_MUNKI ||
	    colorimeter_kind == GCM_COLORIMETER_KIND_SPECTRO_SCAN) {
		patches *= 2;
	}

	return patches;
}

/**
 * gcm_calibrate_argyll_get_colorimeter_image_attach:
 **/
static const gchar *
gcm_calibrate_argyll_get_colorimeter_image_attach (GcmCalibrateArgyll *calibrate_argyll)
{
	GcmColorimeterKind colorimeter_kind;

	g_object_get (calibrate_argyll, "colorimeter-kind", &colorimeter_kind, NULL);
	if (colorimeter_kind == GCM_COLORIMETER_KIND_HUEY)
		return "huey-attach.svg";
	if (colorimeter_kind == GCM_COLORIMETER_KIND_COLOR_MUNKI)
		return "munki-attach.svg";
	if (colorimeter_kind == GCM_COLORIMETER_KIND_SPYDER)
		return "spyder-attach.svg";
	if (colorimeter_kind == GCM_COLORIMETER_KIND_COLORIMTRE_HCFR)
		return "hcfr-attach.svg";
	if (colorimeter_kind == GCM_COLORIMETER_KIND_I1_PRO)
		return "i1-attach.svg";
	return NULL;
}

/**
 * gcm_calibrate_argyll_get_colorimeter_image_calibrate:
 **/
static const gchar *
gcm_calibrate_argyll_get_colorimeter_image_calibrate (GcmCalibrateArgyll *calibrate_argyll)
{
	GcmColorimeterKind colorimeter_kind;

	g_object_get (calibrate_argyll, "colorimeter-kind", &colorimeter_kind, NULL);
	if (colorimeter_kind == GCM_COLORIMETER_KIND_COLOR_MUNKI)
		return "munki-calibrate.svg";
	return NULL;
}

/**
 * gcm_calibrate_argyll_get_colorimeter_image_screen:
 **/
static const gchar *
gcm_calibrate_argyll_get_colorimeter_image_screen (GcmCalibrateArgyll *calibrate_argyll)
{
	GcmColorimeterKind colorimeter_kind;

	g_object_get (calibrate_argyll, "colorimeter-kind", &colorimeter_kind, NULL);
	if (colorimeter_kind == GCM_COLORIMETER_KIND_COLOR_MUNKI)
		return "munki-screen.svg";
	return NULL;
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
			g_set_error_literal (error,
					     GCM_CALIBRATE_ERROR,
					     GCM_CALIBRATE_ERROR_INTERNAL,
					     "failed to match display as RandR is faulty");
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
		g_set_error_literal (error,
				     GCM_CALIBRATE_ERROR,
				     GCM_CALIBRATE_ERROR_INTERNAL,
				     "failed to match display");
		goto out;
	}
out:
	g_free (data);
	g_strfreev (split);
	return display;
}

/**
 * gcm_calibrate_argyll_get_display_kind:
 **/
static gchar
gcm_calibrate_argyll_get_display_kind (GcmCalibrateArgyll *calibrate_argyll)
{
	GcmCalibrateDeviceKind device_kind;

	g_object_get (calibrate_argyll,
		      "calibrate-device-kind", &device_kind,
		      NULL);

	if (device_kind == GCM_CALIBRATE_DEVICE_KIND_LCD)
		return 'l';
	if (device_kind == GCM_CALIBRATE_DEVICE_KIND_CRT)
		return 'c';
	if (device_kind == GCM_CALIBRATE_DEVICE_KIND_PROJECTOR)
		return 'p';
	return '\0';
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
	g_set_error (error,
		     GCM_CALIBRATE_ERROR,
		     GCM_CALIBRATE_ERROR_INTERNAL,
		     "failed to get filename for %s", command);
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
	gchar kind;
	gchar *command = NULL;
	gchar **argv = NULL;
	GnomeRROutput *output;
	GPtrArray *array = NULL;
	gchar *basename = NULL;
	gchar *working_path = NULL;
	gchar *output_name = NULL;
	const gchar *title;
	const gchar *message;

	/* get shared data */
	g_object_get (calibrate_argyll,
		      "basename", &basename,
		      "working-path", &working_path,
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
	kind = gcm_calibrate_argyll_get_display_kind (calibrate_argyll);

	/* TRANSLATORS: title, default paramters needed to calibrate_argyll */
	title = _("Getting default parameters");

	/* TRANSLATORS: dialog message */
	message = _("This pre-calibrates the screen by sending colored and gray patches to your screen and measuring them with the hardware device.");

	/* push new messages into the UI */
	gcm_calibrate_dialog_show (priv->calibrate_dialog, GCM_CALIBRATE_DIALOG_TAB_GENERIC, title, message);
	gcm_calibrate_dialog_set_show_button_ok (priv->calibrate_dialog, FALSE);
	gcm_calibrate_dialog_set_show_expander (priv->calibrate_dialog, TRUE);

	/* argument array */
	array = g_ptr_array_new_with_free_func (g_free);

	/* setup the command */
	g_ptr_array_add (array, g_strdup ("-v9"));
	g_ptr_array_add (array, g_strdup ("-ql"));
	g_ptr_array_add (array, g_strdup ("-m"));
	g_ptr_array_add (array, g_strdup_printf ("-d%i", priv->display));
	g_ptr_array_add (array, g_strdup_printf ("-y%c", kind));
//	g_ptr_array_add (array, g_strdup ("-p 0.8,0.5,1.0"));
	g_ptr_array_add (array, g_strdup (basename));
	argv = gcm_utils_ptr_array_to_strv (array);
	gcm_calibrate_argyll_debug_argv (command, argv);

	/* start up the command */
	priv->state = GCM_CALIBRATE_ARGYLL_STATE_RUNNING;
	vte_terminal_reset (VTE_TERMINAL(priv->terminal), TRUE, FALSE);
	priv->child_pid = vte_terminal_fork_command (VTE_TERMINAL(priv->terminal), command, argv, NULL, working_path, FALSE, FALSE, FALSE);

	/* wait until finished */
	g_main_loop_run (priv->loop);

	/* get result */
	if (priv->response == GTK_RESPONSE_CANCEL) {
		g_set_error_literal (error,
				     GCM_CALIBRATE_ERROR,
				     GCM_CALIBRATE_ERROR_USER_ABORT,
				     "calibration was cancelled");
		ret = FALSE;
		goto out;
	}
	if (priv->response == GTK_RESPONSE_REJECT) {
		gchar *vte_text;
		vte_text = vte_terminal_get_text (VTE_TERMINAL(priv->terminal), NULL, NULL, NULL);
		g_set_error (error,
			     GCM_CALIBRATE_ERROR,
			     GCM_CALIBRATE_ERROR_INTERNAL,
			     "command failed to run successfully: %s", vte_text);
		g_free (vte_text);
		ret = FALSE;
		goto out;
	}
out:
	if (array != NULL)
		g_ptr_array_unref (array);
	g_free (working_path);
	g_free (basename);
	g_free (output_name);
	g_free (command);
	g_strfreev (argv);
	return ret;
}


/**
 * gcm_calibrate_argyll_display_read_chart:
 **/
static gboolean
gcm_calibrate_argyll_display_read_chart (GcmCalibrateArgyll *calibrate_argyll, GError **error)
{
	gboolean ret = TRUE;
	GcmCalibrateArgyllPrivate *priv = calibrate_argyll->priv;
	gchar *command = NULL;
	gchar **argv = NULL;
	GPtrArray *array = NULL;
	gchar *basename = NULL;
	gchar *working_path = NULL;
	const gchar *title;
	const gchar *message;

	/* get shared data */
	g_object_get (calibrate_argyll,
		      "basename", &basename,
		      "working-path", &working_path,
		      NULL);

	/* get correct name of the command */
	command = gcm_calibrate_argyll_get_tool_filename ("chartread", error);
	if (command == NULL) {
		ret = FALSE;
		goto out;
	}

	/* TRANSLATORS: title, patches are specific colours used in calibration */
	title = _("Reading the patches");
	/* TRANSLATORS: dialog message */
	message = _("Reading the patches using the color measuring instrument.");

	/* push new messages into the UI */
	gcm_calibrate_dialog_show (priv->calibrate_dialog, GCM_CALIBRATE_DIALOG_TAB_GENERIC, title, message);
	gcm_calibrate_dialog_set_show_button_ok (priv->calibrate_dialog, FALSE);
	gcm_calibrate_dialog_set_show_expander (priv->calibrate_dialog, TRUE);

	/* argument array */
	array = g_ptr_array_new_with_free_func (g_free);

	/* setup the command */
	g_ptr_array_add (array, g_strdup ("-v9"));
	if (priv->done_calibrate)
		g_ptr_array_add (array, g_strdup ("-N"));
	g_ptr_array_add (array, g_strdup (basename));
	argv = gcm_utils_ptr_array_to_strv (array);
	gcm_calibrate_argyll_debug_argv (command, argv);

	/* start up the command */
	priv->state = GCM_CALIBRATE_ARGYLL_STATE_RUNNING;
	vte_terminal_reset (VTE_TERMINAL(priv->terminal), TRUE, FALSE);
	priv->child_pid = vte_terminal_fork_command (VTE_TERMINAL(priv->terminal), command, argv, NULL, working_path, FALSE, FALSE, FALSE);

	/* wait until finished */
	g_main_loop_run (priv->loop);

	/* get result */
	if (priv->response == GTK_RESPONSE_CANCEL) {
		g_set_error_literal (error,
				     GCM_CALIBRATE_ERROR,
				     GCM_CALIBRATE_ERROR_USER_ABORT,
				     "calibration was cancelled");
		ret = FALSE;
		goto out;
	}
	if (priv->response == GTK_RESPONSE_REJECT) {
		gchar *vte_text;
		vte_text = vte_terminal_get_text (VTE_TERMINAL(priv->terminal), NULL, NULL, NULL);
		g_set_error (error,
			     GCM_CALIBRATE_ERROR,
			     GCM_CALIBRATE_ERROR_INTERNAL,
			     "command failed to run successfully: %s", vte_text);
		g_free (vte_text);
		ret = FALSE;
		goto out;
	}
out:
	if (array != NULL)
		g_ptr_array_unref (array);
	g_free (working_path);
	g_free (basename);
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
	gchar *working_path = NULL;
	const gchar *title;
	const gchar *message;
	GcmDeviceKind device_kind;

	/* get shared data */
	g_object_get (calibrate_argyll,
		      "basename", &basename,
		      "working-path", &working_path,
		      "device-kind", &device_kind,
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
	message = _("Generating the patches that will be measured with the color instrument.");

	/* push new messages into the UI */
	gcm_calibrate_dialog_show (priv->calibrate_dialog, GCM_CALIBRATE_DIALOG_TAB_GENERIC, title, message);
	gcm_calibrate_dialog_set_show_button_ok (priv->calibrate_dialog, FALSE);
	gcm_calibrate_dialog_set_show_expander (priv->calibrate_dialog, TRUE);

	/* argument array */
	array = g_ptr_array_new_with_free_func (g_free);

	/* setup the command */
	g_ptr_array_add (array, g_strdup ("-v9"));
	if (device_kind == GCM_DEVICE_KIND_PRINTER) {
		/* print RGB */
		g_ptr_array_add (array, g_strdup ("-d2"));

		/* Grey axis RGB steps */
		g_ptr_array_add (array, g_strdup ("-g20"));
	} else {
		/* video RGB */
		g_ptr_array_add (array, g_strdup ("-d3"));
	}

	/* get number of patches */
	if (device_kind == GCM_DEVICE_KIND_DISPLAY)
		g_ptr_array_add (array, g_strdup_printf ("-f%i", gcm_calibrate_argyll_display_get_patches (calibrate_argyll)));
	else if (device_kind == GCM_DEVICE_KIND_PRINTER)
		g_ptr_array_add (array, g_strdup_printf ("-f%i", gcm_calibrate_argyll_printer_get_patches (calibrate_argyll)));

	g_ptr_array_add (array, g_strdup (basename));
	argv = gcm_utils_ptr_array_to_strv (array);
	gcm_calibrate_argyll_debug_argv (command, argv);

	/* start up the command */
	priv->state = GCM_CALIBRATE_ARGYLL_STATE_RUNNING;
	vte_terminal_reset (VTE_TERMINAL(priv->terminal), TRUE, FALSE);
	priv->child_pid = vte_terminal_fork_command (VTE_TERMINAL(priv->terminal), command, argv, NULL, working_path, FALSE, FALSE, FALSE);

	/* wait until finished */
	g_main_loop_run (priv->loop);

	/* get result */
	if (priv->response == GTK_RESPONSE_CANCEL) {
		g_set_error_literal (error,
				     GCM_CALIBRATE_ERROR,
				     GCM_CALIBRATE_ERROR_USER_ABORT,
				     "calibration was cancelled");
		ret = FALSE;
		goto out;
	}
	if (priv->response == GTK_RESPONSE_REJECT) {
		gchar *vte_text;
		vte_text = vte_terminal_get_text (VTE_TERMINAL(priv->terminal), NULL, NULL, NULL);
		g_set_error (error,
			     GCM_CALIBRATE_ERROR,
			     GCM_CALIBRATE_ERROR_INTERNAL,
			     "command failed to run successfully: %s", vte_text);
		g_free (vte_text);
		ret = FALSE;
		goto out;
	}
out:
	if (array != NULL)
		g_ptr_array_unref (array);
	g_free (working_path);
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
	gchar kind;
	gchar *command = NULL;
	gchar **argv = NULL;
	GPtrArray *array = NULL;
	gchar *basename = NULL;
	gchar *working_path = NULL;
	const gchar *title;
	const gchar *message;

	/* get shared data */
	g_object_get (calibrate_argyll,
		      "working-path", &working_path,
		      "basename", &basename,
		      NULL);

	/* get correct name of the command */
	command = gcm_calibrate_argyll_get_tool_filename ("dispread", error);
	if (command == NULL) {
		ret = FALSE;
		goto out;
	}

	/* get l-cd or c-rt */
	kind = gcm_calibrate_argyll_get_display_kind (calibrate_argyll);

	/* TRANSLATORS: title, drawing means painting to the screen */
	title = _("Drawing the patches");
	/* TRANSLATORS: dialog message */
	message = _("Drawing the generated patches to the screen, which will then be measured by the hardware device.");

	/* push new messages into the UI */
	gcm_calibrate_dialog_show (priv->calibrate_dialog, GCM_CALIBRATE_DIALOG_TAB_GENERIC, title, message);
	gcm_calibrate_dialog_set_show_button_ok (priv->calibrate_dialog, FALSE);
	gcm_calibrate_dialog_set_show_expander (priv->calibrate_dialog, TRUE);

	/* argument array */
	array = g_ptr_array_new_with_free_func (g_free);

	/* setup the command */
	g_ptr_array_add (array, g_strdup ("-v9"));
	g_ptr_array_add (array, g_strdup_printf ("-d%i", priv->display));
	g_ptr_array_add (array, g_strdup_printf ("-y%c", kind));
	g_ptr_array_add (array, g_strdup ("-k"));
	g_ptr_array_add (array, g_strdup_printf ("%s.cal", basename));
	g_ptr_array_add (array, g_strdup (basename));
	argv = gcm_utils_ptr_array_to_strv (array);
	gcm_calibrate_argyll_debug_argv (command, argv);

	/* start up the command */
	priv->state = GCM_CALIBRATE_ARGYLL_STATE_RUNNING;
	vte_terminal_reset (VTE_TERMINAL(priv->terminal), TRUE, FALSE);
	priv->child_pid = vte_terminal_fork_command (VTE_TERMINAL(priv->terminal), command, argv, NULL, working_path, FALSE, FALSE, FALSE);

	/* wait until finished */
	g_main_loop_run (priv->loop);

	/* get result */
	if (priv->response == GTK_RESPONSE_CANCEL) {
		g_set_error_literal (error,
				     GCM_CALIBRATE_ERROR,
				     GCM_CALIBRATE_ERROR_USER_ABORT,
				     "calibration was cancelled");
		ret = FALSE;
		goto out;
	}
	if (priv->response == GTK_RESPONSE_REJECT) {
		gchar *vte_text;
		vte_text = vte_terminal_get_text (VTE_TERMINAL(priv->terminal), NULL, NULL, NULL);
		g_set_error (error,
			     GCM_CALIBRATE_ERROR,
			     GCM_CALIBRATE_ERROR_INTERNAL,
			     "command failed to run successfully: %s", vte_text);
		g_free (vte_text);
		ret = FALSE;
		goto out;
	}
out:
	if (array != NULL)
		g_ptr_array_unref (array);
	g_free (working_path);
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
	gchar *working_path = NULL;
	const gchar *description = NULL;
	const gchar *manufacturer = NULL;
	const gchar *model = NULL;
	gchar *device = NULL;
	GPtrArray *array = NULL;
	const gchar *title;
	const gchar *message;

	/* get shared data */
	g_object_get (calibrate_argyll,
		      "basename", &basename,
		      "working-path", &working_path,
		      "device", &device,
		      NULL);

	/* get, returning fallbacks if nothing was set */
	model = gcm_calibrate_get_model_fallback (GCM_CALIBRATE (calibrate_argyll));
	manufacturer = gcm_calibrate_get_manufacturer_fallback (GCM_CALIBRATE (calibrate_argyll));
	description = gcm_calibrate_get_description_fallback (GCM_CALIBRATE (calibrate_argyll));

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

	/* TRANSLATORS: this is the copyright string, where it might be "Copyright (c) 2009 Edward Scissorhands" - YOU NEED TO STICK TO ASCII */
	copyright = g_strdup_printf ("%s %04i %s", _("Copyright (c)"), date->year, g_get_real_name ());

	/* TRANSLATORS: title, a profile is a ICC file */
	title = _("Generating the profile");
	/* TRANSLATORS: dialog message */
	message = _("Generating the ICC color profile that can be used with this screen.");

	/* push new messages into the UI */
	gcm_calibrate_dialog_show (priv->calibrate_dialog, GCM_CALIBRATE_DIALOG_TAB_GENERIC, title, message);
	gcm_calibrate_dialog_set_show_button_ok (priv->calibrate_dialog, FALSE);
	gcm_calibrate_dialog_set_show_expander (priv->calibrate_dialog, TRUE);

	/* argument array */
	array = g_ptr_array_new_with_free_func (g_free);

	/* setup the command */
	g_ptr_array_add (array, g_strdup ("-v9"));
	g_ptr_array_add (array, g_strdup_printf ("-A%s", manufacturer));
	g_ptr_array_add (array, g_strdup_printf ("-M%s", model));
	g_ptr_array_add (array, g_strdup_printf ("-D%s", description_new));
	g_ptr_array_add (array, g_strdup_printf ("-C%s", copyright));
	g_ptr_array_add (array, g_strdup (gcm_calibrate_argyll_get_quality_arg (calibrate_argyll)));
	g_ptr_array_add (array, g_strdup ("-as"));
	g_ptr_array_add (array, g_strdup (basename));
	argv = gcm_utils_ptr_array_to_strv (array);
	gcm_calibrate_argyll_debug_argv (command, argv);

	/* start up the command */
	priv->state = GCM_CALIBRATE_ARGYLL_STATE_RUNNING;
	vte_terminal_reset (VTE_TERMINAL(priv->terminal), TRUE, FALSE);
	priv->child_pid = vte_terminal_fork_command (VTE_TERMINAL(priv->terminal), command, argv, NULL, working_path, FALSE, FALSE, FALSE);

	/* wait until finished */
	g_main_loop_run (priv->loop);

	/* get result */
	if (priv->response == GTK_RESPONSE_CANCEL) {
		g_set_error_literal (error,
				     GCM_CALIBRATE_ERROR,
				     GCM_CALIBRATE_ERROR_USER_ABORT,
				     "calibration was cancelled");
		ret = FALSE;
		goto out;
	}
	if (priv->response == GTK_RESPONSE_REJECT) {
		gchar *vte_text;
		vte_text = vte_terminal_get_text (VTE_TERMINAL(priv->terminal), NULL, NULL, NULL);
		g_set_error (error,
			     GCM_CALIBRATE_ERROR,
			     GCM_CALIBRATE_ERROR_INTERNAL,
			     "command failed to run successfully: %s", vte_text);
		g_free (vte_text);
		ret = FALSE;
		goto out;
	}

out:
	if (array != NULL)
		g_ptr_array_unref (array);
	if (date != NULL)
		g_date_free (date);
	g_free (working_path);
	g_free (basename);
	g_free (command);
	g_free (description_new);
	g_free (device);
	g_free (copyright);
	g_strfreev (argv);
	return ret;
}

/**
 * gcm_calibrate_argyll_reference_kind_to_filename:
 **/
static const gchar *
gcm_calibrate_argyll_reference_kind_to_filename (GcmCalibrateReferenceKind kind)
{
	if (kind == GCM_CALIBRATE_REFERENCE_KIND_CMP_DIGITAL_TARGET_3)
		return "CMP_Digital_Target-3.cht";
	if (kind == GCM_CALIBRATE_REFERENCE_KIND_CMP_DT_003)
		return "CMP_DT_003.cht";
	if (kind == GCM_CALIBRATE_REFERENCE_KIND_COLOR_CHECKER)
		return "ColorChecker.cht";
	if (kind == GCM_CALIBRATE_REFERENCE_KIND_COLOR_CHECKER_DC)
		return "ColorCheckerDC.cht";
	if (kind == GCM_CALIBRATE_REFERENCE_KIND_COLOR_CHECKER_SG)
		return "ColorCheckerSG.cht";
	if (kind == GCM_CALIBRATE_REFERENCE_KIND_HUTCHCOLOR)
		return "Hutchcolor.cht";
	if (kind == GCM_CALIBRATE_REFERENCE_KIND_I1_RGB_SCAN_1_4)
		return "i1_RGB_Scan_1.4.cht";
	if (kind == GCM_CALIBRATE_REFERENCE_KIND_IT8)
		return "it8.cht";
	if (kind == GCM_CALIBRATE_REFERENCE_KIND_LASER_SOFT_DC_PRO)
		return "LaserSoftDCPro.cht";
	if (kind == GCM_CALIBRATE_REFERENCE_KIND_QPCARD_201)
		return "QPcard_201.cht";
	g_assert_not_reached ();
	return NULL;
}

/**
 * gcm_calibrate_argyll_device_copy:
 **/
static gboolean
gcm_calibrate_argyll_device_copy (GcmCalibrateArgyll *calibrate_argyll, GError **error)
{
	gboolean ret;
	gchar *device = NULL;
	gchar *destination_cht = NULL;
	gchar *destination_ref = NULL;
	gchar *filename = NULL;
	gchar *basename = NULL;
	gchar *filename_cht = NULL;
	gchar *filename_source = NULL;
	gchar *filename_reference = NULL;
	GFile *file_cht = NULL;
	GFile *file_source = NULL;
	GFile *file_reference = NULL;
	GFile *dest_cht = NULL;
	GFile *dest_source = NULL;
	GFile *dest_reference = NULL;
	gchar *working_path = NULL;
	const gchar *title;
	const gchar *message;
	const gchar *filename_tmp;
	GcmCalibrateReferenceKind reference_kind;
	GcmCalibrateArgyllPrivate *priv = calibrate_argyll->priv;

	/* get shared data */
	g_object_get (calibrate_argyll,
		      "basename", &basename,
		      "working-path", &working_path,
		      "reference-kind", &reference_kind,
		      "filename-source", &filename_source,
		      "filename-reference", &filename_reference,
		      NULL);

	/* TRANSLATORS: title, a profile is a ICC file */
	title = _("Copying files");
	/* TRANSLATORS: dialog message */
	message = _("Copying source image, chart data and CIE reference values.");

	/* push new messages into the UI */
	gcm_calibrate_dialog_show (priv->calibrate_dialog, GCM_CALIBRATE_DIALOG_TAB_GENERIC, title, message);
	gcm_calibrate_dialog_set_show_button_ok (priv->calibrate_dialog, FALSE);
	gcm_calibrate_dialog_set_show_expander (priv->calibrate_dialog, FALSE);

	/* build filenames */
	filename = g_strdup_printf ("%s.tif", basename);
	device = g_build_filename (working_path, filename, NULL);
	destination_cht = g_build_filename (working_path, "scanin.cht", NULL);
	destination_ref = g_build_filename (working_path, "scanin-ref.txt", NULL);

	/* copy all files to /tmp as argyllcms doesn't cope well with paths */
	filename_tmp = gcm_calibrate_argyll_reference_kind_to_filename (reference_kind);
	filename_cht = g_build_filename ("/usr/share/color/argyll/ref", filename_tmp, NULL);

	/* convert to GFile */
	file_cht = g_file_new_for_path (filename_cht);
	file_source = g_file_new_for_path (filename_source);
	file_reference = g_file_new_for_path (filename_reference);
	dest_cht = g_file_new_for_path (destination_cht);
	dest_source = g_file_new_for_path (device);
	dest_reference = g_file_new_for_path (destination_ref);

	/* do the copy */
	ret = gcm_utils_mkdir_and_copy (file_cht, dest_cht, error);
	if (!ret)
		goto out;
	ret = gcm_utils_mkdir_and_copy (file_source, dest_source, error);
	if (!ret)
		goto out;
	ret = gcm_utils_mkdir_and_copy (file_reference, dest_reference, error);
	if (!ret)
		goto out;
out:
	g_free (working_path);
	g_free (basename);
	g_free (filename);
	g_free (filename_cht);
	g_free (filename_source);
	g_free (filename_reference);
	g_free (device);
	g_free (destination_cht);
	g_free (destination_ref);
	g_object_unref (file_cht);
	g_object_unref (file_source);
	g_object_unref (file_reference);
	g_object_unref (dest_cht);
	g_object_unref (dest_source);
	g_object_unref (dest_reference);
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
	gchar *working_path = NULL;
	const gchar *title;
	const gchar *message;

	/* get shared data */
	g_object_get (calibrate_argyll,
		      "basename", &basename,
		      "working-path", &working_path,
		      NULL);

	/* TRANSLATORS: title, drawing means painting to the screen */
	title = _("Measuring the patches");
	/* TRANSLATORS: dialog message */
	message = _("Detecting the reference patches and measuring them.");

	/* push new messages into the UI */
	gcm_calibrate_dialog_show (priv->calibrate_dialog, GCM_CALIBRATE_DIALOG_TAB_GENERIC, title, message);
	gcm_calibrate_dialog_set_show_button_ok (priv->calibrate_dialog, FALSE);
	gcm_calibrate_dialog_set_show_expander (priv->calibrate_dialog, TRUE);

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
	g_ptr_array_add (array, g_strdup ("-p"));
	g_ptr_array_add (array, g_strdup ("-a"));
	g_ptr_array_add (array, g_strdup (filename));
	g_ptr_array_add (array, g_strdup ("scanin.cht"));
	g_ptr_array_add (array, g_strdup ("scanin-ref.txt"));
	argv = gcm_utils_ptr_array_to_strv (array);
	gcm_calibrate_argyll_debug_argv (command, argv);

	/* start up the command */
	priv->state = GCM_CALIBRATE_ARGYLL_STATE_RUNNING;
	vte_terminal_reset (VTE_TERMINAL(priv->terminal), TRUE, FALSE);
	priv->child_pid = vte_terminal_fork_command (VTE_TERMINAL(priv->terminal), command, argv, NULL, working_path, FALSE, FALSE, FALSE);

	/* wait until finished */
	g_main_loop_run (priv->loop);

	/* get result */
	if (priv->response == GTK_RESPONSE_CANCEL) {
		g_set_error_literal (error,
				     GCM_CALIBRATE_ERROR,
				     GCM_CALIBRATE_ERROR_USER_ABORT,
				     "calibration was cancelled");
		ret = FALSE;
		goto out;
	}
	if (priv->response == GTK_RESPONSE_REJECT) {
		gchar *vte_text;
		vte_text = vte_terminal_get_text (VTE_TERMINAL(priv->terminal), NULL, NULL, NULL);
		g_set_error (error,
			     GCM_CALIBRATE_ERROR,
			     GCM_CALIBRATE_ERROR_INTERNAL,
			     "command failed to run successfully: %s", vte_text);
		g_free (vte_text);
		ret = FALSE;
		goto out;
	}
out:
	g_free (working_path);
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
	const gchar *description;
	gchar *copyright = NULL;
	GPtrArray *array = NULL;
	gchar *command = NULL;
	gchar *basename = NULL;
	const gchar *manufacturer;
	const gchar *model;
	const gchar *device;
	gchar *working_path = NULL;
	const gchar *title;
	const gchar *message;
	GcmCalibrateReferenceKind reference_kind;

	/* get shared data */
	g_object_get (calibrate_argyll,
		      "basename", &basename,
		      "working-path", &working_path,
		      "reference-kind", &reference_kind,
		      NULL);

	/* get, returning fallbacks if nothing was set */
	model = gcm_calibrate_get_model_fallback (GCM_CALIBRATE (calibrate_argyll));
	manufacturer = gcm_calibrate_get_manufacturer_fallback (GCM_CALIBRATE (calibrate_argyll));
	description = gcm_calibrate_get_description_fallback (GCM_CALIBRATE (calibrate_argyll));
	device = gcm_calibrate_get_device_fallback (GCM_CALIBRATE (calibrate_argyll));

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
	gcm_calibrate_dialog_show (priv->calibrate_dialog, GCM_CALIBRATE_DIALOG_TAB_GENERIC, title, message);
	gcm_calibrate_dialog_set_show_button_ok (priv->calibrate_dialog, FALSE);
	gcm_calibrate_dialog_set_show_expander (priv->calibrate_dialog, TRUE);

	/* argument array */
	array = g_ptr_array_new_with_free_func (g_free);

	/* setup the command */
	g_ptr_array_add (array, g_strdup ("-v9"));
	g_ptr_array_add (array, g_strdup_printf ("-A%s", manufacturer));
	g_ptr_array_add (array, g_strdup_printf ("-M%s", model));
	g_ptr_array_add (array, g_strdup_printf ("-D%s", description_tmp));
	g_ptr_array_add (array, g_strdup_printf ("-C%s", copyright));
	g_ptr_array_add (array, g_strdup (gcm_calibrate_argyll_get_quality_arg (calibrate_argyll)));

	/* check whether the target is a low patch count target and generate low quality single shaper profile */
	if (reference_kind == GCM_CALIBRATE_REFERENCE_KIND_COLOR_CHECKER ||
	    reference_kind == GCM_CALIBRATE_REFERENCE_KIND_QPCARD_201)
		g_ptr_array_add (array, g_strdup ("-aG"));

	g_ptr_array_add (array, g_strdup (basename));
	argv = gcm_utils_ptr_array_to_strv (array);
	gcm_calibrate_argyll_debug_argv (command, argv);

	/* start up the command */
	priv->state = GCM_CALIBRATE_ARGYLL_STATE_RUNNING;
	vte_terminal_reset (VTE_TERMINAL(priv->terminal), TRUE, FALSE);
	priv->child_pid = vte_terminal_fork_command (VTE_TERMINAL(priv->terminal), command, argv, NULL, working_path, FALSE, FALSE, FALSE);

	/* wait until finished */
	g_main_loop_run (priv->loop);

	/* get result */
	if (priv->response == GTK_RESPONSE_CANCEL) {
		g_set_error_literal (error,
				     GCM_CALIBRATE_ERROR,
				     GCM_CALIBRATE_ERROR_USER_ABORT,
				     "calibration was cancelled");
		ret = FALSE;
		goto out;
	}
	if (priv->response == GTK_RESPONSE_REJECT) {
		gchar *vte_text;
		vte_text = vte_terminal_get_text (VTE_TERMINAL(priv->terminal), NULL, NULL, NULL);
		g_set_error (error,
			     GCM_CALIBRATE_ERROR,
			     GCM_CALIBRATE_ERROR_INTERNAL,
			     "command failed to run successfully: %s", vte_text);
		g_free (vte_text);
		ret = FALSE;
		goto out;
	}
out:
	if (array != NULL)
		g_ptr_array_unref (array);
	if (date != NULL)
		g_date_free (date);
	g_free (working_path);
	g_free (description_tmp);
	g_free (copyright);
	g_free (basename);
	g_free (command);
	g_strfreev (argv);
	return ret;
}

/**
 * gcm_calibrate_argyll_set_filename_result:
 **/
static gboolean
gcm_calibrate_argyll_set_filename_result (GcmCalibrateArgyll *calibrate_argyll, GError **error)
{
	gchar *filename = NULL;
	gboolean ret = TRUE;
	gchar *basename = NULL;
	gchar *working_path = NULL;

	/* get shared data */
	g_object_get (calibrate_argyll,
		      "basename", &basename,
		      "working-path", &working_path,
		      NULL);

	/* we can't have finished with success */
	if (basename == NULL) {
		ret = FALSE;
		g_set_error_literal (error,
				     GCM_CALIBRATE_ERROR,
				     GCM_CALIBRATE_ERROR_INTERNAL,
				     "profile was not generated");
		goto out;
	}

	/* get the finished icc file */
	filename = g_strdup_printf ("%s/%s.icc", working_path, basename);

	/* we never finished all the steps */
	if (!g_file_test (filename, G_FILE_TEST_EXISTS)) {
		ret = FALSE;
		g_set_error_literal (error,
				     GCM_CALIBRATE_ERROR,
				     GCM_CALIBRATE_ERROR_INTERNAL,
				     "could not find completed profile");
		goto out;
	}

	/* success */
	g_object_set (calibrate_argyll,
		      "filename-result", filename,
		      NULL);
out:
	g_free (working_path);
	g_free (basename);
	g_free (filename);
	return ret;
}

/**
 * gcm_calibrate_argyll_remove_temp_files:
 **/
static gboolean
gcm_calibrate_argyll_remove_temp_files (GcmCalibrateArgyll *calibrate_argyll, GError **error)
{
	gchar *filename_tmp;
	guint i;
	gboolean ret;
	const gchar *exts[] = {"cal", "ti1", "ti3", "tif", NULL};
	const gchar *filenames[] = {"scanin.cht", "scanin-ref.txt", NULL};
	gchar *basename = NULL;
	gchar *working_path = NULL;

	/* get shared data */
	g_object_get (calibrate_argyll,
		      "basename", &basename,
		      "working-path", &working_path,
		      NULL);

	/* remove all the temp files */
	if (basename != NULL) {
		for (i=0; exts[i] != NULL; i++) {
			filename_tmp = g_strdup_printf ("%s/%s.%s", working_path, basename, exts[i]);
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
		filename_tmp = g_strdup_printf ("%s/%s", working_path, filenames[i]);
		ret = g_file_test (filename_tmp, G_FILE_TEST_IS_REGULAR);
		if (ret) {
			egg_debug ("removing %s", filename_tmp);
			g_unlink (filename_tmp);
		}
		g_free (filename_tmp);
	}

	/* success */
	ret = TRUE;

	g_free (working_path);
	g_free (basename);
	return ret;
}

/**
 * gcm_calibrate_argyll_display:
 **/
static gboolean
gcm_calibrate_argyll_display (GcmCalibrate *calibrate, GtkWindow *window, GError **error)
{
	GcmCalibrateArgyll *calibrate_argyll = GCM_CALIBRATE_ARGYLL(calibrate);
	GcmCalibrateArgyllPrivate *priv = calibrate_argyll->priv;
	gboolean ret;
	const gchar *title;
	const gchar *message;

	/* set modal windows up correctly */
	gcm_calibrate_dialog_set_move_window (priv->calibrate_dialog, TRUE);
	gcm_calibrate_dialog_set_window (priv->calibrate_dialog, window);

	/* TRANSLATORS: title, hardware refers to a calibration device */
	title = _("Set up display");

	/* TRANSLATORS: dialog message */
	message = _("Setting up display device for use…");

	/* push new messages into the UI */
	gcm_calibrate_dialog_show (priv->calibrate_dialog, GCM_CALIBRATE_DIALOG_TAB_GENERIC, title, message);
	gcm_calibrate_dialog_set_show_button_ok (priv->calibrate_dialog, FALSE);
	gcm_calibrate_dialog_set_show_expander (priv->calibrate_dialog, FALSE);

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
	ret = gcm_calibrate_argyll_remove_temp_files (calibrate_argyll, error);
	if (!ret)
		goto out;

	/* step 6 */
	ret = gcm_calibrate_argyll_set_filename_result (calibrate_argyll, error);
	if (!ret)
		goto out;
out:
	return ret;
}

/**
 * gcm_calibrate_argyll_spotread_read_chart:
 **/
static gboolean
gcm_calibrate_argyll_spotread_read_chart (GcmCalibrateArgyll *calibrate_argyll, GError **error)
{
	gboolean ret = TRUE;
	GcmCalibrateArgyllPrivate *priv = calibrate_argyll->priv;
	gchar *command = NULL;
	gchar **argv = NULL;
	GPtrArray *array = NULL;
	gchar *basename = NULL;
	gchar *working_path = NULL;
	const gchar *title;
	const gchar *message;

	/* get shared data */
	g_object_get (calibrate_argyll,
		      "basename", &basename,
		      "working-path", &working_path,
		      NULL);

	/* get correct name of the command */
	command = gcm_calibrate_argyll_get_tool_filename ("spotread", error);
	if (command == NULL) {
		ret = FALSE;
		goto out;
	}

	/* TRANSLATORS: title, setting up the photospectromiter */
	title = _("Setting up device");
	/* TRANSLATORS: dialog message */
	message = _("Setting up the device to read a spot color…");

	/* push new messages into the UI */
	gcm_calibrate_dialog_show (priv->calibrate_dialog, GCM_CALIBRATE_DIALOG_TAB_GENERIC, title, message);
	gcm_calibrate_dialog_set_show_button_ok (priv->calibrate_dialog, FALSE);
	gcm_calibrate_dialog_set_show_expander (priv->calibrate_dialog, TRUE);

	/* reset flag so we exit after the single spotread */
	priv->done_spot_read = FALSE;

	/* argument array */
	array = g_ptr_array_new_with_free_func (g_free);

	/* setup the command */
	g_ptr_array_add (array, g_strdup ("-v9"));
	if (priv->done_calibrate)
		g_ptr_array_add (array, g_strdup ("-N"));
	g_ptr_array_add (array, g_strdup (basename));
	argv = gcm_utils_ptr_array_to_strv (array);
	gcm_calibrate_argyll_debug_argv (command, argv);

	/* start up the command */
	priv->state = GCM_CALIBRATE_ARGYLL_STATE_RUNNING;
	vte_terminal_reset (VTE_TERMINAL(priv->terminal), TRUE, FALSE);
	priv->child_pid = vte_terminal_fork_command (VTE_TERMINAL(priv->terminal), command, argv, NULL, working_path, FALSE, FALSE, FALSE);

	/* wait until finished */
	g_main_loop_run (priv->loop);

	/* get result */
	if (priv->response == GTK_RESPONSE_CANCEL) {
		g_set_error_literal (error,
				     GCM_CALIBRATE_ERROR,
				     GCM_CALIBRATE_ERROR_USER_ABORT,
				     "calibration was cancelled");
		ret = FALSE;
		goto out;
	}
	if (priv->response == GTK_RESPONSE_REJECT) {
		gchar *vte_text;
		vte_text = vte_terminal_get_text (VTE_TERMINAL(priv->terminal), NULL, NULL, NULL);
		g_set_error (error,
			     GCM_CALIBRATE_ERROR,
			     GCM_CALIBRATE_ERROR_INTERNAL,
			     "command failed to run successfully: %s", vte_text);
		g_free (vte_text);
		ret = FALSE;
		goto out;
	}
out:
	if (array != NULL)
		g_ptr_array_unref (array);
	g_free (working_path);
	g_free (basename);
	g_free (command);
	g_strfreev (argv);
	return ret;
}

/**
 * gcm_calibrate_argyll_spotread:
 **/
static gboolean
gcm_calibrate_argyll_spotread (GcmCalibrate *calibrate, GtkWindow *window, GError **error)
{
	GcmCalibrateArgyll *calibrate_argyll = GCM_CALIBRATE_ARGYLL(calibrate);
	GcmCalibrateArgyllPrivate *priv = calibrate_argyll->priv;
	gboolean ret;

	/* set modal windows up correctly */
	gcm_calibrate_dialog_set_move_window (priv->calibrate_dialog, FALSE);
	gcm_calibrate_dialog_set_window (priv->calibrate_dialog, window);

	/* step 3 */
	ret = gcm_calibrate_argyll_spotread_read_chart (calibrate_argyll, error);
	if (!ret)
		goto out;

	/* step 5 */
	ret = gcm_calibrate_argyll_remove_temp_files (calibrate_argyll, error);
	if (!ret)
		goto out;
out:
	return ret;
}

/**
 * gcm_calibrate_argyll_get_colorimeter_target:
 **/
static const gchar *
gcm_calibrate_argyll_get_colorimeter_target (GcmCalibrateArgyll *calibrate_argyll)
{
	GcmColorimeterKind colorimeter_kind;

	g_object_get (calibrate_argyll,
		      "colorimeter-kind", &colorimeter_kind,
		      NULL);

	if (colorimeter_kind == GCM_COLORIMETER_KIND_DTP20)
		return "20";
	if (colorimeter_kind == GCM_COLORIMETER_KIND_DTP22)
		return "22";
	if (colorimeter_kind == GCM_COLORIMETER_KIND_DTP41)
		return "41";
	if (colorimeter_kind == GCM_COLORIMETER_KIND_DTP51)
		return "51";
	if (colorimeter_kind == GCM_COLORIMETER_KIND_SPECTRO_SCAN)
		return "SS";
	if (colorimeter_kind == GCM_COLORIMETER_KIND_I1_PRO)
		return "i1";
	if (colorimeter_kind == GCM_COLORIMETER_KIND_COLOR_MUNKI)
		return "CM";
	return NULL;
}

/**
 * gcm_calibrate_argyll_display_generate_targets:
 **/
static gboolean
gcm_calibrate_argyll_display_generate_targets (GcmCalibrateArgyll *calibrate_argyll, gdouble width, gdouble height, GError **error)
{
	gboolean ret = TRUE;
	GcmCalibrateArgyllPrivate *priv = calibrate_argyll->priv;
	gchar *command = NULL;
	gchar **argv = NULL;
	GPtrArray *array = NULL;
	gchar *basename = NULL;
	gchar *working_path = NULL;
	const gchar *title;
	const gchar *message;
	GcmColorimeterKind colorimeter_kind;

	/* get shared data */
	g_object_get (calibrate_argyll,
		      "basename", &basename,
		      "working-path", &working_path,
		      "colorimeter-kind", &colorimeter_kind,
		      NULL);

	/* get correct name of the command */
	command = gcm_calibrate_argyll_get_tool_filename ("printtarg", error);
	if (command == NULL) {
		ret = FALSE;
		goto out;
	}

	/* TRANSLATORS: title, patches are specific colors used in calibration */
	title = _("Printing patches");

	/* TRANSLATORS: dialog message */
	message = _("Rendering the patches for the selected paper and ink.");

	/* push new messages into the UI */
	gcm_calibrate_dialog_show (priv->calibrate_dialog, GCM_CALIBRATE_DIALOG_TAB_GENERIC, title, message);
	gcm_calibrate_dialog_set_show_button_ok (priv->calibrate_dialog, FALSE);
	gcm_calibrate_dialog_set_show_expander (priv->calibrate_dialog, TRUE);

	/* argument array */
	array = g_ptr_array_new_with_free_func (g_free);

	/* setup the command */
	g_ptr_array_add (array, g_strdup ("-v"));

	/* target instrument */
	g_ptr_array_add (array, g_strdup_printf ("-i%s", gcm_calibrate_argyll_get_colorimeter_target (calibrate_argyll)));

	/* use double density */
	if (colorimeter_kind == GCM_COLORIMETER_KIND_COLOR_MUNKI ||
	    colorimeter_kind == GCM_COLORIMETER_KIND_SPECTRO_SCAN) {
		g_ptr_array_add (array, g_strdup ("-h"));
	}

	/* 8 bit TIFF 300 dpi */
	g_ptr_array_add (array, g_strdup ("-t300"));

	/* paper size */
	g_ptr_array_add (array, g_strdup_printf ("-p%ix%i", (gint) width, (gint) height));

	g_ptr_array_add (array, g_strdup (basename));
	argv = gcm_utils_ptr_array_to_strv (array);
	gcm_calibrate_argyll_debug_argv (command, argv);

	/* start up the command */
	priv->state = GCM_CALIBRATE_ARGYLL_STATE_RUNNING;
	vte_terminal_reset (VTE_TERMINAL(priv->terminal), TRUE, FALSE);
	priv->child_pid = vte_terminal_fork_command (VTE_TERMINAL(priv->terminal), command, argv, NULL, working_path, FALSE, FALSE, FALSE);

	/* wait until finished */
	g_main_loop_run (priv->loop);

	/* get result */
	if (priv->response == GTK_RESPONSE_CANCEL) {
		g_set_error_literal (error,
				     GCM_CALIBRATE_ERROR,
				     GCM_CALIBRATE_ERROR_USER_ABORT,
				     "target generation was cancelled");
		ret = FALSE;
		goto out;
	}
	if (priv->response == GTK_RESPONSE_REJECT) {
		gchar *vte_text;
		vte_text = vte_terminal_get_text (VTE_TERMINAL(priv->terminal), NULL, NULL, NULL);
		g_set_error (error,
			     GCM_CALIBRATE_ERROR,
			     GCM_CALIBRATE_ERROR_INTERNAL,
			     "command failed to run successfully: %s", vte_text);
		g_free (vte_text);
		ret = FALSE;
		goto out;
	}
out:
	if (array != NULL)
		g_ptr_array_unref (array);
	g_free (working_path);
	g_free (basename);
	g_free (command);
	g_strfreev (argv);
	return ret;
}

/**
 * gcm_calibrate_argyll_render_cb:
 **/
static GPtrArray *
gcm_calibrate_argyll_render_cb (GcmPrint *print, GtkPageSetup *page_setup, GcmCalibrate *calibrate, GError **error)
{
	gboolean ret = TRUE;
	GPtrArray *array = NULL;
	gdouble width, height;
	GtkPaperSize *paper_size;
	GDir *dir = NULL;
	const gchar *filename;
	gchar *basename = NULL;
	gchar *filename_tmp;
	gchar *working_path = NULL;

	/* get shared data */
	g_object_get (calibrate,
		      "basename", &basename,
		      "working-path", &working_path,
		      NULL);

	paper_size = gtk_page_setup_get_paper_size (page_setup);
	width = gtk_paper_size_get_width (paper_size, GTK_UNIT_MM);
	height = gtk_paper_size_get_height (paper_size, GTK_UNIT_MM);

	/* generate the tiff files */
	ret = gcm_calibrate_argyll_display_generate_targets (GCM_CALIBRATE_ARGYLL (calibrate), width, height, error);
	if (!ret)
		goto out;

	/* list files */
	dir = g_dir_open (working_path, 0, error);
	if (dir == NULL)
		goto out;

	/* read in each file */
	array = g_ptr_array_new_with_free_func (g_free);
	filename = g_dir_read_name (dir);
	while (filename != NULL) {
		if (g_str_has_prefix (filename, basename) &&
		    g_str_has_suffix (filename, ".tif")) {
			filename_tmp = g_build_filename (working_path, filename, NULL);
			egg_debug ("add print page %s", filename_tmp);
			g_ptr_array_add (array, filename_tmp);
		}
		filename = g_dir_read_name (dir);
	}
out:
	g_free (working_path);
	g_free (basename);
	if (dir != NULL)
		g_dir_close (dir);
	return array;
}

/**
 * gcm_calibrate_argyll_set_device_from_ti2:
 **/
static gboolean
gcm_calibrate_argyll_set_device_from_ti2 (GcmCalibrate *calibrate, const gchar *filename, GError **error)
{
	gboolean ret;
	gchar *contents = NULL;
	gchar *device = NULL;
	const gchar *device_ptr = NULL;
	gchar **lines = NULL;
	gint i;

	/* get the contents of the file */
	ret = g_file_get_contents (filename, &contents, NULL, error);
	if (!ret)
		goto out;

	/* find the data */
	lines = g_strsplit (contents, "\n", -1);
	for (i=0; lines[i] != NULL; i++) {
		if (g_str_has_prefix (lines[i], "TARGET_INSTRUMENT")) {
			device = g_strdup (lines[i] + 18);
			g_strdelimit (device, "\"", ' ');
			g_strstrip (device);
			break;
		}
	}

	/* set the calibration model */
	if (device == NULL) {
		g_set_error (error,
			     GCM_CALIBRATE_ERROR,
			     GCM_CALIBRATE_ERROR_NO_DATA,
			     "cannot find TARGET_INSTRUMENT in %s", filename);
		ret = FALSE;
		goto out;
	}

	/* remove vendor prefix */
	if (g_str_has_prefix (device, "X-Rite "))
		device_ptr = device + 7;

	/* set for calibration */
	egg_debug ("setting instrument to '%s'", device_ptr);
	g_object_set (calibrate,
		      "device", device_ptr,
		      NULL);
out:
	g_free (device);
	g_free (contents);
	g_strfreev (lines);
	return ret;
}

/**
 * gcm_calibrate_argyll_get_paper_size:
 **/
static GtkPaperSize *
gcm_calibrate_argyll_get_paper_size (GcmCalibrate *calibrate, GtkWindow *window)
{
	GtkPrintSettings *settings;
	GtkPageSetup *page_setup;
	GtkPaperSize *paper_size = NULL;

	/* find out the paper size */
	settings = gtk_print_settings_new ();
	page_setup = gtk_print_run_page_setup_dialog (window, NULL, settings);
	if (page_setup == NULL)
		goto out;

	/* get paper size */
	paper_size = gtk_page_setup_get_paper_size (page_setup);
out:
	g_object_unref (settings);
	return paper_size;
}

/**
 * gcm_calibrate_argyll_printer_convert_jpeg:
 **/
static gboolean
gcm_calibrate_argyll_printer_convert_jpeg (GcmCalibrateArgyll *calibrate_argyll, GError **error)
{
	GDir *dir;
	const gchar *filename;
	gchar *filename_tiff;
	gchar *filename_jpg;
	guint len;
	gboolean ret = TRUE;
	gchar *working_path = NULL;
	GdkPixbuf *pixbuf;

	/* need to ask if we are printing now, or using old data */
	g_object_get (calibrate_argyll,
		      "working-path", &working_path,
		      NULL);

	dir = g_dir_open (working_path, 0, error);
	if (dir == NULL) {
		ret = FALSE;
		goto out;
	}

	filename = g_dir_read_name (dir);
	while (filename != NULL) {
		if (g_str_has_suffix (filename, ".tif")) {

			/* get both files */
			filename_tiff = g_build_filename (working_path, filename, NULL);
			filename_jpg = g_strdup (filename_tiff);

			/* replace ending */
			len = strlen (filename_jpg);
			g_strlcpy (filename_jpg+len-4, ".jpg", 5);

			/* convert from tiff to jpg */
			egg_debug ("convert %s to %s", filename_tiff, filename_jpg);
			pixbuf = gdk_pixbuf_new_from_file (filename_tiff, error);
			if (pixbuf == NULL) {
				ret = FALSE;
				goto out;
			}

			/* try to save new file */
			ret = gdk_pixbuf_save (pixbuf, filename_jpg, "jpeg", error, "quality", "100", NULL);
			if (!ret)
				goto out;
			g_object_unref (pixbuf);
			g_free (filename_tiff);
			g_free (filename_jpg);
		}
		filename = g_dir_read_name (dir);
	}
out:
	if (dir != NULL)
		g_dir_close (dir);
	g_free (working_path);
	return ret;
}

/**
 * gcm_calibrate_argyll_printer:
 **/
static gboolean
gcm_calibrate_argyll_printer (GcmCalibrate *calibrate, GtkWindow *window, GError **error)
{
	gboolean ret;
	gchar *cmdline = NULL;
	gchar *filename = NULL;
	gchar *working_path = NULL;
	gchar *basename = NULL;
	GtkPaperSize *paper_size;
	const gchar *title;
	const gchar *message;
	gdouble width, height;
	GtkResponseType response;
	GcmCalibratePrintKind print_kind;
	GcmCalibrateArgyll *calibrate_argyll = GCM_CALIBRATE_ARGYLL(calibrate);
	GcmCalibrateArgyllPrivate *priv = calibrate_argyll->priv;

	/* need to ask if we are printing now, or using old data */
	g_object_get (calibrate,
		      "basename", &basename,
		      "print-kind", &print_kind,
		      "working-path", &working_path,
		      NULL);

	/* set modal windows up correctly */
	gcm_calibrate_dialog_set_move_window (priv->calibrate_dialog, FALSE);
	gcm_calibrate_dialog_set_window (priv->calibrate_dialog, window);

	/* step 1 */
	if (print_kind == GCM_CALIBRATE_PRINT_KIND_LOCAL ||
	    print_kind == GCM_CALIBRATE_PRINT_KIND_GENERATE) {
		ret = gcm_calibrate_argyll_display_generate_patches (calibrate_argyll, error);
		if (!ret)
			goto out;
	}

	/* print */
	if (print_kind == GCM_CALIBRATE_PRINT_KIND_LOCAL) {
		window = gcm_calibrate_dialog_get_window (priv->calibrate_dialog);
		ret = gcm_print_with_render_callback (priv->print, window, (GcmPrintRenderCb) gcm_calibrate_argyll_render_cb, calibrate, error);
		if (!ret)
			goto out;
	}

	/* page setup, and then we're done */
	if (print_kind == GCM_CALIBRATE_PRINT_KIND_GENERATE) {

		/* get the paper size */
		paper_size = gcm_calibrate_argyll_get_paper_size (calibrate, window);
		width = gtk_paper_size_get_width (paper_size, GTK_UNIT_MM);
		height = gtk_paper_size_get_height (paper_size, GTK_UNIT_MM);

		/* generate the tiff files */
		ret = gcm_calibrate_argyll_display_generate_targets (GCM_CALIBRATE_ARGYLL (calibrate), width, height, error);
		if (!ret)
			goto out;

		/* convert to jpegs */
		ret = gcm_calibrate_argyll_printer_convert_jpeg (GCM_CALIBRATE_ARGYLL (calibrate), error);
		if (!ret)
			goto out;

		cmdline = g_strdup_printf ("nautilus \"%s\"", working_path);
		egg_debug ("we need to open the directory we're using: %s", cmdline);
		ret = g_spawn_command_line_async (cmdline, error);
		goto out;
	}

	/* wait */
	if (print_kind == GCM_CALIBRATE_PRINT_KIND_LOCAL) {
		/* TRANSLATORS: title, patches are specific colours used in calibration */
		title = _("Wait for the ink to dry");

		/* TRANSLATORS: dialog message */
		message = _("Please wait a few minutes for the ink to dry. Profiling damp ink will produce a poor profile and may damage your color measuring instrument.");

		/* push new messages into the UI */
		gcm_calibrate_dialog_show (priv->calibrate_dialog, GCM_CALIBRATE_DIALOG_TAB_GENERIC, title, message);
		gcm_calibrate_dialog_set_show_button_ok (priv->calibrate_dialog, TRUE);
		gcm_calibrate_dialog_set_show_expander (priv->calibrate_dialog, FALSE);
		gcm_calibrate_dialog_set_image_filename (priv->calibrate_dialog, "clock.svg");
		response = gcm_calibrate_dialog_run (priv->calibrate_dialog);
		if (response != GTK_RESPONSE_OK) {
			gcm_calibrate_dialog_hide (priv->calibrate_dialog);
			g_set_error_literal (error,
					     GCM_CALIBRATE_ERROR,
					     GCM_CALIBRATE_ERROR_USER_ABORT,
					     "user did not wait for ink to dry");
			ret = FALSE;
			goto out;
		}
	}

	/* we need to read the ti2 file to set the device used for calibration */
	if (print_kind == GCM_CALIBRATE_PRINT_KIND_ANALYZE) {
		filename = g_strdup_printf ("%s/%s.ti2", working_path, basename);
		ret = g_file_test (filename, G_FILE_TEST_EXISTS);
		if (!ret) {
			gcm_calibrate_dialog_hide (priv->calibrate_dialog);
			g_set_error (error,
				     GCM_CALIBRATE_ERROR,
				     GCM_CALIBRATE_ERROR_NO_DATA,
				     "cannot find %s", filename);
			goto out;
		}
		ret = gcm_calibrate_argyll_set_device_from_ti2 (calibrate, filename, error);
		if (!ret)
			goto out;
	}

	/* step 3 */
	ret = gcm_calibrate_argyll_display_read_chart (calibrate_argyll, error);
	if (!ret)
		goto out;

	/* step 4 */
	ret = gcm_calibrate_argyll_device_generate_profile (calibrate_argyll, error);
	if (!ret)
		goto out;

	/* only delete state if we are doing a local printer */
	if (print_kind == GCM_CALIBRATE_PRINT_KIND_LOCAL) {
		/* step 5 */
		ret = gcm_calibrate_argyll_remove_temp_files (calibrate_argyll, error);
		if (!ret)
			goto out;
	}

	/* step 6 */
	ret = gcm_calibrate_argyll_set_filename_result (calibrate_argyll, error);
	if (!ret)
		goto out;
out:
	g_free (filename);
	g_free (basename);
	g_free (cmdline);
	g_free (working_path);
	return ret;
}

/**
 * gcm_calibrate_argyll_pixbuf_remove_alpha:
 **/
static GdkPixbuf *
gcm_calibrate_argyll_pixbuf_remove_alpha (const GdkPixbuf *pixbuf)
{
	GdkPixbuf *new_pixbuf = NULL;
	gint x, y;
	guchar *src, *dest;

	/* already no alpha */
	if (!gdk_pixbuf_get_has_alpha (pixbuf)) {
		new_pixbuf = gdk_pixbuf_copy (pixbuf);
		goto out;
	}

	/* create new image, and copy RGB */
	new_pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8,
				     gdk_pixbuf_get_width (pixbuf),
				     gdk_pixbuf_get_height (pixbuf));
	for (y = 0; y < gdk_pixbuf_get_height (pixbuf); y++) {
		src = gdk_pixbuf_get_pixels (pixbuf) + y * gdk_pixbuf_get_rowstride (pixbuf);
		dest = gdk_pixbuf_get_pixels (new_pixbuf) + y * gdk_pixbuf_get_rowstride (new_pixbuf);

		/* copy RGB from RGBA */
		for (x = 0; x < gdk_pixbuf_get_width (pixbuf); x++) {
			dest[0] = src[0];
			dest[1] = src[1];
			dest[2] = src[2];
			src += 4;
			dest += 3;
		}
	}
out:
	return new_pixbuf;
}

/**
 * gcm_calibrate_argyll_check_and_remove_alpha:
 **/
static gboolean
gcm_calibrate_argyll_check_and_remove_alpha (GcmCalibrateArgyll *calibrate_argyll, GError **error)
{
	gboolean ret = TRUE;
	GdkPixbuf *pixbuf = NULL;
	GdkPixbuf *pixbuf_new = NULL;
	gchar *reference_image = NULL;
	gchar *basename = NULL;
	gchar *working_path = NULL;
	gchar *filename = NULL;
	const gchar *title;
	GString *string = NULL;
	GtkResponseType response;
	GcmCalibrateArgyllPrivate *priv = calibrate_argyll->priv;

	/* get shared data */
	g_object_get (calibrate_argyll,
		      "basename", &basename,
		      "working-path", &working_path,
		      NULL);

	/* get copied filename */
	filename = g_strdup_printf ("%s.tif", basename);
	reference_image = g_build_filename (working_path, filename, NULL);

	/* check to see if the file has any alpha channel */
	pixbuf = gdk_pixbuf_new_from_file (reference_image, error);
	if (pixbuf == NULL) {
		ret = FALSE;
		goto out;
	}

	/* plain RGB */
	if (!gdk_pixbuf_get_has_alpha (pixbuf))
		goto out;

	/* TRANSLATORS: the supplied image contains an alpha channel which we have to strip out */
	title = _("Image is not suitable without conversion");

	/* TRANSLATORS: dialog message */
	string = g_string_new (_("The supplied image contains an alpha channel which the profiling tools do not understand."));
	g_string_append (string, "\n\n");

	/* TRANSLATORS: dialog message */
	g_string_append (string, _("It is normally safe to convert the image, although you should ensure that the generated profile is valid."));

	/* push new messages into the UI */
	gcm_calibrate_dialog_show (priv->calibrate_dialog, GCM_CALIBRATE_DIALOG_TAB_GENERIC, title, string->str);
	gcm_calibrate_dialog_set_show_button_ok (priv->calibrate_dialog, TRUE);
	gcm_calibrate_dialog_set_show_expander (priv->calibrate_dialog, FALSE);
	/* TRANSLATORS: button text to convert the RGBA image into RGB */
	gcm_calibrate_dialog_set_button_ok_id (priv->calibrate_dialog, _("Convert"));
	response = gcm_calibrate_dialog_run (priv->calibrate_dialog);
	if (response != GTK_RESPONSE_OK) {
		gcm_calibrate_dialog_hide (priv->calibrate_dialog);
		g_set_error_literal (error,
				     GCM_CALIBRATE_ERROR,
				     GCM_CALIBRATE_ERROR_USER_ABORT,
				     "user did not convert RGBA into RGB");
		ret = FALSE;
		goto out;
	}

	/* remove the alpha channel */
	pixbuf_new = gcm_calibrate_argyll_pixbuf_remove_alpha (pixbuf);
	if (pixbuf_new == NULL) {
		g_set_error_literal (error,
				     GCM_CALIBRATE_ERROR,
				     GCM_CALIBRATE_ERROR_INTERNAL,
				     "failed to remove alpha channel");
		ret = FALSE;
		goto out;
	}

	/* save */
	ret = gdk_pixbuf_save (pixbuf_new, reference_image, "tiff", error, NULL);
	if (!ret)
		goto out;

out:
	g_free (working_path);
	g_free (filename);
	g_free (basename);
	g_free (reference_image);
	if (string != NULL)
		g_string_free (string, TRUE);
	if (pixbuf != NULL)
		g_object_unref (pixbuf);
	if (pixbuf_new != NULL)
		g_object_unref (pixbuf_new);
	return ret;
}

/**
 * gcm_calibrate_argyll_device:
 **/
static gboolean
gcm_calibrate_argyll_device (GcmCalibrate *calibrate, GtkWindow *window, GError **error)
{
	GcmCalibrateArgyll *calibrate_argyll = GCM_CALIBRATE_ARGYLL(calibrate);
	GcmCalibrateArgyllPrivate *priv = calibrate_argyll->priv;
	gboolean ret;
	const gchar *title;
	const gchar *message;

	/* set modal windows up correctly */
	gcm_calibrate_dialog_set_move_window (priv->calibrate_dialog, FALSE);
	gcm_calibrate_dialog_set_window (priv->calibrate_dialog, window);

	/* TRANSLATORS: title, instrument refers to a calibration device */
	title = _("Set up instrument");

	/* TRANSLATORS: dialog message */
	message = _("Setting up the instrument for use…");

	/* push new messages into the UI */
	gcm_calibrate_dialog_show (priv->calibrate_dialog, GCM_CALIBRATE_DIALOG_TAB_GENERIC, title, message);
	gcm_calibrate_dialog_set_show_button_ok (priv->calibrate_dialog, FALSE);
	gcm_calibrate_dialog_set_show_expander (priv->calibrate_dialog, FALSE);

	/* step 1 */
	ret = gcm_calibrate_argyll_device_copy (calibrate_argyll, error);
	if (!ret)
		goto out;

	/* step 1.5 */
	ret = gcm_calibrate_argyll_check_and_remove_alpha (calibrate_argyll, error);
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
	ret = gcm_calibrate_argyll_remove_temp_files (calibrate_argyll, error);
	if (!ret)
		goto out;

	/* step 5 */
	ret = gcm_calibrate_argyll_set_filename_result (calibrate_argyll, error);
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
 * gcm_calibrate_argyll_interaction_attach:
 **/
static void
gcm_calibrate_argyll_interaction_attach (GcmCalibrateArgyll *calibrate_argyll)
{
	const gchar *title;
	const gchar *message;
	const gchar *filename;
	GcmCalibrateArgyllPrivate *priv = calibrate_argyll->priv;

	/* different tools assume the device is not on the screen */
	if (priv->already_on_window) {
		egg_debug ("VTE: already on screen so faking keypress");
		g_timeout_add_seconds (1, (GSourceFunc) gcm_calibrate_argyll_timeout_cb, calibrate_argyll);
		goto out;
	}

	/* TRANSLATORS: title, instrument is a hardware color calibration sensor */
	title = _("Please attach instrument");

	/* get the image, if we have one */
	filename = gcm_calibrate_argyll_get_colorimeter_image_attach (calibrate_argyll);

	/* different messages with or without image */
	if (filename != NULL) {
		/* TRANSLATORS: dialog message, ask user to attach device, and there's an example image */
		message = _("Please attach the measuring instrument to the center of the screen on the gray square like the image below.");
	} else {
		/* TRANSLATORS: dialog message, ask user to attach device */
		message = _("Please attach the measuring instrument to the center of the screen on the gray square.");
	}

	/* block for a response */
	egg_debug ("blocking waiting for user input: %s", title);

	/* push new messages into the UI */
	gcm_calibrate_dialog_show (priv->calibrate_dialog, GCM_CALIBRATE_DIALOG_TAB_GENERIC, title, message);
	gcm_calibrate_dialog_set_show_button_ok (priv->calibrate_dialog, TRUE);
	gcm_calibrate_dialog_set_image_filename (priv->calibrate_dialog, filename);
	gcm_calibrate_dialog_set_show_expander (priv->calibrate_dialog, FALSE);

	/* TRANSLATORS: button text */
	gcm_calibrate_dialog_set_button_ok_id (priv->calibrate_dialog, _("Continue"));

	/* set state */
	priv->argyllcms_ok = " ";
	priv->state = GCM_CALIBRATE_ARGYLL_STATE_WAITING_FOR_STDIN,

	/* play sound from the naming spec */
	ca_context_play (ca_gtk_context_get (), 0,
			 CA_PROP_EVENT_ID, "dialog-information",
			 /* TRANSLATORS: this is the application name for libcanberra */
			 CA_PROP_APPLICATION_NAME, _("GNOME Color Manager"),
			 CA_PROP_EVENT_DESCRIPTION, message, NULL);

	/* save as we know the device is on the screen now */
	priv->already_on_window = TRUE;
out:
	return;
}

/**
 * gcm_calibrate_argyll_interaction_calibrate:
 **/
static void
gcm_calibrate_argyll_interaction_calibrate (GcmCalibrateArgyll *calibrate_argyll)
{
	const gchar *title;
	const gchar *message;
	const gchar *filename;
	GcmCalibrateArgyllPrivate *priv = calibrate_argyll->priv;

	/* TRANSLATORS: title, instrument is a hardware color calibration sensor */
	title = _("Please configure instrument");

	/* block for a response */
	egg_debug ("blocking waiting for user input: %s", title);

	/* get the image, if we have one */
	filename = gcm_calibrate_argyll_get_colorimeter_image_calibrate (calibrate_argyll);

	if (filename != NULL) {
		/* TRANSLATORS: this is when the user has to change a setting on the sensor, and we're showing a picture */
		message = _("Please set the measuring instrument to calibration mode like the image below.");
	} else {
		/* TRANSLATORS: this is when the user has to change a setting on the sensor */
		message = _("Please set the measuring instrument to calibration mode.");
	}

	/* push new messages into the UI */
	gcm_calibrate_dialog_show (priv->calibrate_dialog, GCM_CALIBRATE_DIALOG_TAB_GENERIC, title, message);
	gcm_calibrate_dialog_set_show_button_ok (priv->calibrate_dialog, TRUE);
	gcm_calibrate_dialog_set_image_filename (priv->calibrate_dialog, filename);
	gcm_calibrate_dialog_set_show_expander (priv->calibrate_dialog, TRUE);

	/* TRANSLATORS: button text */
	gcm_calibrate_dialog_set_button_ok_id (priv->calibrate_dialog, _("Continue"));

	/* play sound from the naming spec */
	ca_context_play (ca_gtk_context_get (), 0,
			 CA_PROP_EVENT_ID, "dialog-information",
			 /* TRANSLATORS: this is the application name for libcanberra */
			 CA_PROP_APPLICATION_NAME, _("GNOME Color Manager"),
			 CA_PROP_EVENT_DESCRIPTION, message, NULL);

	/* assume it's no longer on the window */
	priv->already_on_window = FALSE;

	/* assume it was done correctly */
	priv->done_calibrate = TRUE;

	/* set state */
	priv->argyllcms_ok = " ";
	priv->state = GCM_CALIBRATE_ARGYLL_STATE_WAITING_FOR_STDIN;
}

/**
 * gcm_calibrate_argyll_interaction_surface:
 **/
static void
gcm_calibrate_argyll_interaction_surface (GcmCalibrateArgyll *calibrate_argyll)
{
	const gchar *title;
	const gchar *message;
	const gchar *filename;
	GcmCalibrateArgyllPrivate *priv = calibrate_argyll->priv;

	/* TRANSLATORS: title, instrument is a hardware color calibration sensor */
	title = _("Please configure instrument");

	/* block for a response */
	egg_debug ("blocking waiting for user input: %s", title);

	/* get the image, if we have one */
	filename = gcm_calibrate_argyll_get_colorimeter_image_screen (calibrate_argyll);

	if (filename != NULL) {
		/* TRANSLATORS: this is when the user has to change a setting on the sensor, and we're showing a picture */
		message = _("Please set the measuring instrument to screen mode like the image below.");
	} else {
		/* TRANSLATORS: this is when the user has to change a setting on the sensor */
		message = _("Please set the measuring instrument to screen mode.");
	}

	/* push new messages into the UI */
	gcm_calibrate_dialog_show (priv->calibrate_dialog, GCM_CALIBRATE_DIALOG_TAB_GENERIC, title, message);
	gcm_calibrate_dialog_set_show_button_ok (priv->calibrate_dialog, TRUE);
	gcm_calibrate_dialog_set_image_filename (priv->calibrate_dialog, filename);
	gcm_calibrate_dialog_set_show_expander (priv->calibrate_dialog, TRUE);

	/* TRANSLATORS: button text */
	gcm_calibrate_dialog_set_button_ok_id (priv->calibrate_dialog, _("Continue"));

	/* play sound from the naming spec */
	ca_context_play (ca_gtk_context_get (), 0,
			 CA_PROP_EVENT_ID, "dialog-information",
			 /* TRANSLATORS: this is the application name for libcanberra */
			 CA_PROP_APPLICATION_NAME, _("GNOME Color Manager"),
			 CA_PROP_EVENT_DESCRIPTION, message, NULL);

	/* assume it's no longer on the window */
	priv->already_on_window = FALSE;

	/* set state */
	priv->argyllcms_ok = " ";
	priv->state = GCM_CALIBRATE_ARGYLL_STATE_WAITING_FOR_STDIN;
}

/**
 * gcm_calibrate_argyll_process_output_cmd:
 *
 * Return value: if FALSE then abort processing input
 **/
static gboolean
gcm_calibrate_argyll_process_output_cmd (GcmCalibrateArgyll *calibrate_argyll, const gchar *line)
{
	const gchar *title;
	gchar *title_str = NULL;
	const gchar *message;
	GString *string = NULL;
	gchar *found;
	gchar **split = NULL;
	gboolean ret = TRUE;
	GcmCalibrateArgyllPrivate *priv = calibrate_argyll->priv;

	/* attach device */
	if (g_strcmp0 (line, "Place instrument on test window.") == 0) {
		egg_debug ("VTE: interaction required: %s", line);
		gcm_calibrate_argyll_interaction_attach (calibrate_argyll);
		ret = FALSE;
		goto out;
	}

	/* set to calibrate */
	if (g_strcmp0 (line, "Set instrument sensor to calibration position,") == 0) {
		egg_debug ("VTE: interaction required, set to calibrate");
		gcm_calibrate_argyll_interaction_calibrate (calibrate_argyll);
		ret = FALSE;
		goto out;
	}

	/* set to calibrate */
	if (g_strcmp0 (line, "(Sensor should be in surface position)") == 0) {
		egg_debug ("VTE: interaction required, set to surface");
		gcm_calibrate_argyll_interaction_surface (calibrate_argyll);
		ret = FALSE;
		goto out;
	}

	/* something went wrong with a measurement */
	if (g_strstr_len (line, -1, "Measurement misread") != NULL) {
		/* TRANSLATORS: title, the calibration failed */
		title = _("Calibration error");

		/* TRANSLATORS: message, the sample was not read correctly */
		message = _("The sample could not be read at this time.");

		/* push new messages into the UI */
		gcm_calibrate_dialog_show (priv->calibrate_dialog, GCM_CALIBRATE_DIALOG_TAB_GENERIC, title, message);
		gcm_calibrate_dialog_set_show_button_ok (priv->calibrate_dialog, TRUE);
		gcm_calibrate_dialog_set_show_expander (priv->calibrate_dialog, TRUE);

		/* TRANSLATORS: button text */
		gcm_calibrate_dialog_set_button_ok_id (priv->calibrate_dialog, _("Try again"));

		/* set state */
		priv->argyllcms_ok = " ";
		priv->state = GCM_CALIBRATE_ARGYLL_STATE_WAITING_FOR_STDIN;

		/* play sound from the naming spec */
		ca_context_play (ca_gtk_context_get (), 0,
				 CA_PROP_EVENT_ID, "dialog-warning",
				 /* TRANSLATORS: this is the application name for libcanberra */
				 CA_PROP_APPLICATION_NAME, _("GNOME Color Manager"),
				 CA_PROP_EVENT_DESCRIPTION, message, NULL);
		goto out;
	}

	/* lines we're ignoring */
	if (g_strcmp0 (line, "Q") == 0 ||
	    g_strcmp0 (line, "Sample read stopped at user request!") == 0 ||
	    g_strcmp0 (line, "Hit Esc or Q to give up, any other key to retry:") == 0 ||
	    g_strcmp0 (line, "Correct position then hit Esc or Q to give up, any other key to retry:") == 0 ||
	    g_strcmp0 (line, "Calibration complete") == 0 ||
	    g_strcmp0 (line, "Spot read failed due to the sensor being in the wrong position") == 0 ||
	    g_strcmp0 (line, "and then hit any key to continue,") == 0 ||
	    g_strcmp0 (line, "or hit Esc or Q to abort:") == 0 ||
	    g_strcmp0 (line, "The instrument can be removed from the screen.") == 0 ||
	    g_strstr_len (line, -1, "User Aborted") != NULL ||
	    g_str_has_prefix (line, "Perspective correction factors") ||
	    g_str_has_suffix (line, "key to continue:")) {
		egg_debug ("VTE: ignore: %s", line);
		goto out;
	}

	/* spot read result */
	found = g_strstr_len (line, -1, "Result is XYZ");
	if (found != NULL) {
		GcmXyz *xyz;
		egg_warning ("line=%s", line);
		split = g_strsplit (line, " ", -1);
		xyz = gcm_xyz_new ();
		g_object_set (xyz,
			      "cie-x", g_ascii_strtod (split[4], NULL),
			      "cie-y", g_ascii_strtod (split[5], NULL),
			      "cie-z", g_ascii_strtod (split[6], NULL),
			      NULL);
		g_object_set (calibrate_argyll,
			      "xyz", xyz,
			      NULL);
		priv->done_spot_read = TRUE;
		gcm_calibrate_dialog_response (priv->calibrate_dialog, GTK_RESPONSE_CANCEL);
		g_object_unref (xyz);
		goto out;
	}

	/* error */
	found = g_strstr_len (line, -1, "Error - ");
	if (found != NULL) {

		/* TRANSLATORS: title, the calibration failed */
		title = _("Calibration error");

		if (g_strstr_len (line, -1, "No PLD firmware pattern is available") != NULL) {
			/* TRANSLATORS: message, no firmware is available */
			message = _("No firmware is installed for this instrument.");
		} else if (g_strstr_len (line, -1, "Pattern match wasn't good enough") != NULL) {
			/* TRANSLATORS: message, the image wasn't good enough */
			message = _("The pattern match wasn't good enough. Ensure you have the correct type of target selected.");
		} else if (g_strstr_len (line, -1, "Aprox. fwd matrix unexpectedly singular") != NULL ||
			   g_strstr_len (line, -1, "Inverting aprox. fwd matrix failed") != NULL) {
			/* TRANSLATORS: message, the sensor got no readings */
			message = _("The measuring instrument got no valid readings. Please ensure the aperture is fully open.");
		} else if (g_strstr_len (line, -1, "Device or resource busy") != NULL) {
			/* TRANSLATORS: message, the colorimeter has got confused */
			message = _("The measuring instrument is busy and is not starting up. Please remove the USB plug and re-insert before trying to use this device.");
		} else {
			message = found + 8;
		}

		/* push new messages into the UI */
		gcm_calibrate_dialog_show (priv->calibrate_dialog, GCM_CALIBRATE_DIALOG_TAB_GENERIC, title, message);
		gcm_calibrate_dialog_set_show_button_ok (priv->calibrate_dialog, FALSE);
		gcm_calibrate_dialog_set_show_expander (priv->calibrate_dialog, TRUE);

		egg_debug ("VTE: error: %s", found+8);

		/* set state */
		priv->state = GCM_CALIBRATE_ARGYLL_STATE_WAITING_FOR_LOOP;

		/* play sound from the naming spec */
		ca_context_play (ca_gtk_context_get (), 0,
				 CA_PROP_EVENT_ID, "dialog-warning",
				 /* TRANSLATORS: this is the application name for libcanberra */
				 CA_PROP_APPLICATION_NAME, _("GNOME Color Manager"),
				 CA_PROP_EVENT_DESCRIPTION, message, NULL);

		/* wait until finished */
		g_main_loop_run (priv->loop);
		goto out;
	}

	/* all done */
	found = g_strstr_len (line, -1, "(All rows read)");
	if (found != NULL) {
		gcm_calibrate_dialog_set_image_filename (priv->calibrate_dialog, "scan-target-good.svg");
		vte_terminal_feed_child (VTE_TERMINAL(priv->terminal), "d", 1);
		goto out;
	}

	/* reading strip */
	if (g_str_has_prefix (line, "Strip read failed due to misread")) {
		/* TRANSLATORS: dialog title */
		title = _("Reading target");

		/* TRANSLATORS: message, no firmware is available */
		message = _("Failed to read the strip correctly.");

		/* push new messages into the UI */
		gcm_calibrate_dialog_show (priv->calibrate_dialog, GCM_CALIBRATE_DIALOG_TAB_GENERIC, title, message);
		gcm_calibrate_dialog_set_show_button_ok (priv->calibrate_dialog, TRUE);
		gcm_calibrate_dialog_set_show_expander (priv->calibrate_dialog, TRUE);
		gcm_calibrate_dialog_set_image_filename (priv->calibrate_dialog, "scan-target-bad.svg");

		/* TRANSLATORS: button text */
		gcm_calibrate_dialog_set_button_ok_id (priv->calibrate_dialog, _("Retry"));

		/* set state */
		priv->argyllcms_ok = " ";
		priv->state = GCM_CALIBRATE_ARGYLL_STATE_WAITING_FOR_STDIN;

		/* play sound from the naming spec */
		ca_context_play (ca_gtk_context_get (), 0,
				 CA_PROP_EVENT_ID, "dialog-warning",
				 /* TRANSLATORS: this is the application name for libcanberra */
				 CA_PROP_APPLICATION_NAME, _("GNOME Color Manager"),
				 CA_PROP_EVENT_DESCRIPTION, message, NULL);
		goto out;
	}

	/* reading strip */
	if (g_str_has_prefix (line, "(Warning) Seem to have read strip pass ")) {

		/* find the strip we read, and the one we wanted */
		split = g_strsplit (line, " ", -1);
		g_strdelimit (split[10], "!", '\0');

		/* TRANSLATORS: dialog title, where %s is a letter like 'A' */
		title_str = g_strdup_printf (_("Read strip %s rather than %s!"), split[7], split[10]);

		string = g_string_new ("");

		/* TRANSLATORS: dialog message, just follow the hardware instructions */
		g_string_append (string, _("It looks like you've measured the wrong strip."));
		g_string_append (string, "\n\n");

		/* TRANSLATORS: dialog message, just follow the hardware instructions */
		g_string_append (string, _("If you've really measured the right one, it's okay, it could just be unusual paper."));
		g_string_append (string, "\n\n");

		/* push new messages into the UI */
		gcm_calibrate_dialog_show (priv->calibrate_dialog, GCM_CALIBRATE_DIALOG_TAB_GENERIC, title_str, string->str);
		gcm_calibrate_dialog_set_show_button_ok (priv->calibrate_dialog, TRUE);
		gcm_calibrate_dialog_set_show_expander (priv->calibrate_dialog, TRUE);
		gcm_calibrate_dialog_set_image_filename (priv->calibrate_dialog, "scan-target-bad.svg");

		/* TRANSLATORS: button */
		gcm_calibrate_dialog_set_button_ok_id (priv->calibrate_dialog, _("Use anyway"));

		/* set state */
		priv->argyllcms_ok = "\n";
		priv->state = GCM_CALIBRATE_ARGYLL_STATE_WAITING_FOR_STDIN;
		goto out;
	}

	/* reading spot */
	if (g_str_has_prefix (line, "Place instrument on spot to be measured")) {
		if (!priv->done_spot_read)
			vte_terminal_feed_child (VTE_TERMINAL(priv->terminal), " ", 1);
		gcm_calibrate_dialog_hide (priv->calibrate_dialog);
		goto out;
	}


	/* reading strip */
	if (g_str_has_prefix (line, "Spot read failed due to misread")) {

		/* TRANSLATORS: title, the calibration failed */
		title = _("Device Error");

		/* TRANSLATORS: message, the sample was not read correctly */
		message = _("The device could not measure the color spot correctly.");

		/* push new messages into the UI */
		gcm_calibrate_dialog_show (priv->calibrate_dialog, GCM_CALIBRATE_DIALOG_TAB_GENERIC, title, message);
		gcm_calibrate_dialog_set_show_button_ok (priv->calibrate_dialog, TRUE);
		gcm_calibrate_dialog_set_show_expander (priv->calibrate_dialog, TRUE);

		/* TRANSLATORS: button */
		gcm_calibrate_dialog_set_button_ok_id (priv->calibrate_dialog, _("Retry"));

		/* set state */
		priv->argyllcms_ok = " ";
		priv->state = GCM_CALIBRATE_ARGYLL_STATE_WAITING_FOR_STDIN;
		goto out;
	}

	/* reading strip */
	if (g_str_has_prefix (line, "Ready to read strip pass ")) {

		/* TRANSLATORS: dialog title, where %s is a letter like 'A' */
		title_str = g_strdup_printf (_("Ready to read strip %s"), line+25);

		string = g_string_new ("");

		/* TRANSLATORS: dialog message, just follow the hardware instructions */
		g_string_append (string, _("Place the colorimeter on the area of white next to the letter and click and hold the measure switch."));
		g_string_append (string, "\n\n");

		/* TRANSLATORS: dialog message, just follow the hardware instructions */
		g_string_append (string, _("Slowly scan the target line from left to right and release the switch when you get to the end of the page."));
		g_string_append (string, "\n\n");

		/* TRANSLATORS: dialog message, the sensor has to be above the line */
		g_string_append (string, _("Ensure the center of the device is properly aligned with the row you are trying to measure."));
		g_string_append (string, "\n\n");

		/* TRANSLATORS: dialog message, just follow the hardware instructions */
		g_string_append (string, _("If you make a mistake just release the switch and you'll get a chance to try again."));

		/* push new messages into the UI */
		gcm_calibrate_dialog_show (priv->calibrate_dialog, GCM_CALIBRATE_DIALOG_TAB_GENERIC, title_str, string->str);
		gcm_calibrate_dialog_set_show_button_ok (priv->calibrate_dialog, FALSE);
		gcm_calibrate_dialog_set_show_expander (priv->calibrate_dialog, TRUE);
		gcm_calibrate_dialog_set_image_filename (priv->calibrate_dialog, "scan-target.svg");
		goto out;
	}

	/* report a warning so friendly people report bugs */
	egg_warning ("VTE: could not screenscrape: %s", line);
out:
	g_free (title_str);
	g_strfreev (split);
	if (string != NULL)
		g_string_free (string, TRUE);
	return ret;
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
	gboolean ret;
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
		ret = gcm_calibrate_argyll_process_output_cmd (calibrate_argyll, split[i]);
		if (!ret)
			break;
	}

	/* save, so we don't re-process old text */
	priv->vte_previous_row = row;
	priv->vte_previous_col = col;

	g_free (output);
	g_strfreev (split);
}

/**
 * gcm_calibrate_argyll_response_cb:
 **/
static void
gcm_calibrate_argyll_response_cb (GtkWidget *widget, GtkResponseType response, GcmCalibrateArgyll *calibrate_argyll)
{
	GcmCalibrateArgyllPrivate *priv = calibrate_argyll->priv;

	/* save our state */
	priv->response = response;

	/* ok */
	if (response == GTK_RESPONSE_OK) {

		/* send input if waiting */
		if (priv->state == GCM_CALIBRATE_ARGYLL_STATE_WAITING_FOR_STDIN) {
			egg_debug ("sending '%s' to argyll", priv->argyllcms_ok);
			vte_terminal_feed_child (VTE_TERMINAL(priv->terminal), priv->argyllcms_ok, 1);
			gcm_calibrate_dialog_pop (priv->calibrate_dialog);
			priv->state = GCM_CALIBRATE_ARGYLL_STATE_RUNNING;
		}

		/* clear loop if waiting */
		if (priv->state == GCM_CALIBRATE_ARGYLL_STATE_WAITING_FOR_LOOP) {
			g_main_loop_quit (priv->loop);
			priv->state = GCM_CALIBRATE_ARGYLL_STATE_RUNNING;
		}

		goto out;
	}

	/* cancel */
	if (response == GTK_RESPONSE_CANCEL) {

		/* send input if waiting */
		if (priv->state == GCM_CALIBRATE_ARGYLL_STATE_WAITING_FOR_STDIN) {
			egg_debug ("sending 'Q' to argyll");
			vte_terminal_feed_child (VTE_TERMINAL(priv->terminal), "Q", 1);
			priv->state = GCM_CALIBRATE_ARGYLL_STATE_RUNNING;
		}

		/* clear loop if waiting */
		if (priv->state == GCM_CALIBRATE_ARGYLL_STATE_WAITING_FOR_LOOP) {
			g_main_loop_quit (priv->loop);
			priv->state = GCM_CALIBRATE_ARGYLL_STATE_RUNNING;
		}

		/* hide the window */
		gcm_calibrate_dialog_hide (priv->calibrate_dialog);

		/* stop loop */
		if (g_main_loop_is_running (priv->loop))
			g_main_loop_quit (priv->loop);
		goto out;
	}
out:
	return;
}

/**
 * gcm_calibrate_argyll_status_changed_cb:
 **/
static void
gcm_calibrate_argyll_status_changed_cb (GcmPrint *print, GtkPrintStatus status, GcmCalibrateArgyll *calibrate_argyll)
{
	const gchar *title;
	const gchar *message = NULL;
	GcmCalibrateArgyllPrivate *priv = calibrate_argyll->priv;

	/* TRANSLATORS: title, printing reference files to media */
	title = _("Printing");

	switch (status) {
	case GTK_PRINT_STATUS_INITIAL:
	case GTK_PRINT_STATUS_PREPARING:
	case GTK_PRINT_STATUS_GENERATING_DATA:
		/* TRANSLATORS: dialog message */
		message = _("Preparing the data for the printer.");
		break;
	case GTK_PRINT_STATUS_SENDING_DATA:
	case GTK_PRINT_STATUS_PENDING:
	case GTK_PRINT_STATUS_PENDING_ISSUE:
		/* TRANSLATORS: dialog message */
		message = _("Sending the targets to the printer.");
		break;
	case GTK_PRINT_STATUS_PRINTING:
		/* TRANSLATORS: dialog message */
		message = _("Printing the targets...");
		break;
	case GTK_PRINT_STATUS_FINISHED:
		/* TRANSLATORS: dialog message */
		message = _("The printing has finished.");
		break;
	case GTK_PRINT_STATUS_FINISHED_ABORTED:
		/* TRANSLATORS: dialog message */
		message = _("The print was aborted.");
		break;
	default:
		break;
	}

	/* not a translated message */
	if (message == NULL)
		return;

	/* push new messages into the UI */
	gcm_calibrate_dialog_show (priv->calibrate_dialog, GCM_CALIBRATE_DIALOG_TAB_GENERIC, title, message);
	gcm_calibrate_dialog_set_show_button_ok (priv->calibrate_dialog, FALSE);
	gcm_calibrate_dialog_set_show_expander (priv->calibrate_dialog, FALSE);
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
	parent_class->calibrate_printer = gcm_calibrate_argyll_printer;
	parent_class->calibrate_spotread = gcm_calibrate_argyll_spotread;

	g_type_class_add_private (klass, sizeof (GcmCalibrateArgyllPrivate));
}

/**
 * gcm_calibrate_argyll_init:
 **/
static void
gcm_calibrate_argyll_init (GcmCalibrateArgyll *calibrate_argyll)
{
	calibrate_argyll->priv = GCM_CALIBRATE_ARGYLL_GET_PRIVATE (calibrate_argyll);
	calibrate_argyll->priv->child_pid = -1;
	calibrate_argyll->priv->loop = g_main_loop_new (NULL, FALSE);
	calibrate_argyll->priv->vte_previous_row = 0;
	calibrate_argyll->priv->vte_previous_col = 0;
	calibrate_argyll->priv->already_on_window = FALSE;
	calibrate_argyll->priv->done_calibrate = FALSE;
	calibrate_argyll->priv->state = GCM_CALIBRATE_ARGYLL_STATE_IDLE;
	calibrate_argyll->priv->print = gcm_print_new ();
	g_signal_connect (calibrate_argyll->priv->print, "status-changed",
			  G_CALLBACK (gcm_calibrate_argyll_status_changed_cb), calibrate_argyll);
	calibrate_argyll->priv->calibrate_dialog = gcm_calibrate_dialog_new ();
	g_signal_connect (calibrate_argyll->priv->calibrate_dialog, "response",
			  G_CALLBACK (gcm_calibrate_argyll_response_cb), calibrate_argyll);

	/* get screen */
	calibrate_argyll->priv->screen = gcm_screen_new ();

	/* add vte widget */
	calibrate_argyll->priv->terminal = vte_terminal_new ();
	vte_terminal_set_size (VTE_TERMINAL(calibrate_argyll->priv->terminal), 80, 10);
	g_signal_connect (calibrate_argyll->priv->terminal, "child-exited",
			  G_CALLBACK (gcm_calibrate_argyll_exit_cb), calibrate_argyll);
	g_signal_connect (calibrate_argyll->priv->terminal, "cursor-moved",
			  G_CALLBACK (gcm_calibrate_argyll_cursor_moved_cb), calibrate_argyll);
	gcm_calibrate_dialog_pack_details (calibrate_argyll->priv->calibrate_dialog,
					   calibrate_argyll->priv->terminal);
}

/**
 * gcm_calibrate_argyll_finalize:
 **/
static void
gcm_calibrate_argyll_finalize (GObject *object)
{
	GcmCalibrateArgyll *calibrate_argyll = GCM_CALIBRATE_ARGYLL (object);
	GcmCalibrateArgyllPrivate *priv = calibrate_argyll->priv;

	/* process running */
	if (priv->child_pid != -1) {
		egg_debug ("killing child");
		kill (priv->child_pid, SIGKILL);

		/* wait until child has quit */
		g_main_loop_run (priv->loop);
	}

	/* hide */
	gcm_calibrate_dialog_hide (priv->calibrate_dialog);

	g_main_loop_unref (priv->loop);
	g_object_unref (priv->screen);
	g_object_unref (priv->calibrate_dialog);
	g_object_unref (priv->print);

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


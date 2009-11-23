/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007-2009 Richard Hughes <richard@hughsie.com>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:egg-debug
 * @short_description: Debugging functions
 *
 * This file contains functions that can be used for debugging.
 */

#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

#ifdef HAVE_EXECINFO_H
#include <execinfo.h>
#endif

#include "egg-debug.h"

#define CONSOLE_RESET		0
#define CONSOLE_BLACK 		30
#define CONSOLE_RED		31
#define CONSOLE_GREEN		32
#define CONSOLE_YELLOW		33
#define CONSOLE_BLUE		34
#define CONSOLE_MAGENTA		35
#define CONSOLE_CYAN		36
#define CONSOLE_WHITE		37

static gint _fd = -1;
static gboolean _verbose = FALSE;
static gboolean _console = FALSE;
static gchar *_log_filename = NULL;
static gboolean _initialized = FALSE;
static GPtrArray *_modules = NULL;
static GPtrArray *_functions = NULL;

/**
 * egg_debug_filter_module:
 **/
static gboolean
egg_debug_filter_module (const gchar *filename)
{
	gchar *module;
	const gchar *module_tmp;
	guint i;
	gboolean ret = FALSE;

	/* nothing filtering */
	if (_modules == NULL)
		return FALSE;

	/* are we in the filter list */
	module = g_strdup (filename);
	g_strdelimit (module, ".", '\0');
	for (i=0; i<_modules->len; i++) {
		module_tmp = g_ptr_array_index (_modules, i);
		if (g_strcmp0 (module_tmp, module) == 0) {
			ret = TRUE;
			break;
		}
	}
	return ret;
}

/**
 * egg_debug_filter_function:
 **/
static gboolean
egg_debug_filter_function (const gchar *function)
{
	guint i;
	const gchar *function_tmp;
	gboolean ret = FALSE;

	/* nothing filtering */
	if (_functions == NULL)
		return FALSE;

	/* are we in the filter list */
	for (i=0; i<_functions->len; i++) {
		function_tmp = g_ptr_array_index (_functions, i);
		if (g_str_has_prefix (function, function_tmp)) {
			ret = TRUE;
			break;
		}
	}
	return ret;
}

/**
 * egg_debug_set_console_mode:
 **/
static void
egg_debug_set_console_mode (guint console_code)
{
	gchar command[13];

	/* don't put extra commands into logs */
	if (!_console)
		return;

	/* Command is the control command to the terminal */
	g_snprintf (command, 13, "%c[%dm", 0x1B, console_code);
	printf ("%s", command);
}

/**
 * egg_debug_backtrace:
 **/
void
egg_debug_backtrace (void)
{
#ifdef HAVE_EXECINFO_H
	void *call_stack[512];
	int  call_stack_size;
	char **symbols;
	int i = 1;

	call_stack_size = backtrace (call_stack, G_N_ELEMENTS (call_stack));
	symbols = backtrace_symbols (call_stack, call_stack_size);
	if (symbols != NULL) {
		egg_debug_set_console_mode (CONSOLE_RED);
		g_print ("Traceback:\n");
		while (i < call_stack_size) {
			g_print ("\t%s\n", symbols[i]);
			i++;
		}
		egg_debug_set_console_mode (CONSOLE_RESET);
		free (symbols);
	}
#endif
}

/**
 * egg_debug_log_line:
 **/
static void
egg_debug_log_line (const gchar *buffer)
{
	ssize_t count;

	/* open a file */
	if (_fd == -1) {
		/* ITS4: ignore, /var/log/foo is owned by root, and this is just debug text */
		_fd = open (_log_filename, O_WRONLY|O_APPEND|O_CREAT, 0777);
		if (_fd == -1)
			g_error ("could not open log: '%s'", _log_filename);
	}

	/* ITS4: ignore, debug text always NULL terminated */
	count = write (_fd, buffer, strlen (buffer));
	if (count == -1)
		g_warning ("could not write %s", buffer);

	/* newline */
	count = write (_fd, "\n", 1);
	if (count == -1)
		g_warning ("could not write newline");
}

/**
 * egg_debug_print_line:
 **/
static void
egg_debug_print_line (const gchar *func, const gchar *file, const int line, const gchar *buffer, guint color)
{
	gchar *str_time;
	gchar *header;
	time_t the_time;

	time (&the_time);
	str_time = g_new0 (gchar, 255);
	strftime (str_time, 254, "%H:%M:%S", localtime (&the_time));

	/* generate header text */
	header = g_strdup_printf ("TI:%s\tFI:%s\tFN:%s,%d", str_time, file, func, line);
	g_free (str_time);

	/* always in light green */
	egg_debug_set_console_mode (CONSOLE_GREEN);
	printf ("%s\n", header);

	/* different colors according to the severity */
	egg_debug_set_console_mode (color);
	printf (" - %s\n", buffer);
	egg_debug_set_console_mode (CONSOLE_RESET);

	/* log to a file */
	if (_log_filename != NULL) {
		egg_debug_log_line (header);
		egg_debug_log_line (buffer);
	}

	/* flush this output, as we need to debug */
	fflush (stdout);

	g_free (header);
}

/**
 * egg_debug_real:
 **/
void
egg_debug_real (const gchar *func, const gchar *file, const int line, const gchar *format, ...)
{
	va_list args;
	gchar *buffer = NULL;

	if (!_verbose && !egg_debug_filter_module (file) && !egg_debug_filter_function (func))
		return;

	va_start (args, format);
	g_vasprintf (&buffer, format, args);
	va_end (args);

	egg_debug_print_line (func, file, line, buffer, CONSOLE_BLUE);

	g_free (buffer);
}

/**
 * egg_warning_real:
 **/
void
egg_warning_real (const gchar *func, const gchar *file, const int line, const gchar *format, ...)
{
	va_list args;
	gchar *buffer = NULL;

	if (!_verbose && !egg_debug_filter_module (file) && !egg_debug_filter_function (func))
		return;

	va_start (args, format);
	g_vasprintf (&buffer, format, args);
	va_end (args);

	/* do extra stuff for a warning */
	if (!_console)
		printf ("*** WARNING ***\n");
	egg_debug_print_line (func, file, line, buffer, CONSOLE_RED);

	g_free (buffer);
}

/**
 * egg_error_real:
 **/
void
egg_error_real (const gchar *func, const gchar *file, const int line, const gchar *format, ...)
{
	va_list args;
	gchar *buffer = NULL;

	va_start (args, format);
	g_vasprintf (&buffer, format, args);
	va_end (args);

	/* do extra stuff for a warning */
	if (!_console)
		printf ("*** ERROR ***\n");
	egg_debug_print_line (func, file, line, buffer, CONSOLE_RED);
	g_free (buffer);

	/* we want to fix this! */
	egg_debug_backtrace ();

	exit (1);
}

/**
 * egg_debug_is_verbose:
 *
 * Returns: TRUE if we have debugging enabled
 **/
gboolean
egg_debug_is_verbose (void)
{
	return _verbose;
}

/**
 * egg_debug_set_log_filename:
 **/
void
egg_debug_set_log_filename (const gchar *filename)
{
	g_free (_log_filename);
	_log_filename = g_strdup (filename);
}

/**
 * egg_debug_strv_split_to_ptr_array:
 **/
static GPtrArray *
egg_debug_strv_split_to_ptr_array (gchar **modules)
{
	GPtrArray *array = NULL;
	guint i, j;
	gchar **split;

	/* nothing */
	if (modules == NULL)
		goto out;

	/* create array of strings */
	array = g_ptr_array_new_with_free_func (g_free);

	/* parse each --debug-foo option */
	for (i=0; modules[i] != NULL; i++) {
		/* use a comma to delimit multiple entries */
		split = g_strsplit (modules[i], ",", -1);
		for (j=0; split[j] != NULL; j++)
			g_ptr_array_add (array, g_strdup (split[j]));
		g_strfreev (split);
	}
out:
	return array;
}

/**
 * egg_debug_init:
 * @argc: a pointer to the number of command line arguments.
 * @argv: a pointer to the array of command line arguments.
 *
 * Parses command line arguments.
 *
 * Any arguments used are removed from the array and
 * @argc and @argv are updated accordingly.
 *
 * Return value: %TRUE if initialization succeeded, otherwise %FALSE.
 **/
gboolean
egg_debug_init (gint *argc, gchar ***argv)
{
	GOptionContext *option_context;
	GOptionGroup *group;
	GError *error = NULL;
	gboolean verbose = FALSE;
	gchar *log_filename = NULL;
	gchar **modules = NULL;
	gchar **functions = NULL;
	const GOptionEntry main_entries[] = {
		{ "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose,
		  /* TRANSLATORS: turn on all debugging */
		  _("Show debugging information for all files"), NULL },
		{ NULL}
	};
	const GOptionEntry debug_entries[] = {
		{ "debug-modules", '\0', 0, G_OPTION_ARG_STRING_ARRAY, &modules,
		  /* TRANSLATORS: a list of modules to debug */
		  _("Debug these specific modules"), NULL },
		{ "debug-functions", '\0', 0, G_OPTION_ARG_STRING_ARRAY, &functions,
		  /* TRANSLATORS: a list of functions to debug */
		  _("Debug these specific functions"), NULL },
		{ "debug-log-filename", '\0', 0, G_OPTION_ARG_STRING, &log_filename,
		  /* TRANSLATORS: save to a log */
		  _("Log debugging data to a file"), NULL },
		{ NULL}
	};

	/* already initialized */
	if (_initialized)
		return TRUE;

	option_context = g_option_context_new (NULL);
	g_option_context_set_ignore_unknown_options (option_context, TRUE);
	g_option_context_set_help_enabled (option_context, TRUE);

	/* create a new group */
	group = g_option_group_new ("debug", "Detailed debugging", "Show all debugging options", NULL, NULL);
	g_option_group_add_entries (group, debug_entries);

	/* only add one main entry */
	g_option_context_add_main_entries (option_context, main_entries, NULL);
	g_option_context_add_group (option_context, group);
	if (!g_option_context_parse (option_context, argc, argv, &error)) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}

	/* set options */
	_log_filename = g_strdup (log_filename);
	_verbose = verbose;
	_initialized = TRUE;
	_modules = egg_debug_strv_split_to_ptr_array (modules);
	_functions = egg_debug_strv_split_to_ptr_array (functions);
	_console = (isatty (fileno (stdout)) == 1);
	egg_debug ("Verbose debugging %i (on console %i)", _verbose, _console);

	g_option_context_free (option_context);
	g_free (log_filename);
	g_strfreev (modules);
	g_strfreev (functions);
	return TRUE;
}

/**
 * egg_debug_free:
 **/
void
egg_debug_free (void)
{
	if (!_initialized)
		return;

	/* close file */
	if (_fd != -1)
		close (_fd);

	/* free memory */
	g_free (_log_filename);
	g_ptr_array_unref (_modules);
	g_ptr_array_unref (_functions);

	/* can not re-init */
	_initialized = FALSE;
}


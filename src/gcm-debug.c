/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include <config.h>
#include <glib/gi18n.h>
#include <unistd.h>
#include <stdio.h>

#include <gcm-debug.h>

static gboolean _verbose = FALSE;
static gboolean _console = FALSE;

gboolean
gcm_debug_is_verbose (void)
{
	/* local first */
	if (_verbose)
		 return TRUE;

	/* fall back to env variable */
	if (g_getenv ("VERBOSE") != NULL)
		 return TRUE;
	return FALSE;
}


static void
gcm_debug_ignore_cb (const gchar *log_domain, GLogLevelFlags log_level,
		     const gchar *message, gpointer user_data)
{
}

static void
gcm_debug_handler_cb (const gchar *log_domain, GLogLevelFlags log_level,
		      const gchar *message, gpointer user_data)
{
	gchar str_time[255];
	time_t the_time;

	/* time header */
	time (&the_time);
	strftime (str_time, 254, "%H:%M:%S", localtime (&the_time));

	/* no color please, we're British */
	if (!_console) {
		if (log_level == G_LOG_LEVEL_DEBUG) {
			g_print ("%s\t%s\n", str_time, message);
		} else {
			g_print ("***\n%s\t%s\n***\n", str_time, message);
		}
		return;
	}

	/* critical is also in red */
	if (log_level == G_LOG_LEVEL_CRITICAL ||
	    log_level == G_LOG_LEVEL_ERROR) {
		g_print ("%c[%dm%s\t", 0x1B, 32, str_time);
		g_print ("%c[%dm%s\n%c[%dm", 0x1B, 31, message, 0x1B, 0);
	} else {
		/* debug in blue */
		g_print ("%c[%dm%s\t", 0x1B, 32, str_time);
		g_print ("%c[%dm%s\n%c[%dm", 0x1B, 34, message, 0x1B, 0);
	}
}

static gboolean
gcm_debug_pre_parse_hook (GOptionContext *context, GOptionGroup *group, gpointer data, GError **error)
{
	const GOptionEntry main_entries[] = {
		{ "verbose", 'v', 0, G_OPTION_ARG_NONE, &_verbose,
		  /* TRANSLATORS: turn on all debugging */
		  N_("Show debugging information for all files"), NULL },
		{ NULL}
	};

	/* add main entry */
	g_option_context_add_main_entries (context, main_entries, NULL);
	return TRUE;
}

void
gcm_debug_setup (gboolean enabled)
{
	if (enabled) {
		g_log_set_fatal_mask (NULL, G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL);
		g_log_set_handler (G_LOG_DOMAIN,
				   G_LOG_LEVEL_ERROR |
				   G_LOG_LEVEL_CRITICAL |
				   G_LOG_LEVEL_DEBUG |
				   G_LOG_LEVEL_WARNING,
				   gcm_debug_handler_cb, NULL);
	} else {
		/* hide all debugging */
		g_log_set_handler (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
				   gcm_debug_ignore_cb, NULL);
	}
}

static gboolean
gcm_debug_post_parse_hook (GOptionContext *context, GOptionGroup *group, gpointer data, GError **error)
{
	/* verbose? */
	gcm_debug_setup (_verbose);
	_console = (isatty (fileno (stdout)) == 1);
	g_debug ("Verbose debugging %s (on console %i)", _verbose ? "enabled" : "disabled", _console);
	return TRUE;
}

/**
 * gcm_debug_get_option_group:
 *
 * Returns a #GOptionGroup for the commandline arguments recognized
 * by debugging. You should add this group to your #GOptionContext
 * with g_option_context_add_group(), if you are using
 * g_option_context_parse() to parse your commandline arguments.
 *
 * Returns: a #GOptionGroup for the commandline arguments
 */
GOptionGroup *
gcm_debug_get_option_group (void)
{
	GOptionGroup *group;
	group = g_option_group_new ("debug", _("Debugging Options"), _("Show debugging options"), NULL, NULL);
	g_option_group_set_parse_hooks (group, gcm_debug_pre_parse_hook, gcm_debug_post_parse_hook);
	return group;
}


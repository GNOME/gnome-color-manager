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

#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>

#include <glib.h>

#define HUEY_RETVAL_SUCCESS		0x00
#define HUEY_RETVAL_LOCKED		0xc0
#define HUEY_RETVAL_UNKNOWN_5A		0x5a /* seen in profiling */
#define HUEY_RETVAL_ERROR		0x80
#define HUEY_RETVAL_UNKNOWN_81		0x81 /* seen once in init */
#define HUEY_RETVAL_RETRY		0x90

static const gchar *
get_return_string (guchar value)
{
	if (value == HUEY_RETVAL_SUCCESS)
		return "success";
	if (value == HUEY_RETVAL_LOCKED)
		return "locked";
	if (value == HUEY_RETVAL_ERROR)
		return "error";
	if (value == HUEY_RETVAL_RETRY)
		return "retry";
	if (value == HUEY_RETVAL_UNKNOWN_5A)
		return "unknown5a";
	if (value == HUEY_RETVAL_UNKNOWN_81)
		return "unknown81";
	return NULL;
}

static const gchar *
get_command_string (guchar value)
{
	if (value == 0x00)
		return "get-status";
	if (value == 0x02)
		return "read-green";
	if (value == 0x03)
		return "read-blue";
	if (value == 0x05)
		return "set-value";
	if (value == 0x06)
		return "get-value";
	if (value == 0x07)
		return "unknown07";
	if (value == 0x08)
		return "reg-read";
	if (value == 0x0e)
		return "unlock";
	if (value == 0x0f)
		return "unknown0f";
	if (value == 0x10)
		return "unknown10";
	if (value == 0x11)
		return "unknown11";
	if (value == 0x12)
		return "unknown12";
	if (value == 0x13)
		return "unknown13";
	if (value == 0x15)
		return "unknown15(status?)";
	if (value == 0x16)
		return "measure-rgb";
	if (value == 0x21)
		return "unknown21";
	if (value == 0x17)
		return "ambient";
	if (value == 0x18)
		return "set-leds";
	if (value == 0x19)
		return "unknown19";
	return NULL;
}

static void
parse_output (GString *output, const gchar *line, gboolean reply)
{
	gchar **tok;
	guint j;
	guchar cmd;
	const gchar *annote;
	tok = g_strsplit (line, " ", -1);

	/* only know how to parse 8 bytes */
	if (g_strv_length (tok) != 8)
		goto out;
	for (j=0; j<8; j++) {
		annote = NULL;
		cmd = g_ascii_strtoll (tok[j], NULL, 16);
		if (j == 0 && reply) {
			annote = get_return_string (cmd);
			if (annote == NULL)
				g_warning ("return code 0x%02x not known in %s", cmd, line);
		}
		if ((j == 0 && !reply) ||
		    (j == 1 && reply)) {
			annote = get_command_string (cmd);
			if (annote == NULL)
				g_warning ("command code 0x%02x not known", cmd);
		}
		if (annote != NULL)
			g_string_append_printf (output, "%02x(%s) ", cmd, annote);
		else
			g_string_append_printf (output, "%02x ", cmd);
	}
out:
	g_strfreev (tok);
}

gint
main (gint argc, gchar *argv[])
{
	gboolean ret;
	gchar *data = NULL;
	gchar **split = NULL;
	GString *output = NULL;
	GError *error = NULL;
	guint i;
	gchar *line;
	gboolean reply = FALSE;

	if (argc != 3) {
		g_print ("need to specify two files\n");
		goto out;
	}

	g_print ("parsing %s into %s... ", argv[1], argv[2]);

	/* read file */
	ret = g_file_get_contents (argv[1], &data, NULL, &error);
	if (!ret) {
		g_print ("failed to read: %s\n", error->message);
		g_error_free (error);
		goto out;
	}

	/* parse string */
	output = g_string_new ("// automatically generated, do not edit\n");
	split = g_strsplit (data, "\n", -1);
	for (i=0; split[i] != NULL; i++) {
		line = split[i];

		/* timestamp */
		if (line[0] == '[')
			continue;

		/* --------- argyll -D9 format --------- */
		if (g_str_has_prefix (line, "huey: Sending cmd")) {
			g_string_append (output, " ---> ");
			reply = FALSE;
		}
		if (g_str_has_prefix (line, "huey: Reading response")) {
			g_string_append (output, " <--- ");
			reply = TRUE;
		}
		if (g_str_has_prefix (line, "icoms: Writing control data")) {
			parse_output (output, &line[28], reply);
		}
		if (g_str_has_prefix (line, " '")) {
			line[21] = '\0';
			/* argyll 'helpfully' removes the two bytes */
			g_string_append (output, "00(success) xx(cmd) ");
			g_string_append (output, &line[2]);
			g_string_append (output, "\n");
		}
		if (g_strcmp0 (line, " ICOM err 0x0") == 0)
			g_string_append (output, "\n");
		/* --------- argyll -D9 format --------- */

		/* function */
		if (line[0] == '-') {
			g_string_append (output, "\n");
			if (g_str_has_suffix (line, "URB_FUNCTION_CLASS_INTERFACE:"))
				g_string_append (output, "[class-interface]     ");
			else if (g_str_has_suffix (line, "URB_FUNCTION_CONTROL_TRANSFER:"))
				g_string_append (output, "[control-transfer]    ");
			else if (g_str_has_suffix (line, "URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER:"))
				g_string_append (output, "[interrupt-transfer]  ");
			else
				g_string_append (output, "[unknown]             ");
		}

		if (g_strstr_len (line, -1, "USBD_TRANSFER_DIRECTION_IN") != NULL) {
			g_string_append (output, " <--- ");
			reply = TRUE;
		}
		if (g_strstr_len (line, -1, "USBD_TRANSFER_DIRECTION_OUT") != NULL) {
			g_string_append (output, " ---> ");
			reply = FALSE;
		}

		if (g_strstr_len (line, -1, "00000000:") != NULL) {
			parse_output (output, &line[14], reply);
		}

//		g_print ("%i:%s\n", i, split[i]);
	}

	/* write file */
	ret = g_file_set_contents (argv[2], output->str, -1, &error);
	if (!ret) {
		g_print ("failed to read: %s\n", error->message);
		g_error_free (error);
		goto out;
	}

	g_print ("done!\n");

out:
	if (output != NULL)
		g_string_free (output, TRUE);
	g_free (data);
	g_strfreev (split);
	return 0;
}


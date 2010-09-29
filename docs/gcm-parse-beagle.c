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
#include <libcolor-glib.h>

#include "gcm-sensor-huey-private.h"

typedef enum {
	GCM_PARSE_SECTION_LEVEL,
	GCM_PARSE_SECTION_SP,
	GCM_PARSE_SECTION_MS_US,
	GCM_PARSE_SECTION_DUR,
	GCM_PARSE_SECTION_LEN,
	GCM_PARSE_SECTION_ERR,
	GCM_PARSE_SECTION_DEV,
	GCM_PARSE_SECTION_EP,
	GCM_PARSE_SECTION_RECORD,
	GCM_PARSE_SECTION_SUMMARY
} GcmParseSection;

typedef enum {
	GCM_PARSE_ENTRY_DIRECTION_UNKNOWN,
	GCM_PARSE_ENTRY_DIRECTION_REQUEST,
	GCM_PARSE_ENTRY_DIRECTION_REPLY
} GcmParseEntryDirection;

typedef struct {
	const gchar		*record;
	const gchar		*summary;
	const gchar		*summary_pretty;
	gint			 dev;
	gint			 ep;
	GcmParseEntryDirection	 direction;
} GcmParseEntry;

/**
 * gcm_parse_beagle_process_entry_huey:
 **/
static void
gcm_parse_beagle_process_entry_huey (GcmParseEntry *entry)
{
	gchar **tok;
	guint j;
	guchar cmd;
	guchar instruction = 0;
	const gchar *command_as_text;
	GString *output;

	/* only know how to parse 8 bytes */
	tok = g_strsplit (entry->summary, " ", -1);
	if (g_strv_length (tok) != 8) {
		g_print ("not 8 tokens: %s\n", entry->summary);
		goto out;
	}

	output = g_string_new ("");
	for (j=0; j<8; j++) {
		command_as_text = NULL;
		cmd = g_ascii_strtoll (tok[j], NULL, 16);
		if (j == 0 && entry->direction == GCM_PARSE_ENTRY_DIRECTION_REPLY) {
			command_as_text = gcm_sensor_huey_return_code_to_string (cmd);
			if (command_as_text == NULL)
				g_warning ("return code 0x%02x not known in %s", cmd, entry->summary);
		}
		if ((j == 0 && entry->direction == GCM_PARSE_ENTRY_DIRECTION_REQUEST) ||
		    (j == 1 && entry->direction == GCM_PARSE_ENTRY_DIRECTION_REPLY)) {
			instruction = cmd;
			command_as_text = gcm_sensor_huey_command_code_to_string (instruction);
			if (command_as_text == NULL)
				g_warning ("command code 0x%02x not known", cmd);
		}

		/* some requests are filled with junk data */
		if (entry->direction == GCM_PARSE_ENTRY_DIRECTION_REQUEST &&
		    instruction == GCM_SENSOR_HUEY_COMMAND_REGISTER_READ && j > 1)
			g_string_append_printf (output, "xx ");
		else if (entry->direction == GCM_PARSE_ENTRY_DIRECTION_REQUEST &&
			 instruction == GCM_SENSOR_HUEY_COMMAND_SET_LEDS && j > 4)
			g_string_append_printf (output, "xx ");
		else if (entry->direction == GCM_PARSE_ENTRY_DIRECTION_REQUEST &&
			 instruction == GCM_SENSOR_HUEY_COMMAND_GET_AMBIENT && j > 3)
			g_string_append_printf (output, "xx ");
		else if (command_as_text != NULL)
			g_string_append_printf (output, "%02x(%s) ", cmd, command_as_text);
		else
			g_string_append_printf (output, "%02x ", cmd);
	}

	/* remove trailing space */
	if (output->len > 1)
		g_string_set_size (output, output->len - 1);
out:
	if (output != NULL)
		entry->summary_pretty = g_string_free (output, FALSE);
	g_strfreev (tok);
}

/**
 * gcm_parse_beagle_process_entry:
 **/
static gchar *
gcm_parse_beagle_process_entry (GcmSensorKind kind, GcmParseEntry *entry)
{
	gchar *retval = NULL;
	const gchar *direction = "??";

	/* timeout */
	if (g_strcmp0 (entry->record, "[250 IN-NAK]") == 0)
		goto out;

	/* device closed */
	if (g_strcmp0 (entry->record, "[1 ORPHANED]") == 0)
		goto out;

	/* usb error */
	if (g_strcmp0 (entry->record, "[53 SYNC ERRORS]") == 0)
		goto out;
	if (g_strcmp0 (entry->record, "[240 IN-NAK]") == 0)
		goto out;
	if (g_strcmp0 (entry->record, "Bus event") == 0)
		goto out;

	/* start or end of file */
	if (g_str_has_prefix (entry->record, "Capture started"))
		goto out;
	if (g_strcmp0 (entry->record, "Capture stopped") == 0)
		goto out;

	/* get direction */
	if (g_str_has_prefix (entry->record, "IN txn"))
		entry->direction = GCM_PARSE_ENTRY_DIRECTION_REPLY;
	else if (g_strcmp0 (entry->record, "Control Transfer") == 0)
		entry->direction = GCM_PARSE_ENTRY_DIRECTION_REQUEST;

	/* get correct string */
	if (entry->direction == GCM_PARSE_ENTRY_DIRECTION_REQUEST)
		direction = ">>";
	else if (entry->direction == GCM_PARSE_ENTRY_DIRECTION_REPLY)
		direction = "<<";

	/* sexify the output */
	if (kind == GCM_SENSOR_KIND_HUEY)
		gcm_parse_beagle_process_entry_huey (entry);
	retval = g_strdup_printf ("dev%02i ep%02i\t%s\t%s\n",
				  entry->dev, entry->ep, direction,
				  entry->summary_pretty != NULL ? entry->summary_pretty : entry->summary);
out:
	return retval;
}

/**
 * main:
 **/
gint
main (gint argc, gchar *argv[])
{
	gboolean ret;
	gchar *data = NULL;
	gchar **split = NULL;
	gchar **sections = NULL;
	GString *output = NULL;
	GError *error = NULL;
	guint i;
	GcmParseEntry entry;
	gchar *part;
	gint retval = 1;
	GcmSensorKind kind;

	if (argc != 4) {
		g_print ("need to specify [huey|colormunki] input output\n");
		goto out;
	}
	kind = gcm_sensor_kind_from_string (argv[1]);
	if (kind != GCM_SENSOR_KIND_HUEY &&
	    kind != GCM_SENSOR_KIND_COLOR_MUNKI) {
		g_print ("only huey and colormunki device kinds supported\n");
		goto out;
	}

	/* read file */
	ret = g_file_get_contents (argv[2], &data, NULL, &error);
	if (!ret) {
		g_print ("failed to read: %s\n", error->message);
		g_error_free (error);
		goto out;
	}

	/* parse string */
	output = g_string_new ("// automatically generated, do not edit\n");

	/* parse string */
	split = g_strsplit (data, "\n", -1);
	for (i=0; split[i] != NULL; i++) {

		/* comment or blank line */
		if (split[i][0] == '#' ||
		    split[i][0] == '\0')
			continue;

		/* populate a GcmParseEntry */
		sections = g_strsplit (split[i], ",", -1);
		entry.record = sections[GCM_PARSE_SECTION_RECORD];
		entry.summary = sections[GCM_PARSE_SECTION_SUMMARY];
		entry.dev = atoi (sections[GCM_PARSE_SECTION_DEV]);
		entry.ep = atoi (sections[GCM_PARSE_SECTION_EP]);
		entry.direction = GCM_PARSE_ENTRY_DIRECTION_UNKNOWN;
		entry.summary_pretty = NULL;
		part = gcm_parse_beagle_process_entry (kind, &entry);
		if (part != NULL) {
			g_string_append (output, part);
//			g_print ("%s\n", part);
		}
		g_free (part);
		g_strfreev (sections);
	}

	/* write file */
	ret = g_file_set_contents (argv[3], output->str, -1, &error);
	if (!ret) {
		g_print ("failed to read: %s\n", error->message);
		g_error_free (error);
		goto out;
	}

	g_print ("done!\n");
	retval = 0;
out:
	if (output != NULL)
		g_string_free (output, TRUE);
	g_free (data);
	g_strfreev (split);
	return retval;
}


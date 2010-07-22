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

/**
 * SECTION:gcm-buffer
 * @short_description: buffer functionality
 *
 * Functions to manipulate buffer.
 */

#include "config.h"

#include <glib-object.h>

#include <gcm-buffer.h>

/**
 * gcm_buffer_write_uint16_be:
 * @data: the writable data buffer
 * @value: the value to write
 *
 * Writes a big endian value into a data buffer.
 * NOTE: No validation is done on the buffer to ensure it's big enough.
 **/
void
gcm_buffer_write_uint16_be (guchar *buffer, guint16 value)
{
	buffer[0] = (value >> 8) & 0xff;
	buffer[1] = (value >> 0) & 0xff;
}

/**
 * gcm_buffer_write_uint16_le:
 * @data: the writable data buffer
 * @value: the value to write
 *
 * Writes a little endian value into a data buffer.
 * NOTE: No validation is done on the buffer to ensure it's big enough.
 **/
void
gcm_buffer_write_uint16_le (guchar *buffer, guint16 value)
{
	buffer[0] = (value >> 0) & 0xff;
	buffer[1] = (value >> 8) & 0xff;
}

/**
 * gcm_buffer_read_uint16_be:
 * @data: the writable data buffer
 *
 * Reads a big endian value from a data buffer.
 * NOTE: No validation is done on the buffer to ensure it's valid.
 *
 * Return value: the value to read.
 **/
guint16
gcm_buffer_read_uint16_be (const guchar *buffer)
{
	return GUINT16_FROM_BE (*(guint16*)buffer);
}

/**
 * gcm_buffer_read_uint16_le:
 * @data: the writable data buffer
 *
 * Reads a big endian value from a data buffer.
 * NOTE: No validation is done on the buffer to ensure it's valid.
 *
 * Return value: the value to read.
 **/
guint16
gcm_buffer_read_uint16_le (const guchar *buffer)
{
	return GUINT16_FROM_LE (*(guint16*)buffer);
}

/**
 * gcm_buffer_write_uint32_be:
 * @data: the writable data buffer
 * @value: the value to write
 *
 * Writes a big endian value into a data buffer.
 * NOTE: No validation is done on the buffer to ensure it's big enough.
 **/
void
gcm_buffer_write_uint32_be (guchar *buffer, guint32 value)
{
	buffer[0] = (value >> 24) & 0xff;
	buffer[1] = (value >> 16) & 0xff;
	buffer[2] = (value >> 8) & 0xff;
	buffer[3] = (value >> 0) & 0xff;
}

/**
 * gcm_buffer_write_uint32_le:
 * @data: the writable data buffer
 * @value: the value to write
 *
 * Writes a little endian value into a data buffer.
 * NOTE: No validation is done on the buffer to ensure it's big enough.
 **/
void
gcm_buffer_write_uint32_le (guchar *buffer, guint32 value)
{
	buffer[0] = (value >> 0) & 0xff;
	buffer[1] = (value >> 8) & 0xff;
	buffer[2] = (value >> 16) & 0xff;
	buffer[3] = (value >> 24) & 0xff;
}

/**
 * gcm_buffer_read_uint32_be:
 * @data: the writable data buffer
 *
 * Reads a big endian value from a data buffer.
 * NOTE: No validation is done on the buffer to ensure it's valid.
 *
 * Return value: the value to read.
 **/
guint32
gcm_buffer_read_uint32_be (const guchar *buffer)
{
	return GUINT32_FROM_BE (*(guint32*)buffer);
}

/**
 * gcm_buffer_read_uint32_le:
 * @data: the writable data buffer
 *
 * Reads a big endian value from a data buffer.
 * NOTE: No validation is done on the buffer to ensure it's valid.
 *
 * Return value: the value to read.
 **/
guint32
gcm_buffer_read_uint32_le (const guchar *buffer)
{
	return GUINT32_FROM_LE (*(guint32*)buffer);
}

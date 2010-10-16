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
 * SECTION:gcm-sensor-colormunki-private
 */

#include "gcm-sensor-colormunki-private.h"

/**
 * gcm_sensor_colormunki_command_value_to_string:
 *
 * Since: 2.91.1
 **/
const gchar *
gcm_sensor_colormunki_command_value_to_string (guchar value)
{
	if (value == GCM_SENSOR_COLORMUNKI_COMMAND_DIAL_ROTATE)
		return "dial-rotate";
	if (value == GCM_SENSOR_COLORMUNKI_COMMAND_BUTTON_PRESSED)
		return "button-released";
	if (value == GCM_SENSOR_COLORMUNKI_COMMAND_BUTTON_RELEASED)
		return "button-released";
	return NULL;
}

/**
 * gcm_sensor_colormunki_button_state_to_string:
 *
 * Since: 2.91.1
 **/
const gchar *
gcm_sensor_colormunki_button_state_to_string (guchar value)
{
	if (value == GCM_SENSOR_COLORMUNKI_BUTTON_STATE_RELEASED)
		return "released";
	if (value == GCM_SENSOR_COLORMUNKI_BUTTON_STATE_PRESSED)
		return "pressed";
	return NULL;
}

/**
 * gcm_sensor_colormunki_dial_position_to_string:
 *
 * Since: 2.91.1
 **/
const gchar *
gcm_sensor_colormunki_dial_position_to_string (guchar value)
{
	if (value == GCM_SENSOR_COLORMUNKI_DIAL_POSITION_PROJECTOR)
		return "projector";
	if (value == GCM_SENSOR_COLORMUNKI_DIAL_POSITION_SURFACE)
		return "surface";
	if (value == GCM_SENSOR_COLORMUNKI_DIAL_POSITION_CALIBRATION)
		return "calibration";
	if (value == GCM_SENSOR_COLORMUNKI_DIAL_POSITION_AMBIENT)
		return "ambient";
	return NULL;
}

/**
 * gcm_sensor_colormunki_endpoint_to_string:
 *
 * Since: 2.91.1
 **/
const gchar *
gcm_sensor_colormunki_endpoint_to_string (guint value)
{
	if (value == GCM_SENSOR_COLORMUNKI_EP_CONTROL)
		return "control";
	if (value == GCM_SENSOR_COLORMUNKI_EP_DATA)
		return "data";
	if (value == GCM_SENSOR_COLORMUNKI_EP_EVENT)
		return "event";
	return NULL;
}

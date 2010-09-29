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

#if !defined (LIBCOLOR_GLIB_COMPILATION) && !defined (GCM_I_KNOW_THIS_IS_PRIVATE)
#error "This header file is for internal use only."
#endif

#ifndef __GCM_SENSOR_COLORMUNKI_PRIVATE_H
#define __GCM_SENSOR_COLORMUNKI_PRIVATE_H

#include <glib.h>

G_BEGIN_DECLS

#define GCM_SENSOR_COLORMUNKI_VENDOR_ID				0x0971
#define GCM_SENSOR_COLORMUNKI_PRODUCT_ID			0x2007

#define GCM_SENSOR_COLORMUNKI_COMMAND_DIAL_ROTATE		0x00
#define GCM_SENSOR_COLORMUNKI_COMMAND_BUTTON_PRESSED		0x01
#define GCM_SENSOR_COLORMUNKI_COMMAND_BUTTON_RELEASED		0x02

#define	GCM_SENSOR_COLORMUNKI_BUTTON_STATE_RELEASED		0x00
#define	GCM_SENSOR_COLORMUNKI_BUTTON_STATE_PRESSED		0x01

#define	GCM_SENSOR_COLORMUNKI_DIAL_POSITION_PROJECTOR		0x00
#define	GCM_SENSOR_COLORMUNKI_DIAL_POSITION_SURFACE		0x01
#define	GCM_SENSOR_COLORMUNKI_DIAL_POSITION_CALIBRATION		0x02
#define	GCM_SENSOR_COLORMUNKI_DIAL_POSITION_AMBIENT		0x03
#define	GCM_SENSOR_COLORMUNKI_DIAL_POSITION_UNKNOWN		0xff

#define	COLORMUNKI_EEPROM_OFFSET_SERIAL_NUMBER			0x0018

const gchar	*gcm_sensor_colormunki_button_state_to_string	(guchar	 value);
const gchar	*gcm_sensor_colormunki_dial_position_to_string	(guchar	 value);
const gchar	*gcm_sensor_colormunki_command_value_to_string	(guchar	 value);

G_END_DECLS

#endif /* __GCM_SENSOR_COLORMUNKI_PRIVATE_H */


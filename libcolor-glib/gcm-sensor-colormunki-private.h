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


/*
 * Triggers a request for a bulk transfer of EEPROM
 * Length: 8 bytes
 *
 *   address     length (LE)
 *  ____|____   ____|____
 * /         \ /         \
 * 04 00 00 00 04 00 00 00
 */
#define	GCM_SENSOR_COLORMUNKI_REQUEST_EEPROM_DATA		0x81

/*
 * Gets the next hardware event
 * Length: 8 bytes
 *
 * This blocks until the hardware sends an event, and must either be
 * run in a mainloop or thread to avoid blocking.
 *
 *   subcmd ----\       /------------ 32 bit event time
 *  cmd ----|\ ||       || || || ||
 * Returns: 02 00 00 00 ac 62 07 00
 * always zero ---||-||
 *
 * cmd is:
 * 00	dial rotate
 * 01	button pressed
 * 02	button released
 *
 * subcmd is:
 * 00	button event
 * 01	dial rotate
 */
#define	GCM_SENSOR_COLORMUNKI_REQUEST_INTERRUPT			0x83

/*
 * Returns the major and minor version numbers
 * Length: 24 bytes
 */
#define	GCM_SENSOR_COLORMUNKI_REQUEST_VERSION_STRING		0x85

/*
 * Returns the chip id
 * Length: 8 bytes
 */
#define	GCM_SENSOR_COLORMUNKI_REQUEST_FIRMWARE_PARAMS		0x86

/*
 * Gets the device status
 * Length: 2 bytes
 *
 * Returns:  00 00
 *           |/ ||
 * dial pos -/  \--- button value
 * - 00 = projector
 * - 01 = surface
 * - 02 = calibration
 * - 03 = ambient
 */
#define	GCM_SENSOR_COLORMUNKI_REQUEST_GET_STATUS		0x87

/*
 * Returns the version string
 * Length: 36 bytes
 */
#define	GCM_SENSOR_COLORMUNKI_REQUEST_CHIP_ID			0x8A

/* USB endpoints in use */
#define GCM_SENSOR_COLORMUNKI_EP_CONTROL			0x00
#define GCM_SENSOR_COLORMUNKI_EP_DATA				0x01
#define GCM_SENSOR_COLORMUNKI_EP_EVENT				0x03

/* EEPROM is massive */
#define	COLORMUNKI_EEPROM_OFFSET_SERIAL_NUMBER			0x0018 /* 10 bytes */

const gchar	*gcm_sensor_colormunki_button_state_to_string	(guchar	 value);
const gchar	*gcm_sensor_colormunki_dial_position_to_string	(guchar	 value);
const gchar	*gcm_sensor_colormunki_command_value_to_string	(guchar	 value);
const gchar	*gcm_sensor_colormunki_endpoint_to_string	(guint	 value);

G_END_DECLS

#endif /* __GCM_SENSOR_COLORMUNKI_PRIVATE_H */


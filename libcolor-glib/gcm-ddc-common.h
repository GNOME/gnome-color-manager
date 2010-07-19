/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
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

#if !defined (__LIBCOLOR_GLIB_H_INSIDE__) && !defined (LIBCOLOR_GLIB_COMPILATION)
#error "Only <libcolor-glib.h> can be included directly."
#endif

#ifndef __GCM_DDC_COMMON_H__
#define __GCM_DDC_COMMON_H__

#define __GCM_DDC_COMMON_H_INSIDE__

typedef enum {
	GCM_VERBOSE_NONE,
	GCM_VERBOSE_OVERVIEW,
	GCM_VERBOSE_PROTOCOL
} GcmVerbose;

#define GCM_VCP_REQUEST			0x01
#define GCM_VCP_REPLY			0x02
#define GCM_VCP_SET			0x03
#define GCM_VCP_RESET			0x09
#define GCM_DEFAULT_DDCCI_ADDR		0x37
#define GCM_DEFAULT_EDID_ADDR		0x50
#define GCM_CAPABILITIES_REQUEST	0xf3
#define GCM_CAPABILITIES_REPLY		0xe3
#define GCM_COMMAND_PRESENCE		0xf7
#define GCM_ENABLE_APPLICATION_REPORT	0xf5

#define	GCM_VCP_ID_INVALID		0x00

#define GCM_CTRL_DISABLE		0x0000
#define GCM_CTRL_ENABLE			0x0001

const gchar	*gcm_get_vcp_description_from_index	(guchar		 idx);
guchar		 gcm_get_vcp_index_from_description	(const gchar	*description);

#undef __GCM_DDC_COMMON_H_INSIDE__

#endif /* __GCM_DDC_COMMON_H__ */


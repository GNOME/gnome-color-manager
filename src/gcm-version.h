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

/**
 * SECTION:gcm-version
 * @short_description: Simple versioning helper macros.
 *
 * Simple versioning helper macros.
 */

#ifndef __GCM_VERSION_H
#define __GCM_VERSION_H

/* compile time version
 */
#define GCM_MAJOR_VERSION				(2)
#define GCM_MINOR_VERSION				(91)
#define GCM_MICRO_VERSION				(6)

/* check whether a gcm version equal to or greater than
 * major.minor.micro.
 */
#define GCM_CHECK_VERSION(major,minor,micro)    \
    (GCM_MAJOR_VERSION > (major) || \
     (GCM_MAJOR_VERSION == (major) && GCM_MINOR_VERSION > (minor)) || \
     (GCM_MAJOR_VERSION == (major) && GCM_MINOR_VERSION == (minor) && \
      GCM_MICRO_VERSION >= (micro)))

#endif /* __GCM_VERSION_H */

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#pragma once

#include <glib.h>

gboolean	 gcm_debug_is_verbose		(void);
GOptionGroup	*gcm_debug_get_option_group	(void);
void		 gcm_debug_setup		(gboolean	 enabled);

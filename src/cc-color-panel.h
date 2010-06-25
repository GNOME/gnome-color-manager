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

#ifndef _CC_COLOR_PANEL_H
#define _CC_COLOR_PANEL_H

#include <libgnome-control-center/cc-panel.h>

G_BEGIN_DECLS

#define CC_TYPE_COLOR_PANEL		cc_color_panel_get_type()
#define CC_COLOR_PANEL(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), CC_TYPE_COLOR_PANEL, CcColorPanel))
#define CC_COLOR_PANEL_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), CC_TYPE_COLOR_PANEL, CcColorPanelClass))
#define CC_IS_COLOR_PANEL(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), CC_TYPE_COLOR_PANEL))
#define CC_IS_COLOR_PANEL_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), CC_TYPE_COLOR_PANEL))
#define CC_COLOR_PANEL_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), CC_TYPE_COLOR_PANEL, CcColorPanelClass))

typedef struct _CcColorPanel		CcColorPanel;
typedef struct _CcColorPanelClass	CcColorPanelClass;
typedef struct _CcColorPanelPrivate	CcColorPanelPrivate;

struct _CcColorPanel {
	CcPanel			 parent;
	CcColorPanelPrivate	*priv;
};

struct _CcColorPanelClass {
	CcPanelClass parent_class;
};

GType cc_color_panel_get_type	(void) G_GNUC_CONST;
void  cc_color_panel_register	(GIOModule		*module);

G_END_DECLS

#endif /* _CC_COLOR_PANEL_H */

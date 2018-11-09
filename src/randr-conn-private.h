/*
 * Copyright (C) 2013 Alexey Galakhov <agalakhov@gmail.com>
 *
 * Licensed under the GNU General Public License Version 3
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __RANDR_CONN_PRIVATE_H__
#define __RANDR_CONN_PRIVATE_H__

#include "randr-conn.h"

#include <glib.h>
#include <glib-object.h>

#include <colord.h>

#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

G_BEGIN_DECLS

struct randr_conn {
	GObject		*object;
	Display		*dpy;
	int		event_base;
	int		error_base;
	Atom		edid_atom;
	Atom		type_atom;
	GPtrArray	*displays;
};

struct randr_display_priv {
	struct randr_display	pub;

	struct randr_conn	*conn;
	Window			root;
	RRCrtc			crtc;
};

struct randr_source {
	GSource			parent;
	struct randr_conn	*conn;
	GPollFD			poll_fd;
};

enum {
	SIG_DISPLAY_ADDED,
	SIG_DISPLAY_REMOVED,
	SIG_DISPLAY_CHANGED,
	N_SIG
};

extern guint randr_signals[N_SIG];

void randr_conn_private_init (struct randr_conn *conn, const gchar *disp_name);
void randr_conn_private_finalize (struct randr_conn *conn);
void randr_conn_private_update (struct randr_conn *conn);
struct randr_display *randr_conn_private_find_display (struct randr_conn *conn,
						       const gchar *key, guint offset);
void randr_display_private_apply_icc (struct randr_display *disp, CdIcc *icc);


G_END_DECLS

#endif /* __RANDR_CONN_PRIVATE_H__ */

/* vim: set ts=8 sw=8 tw=0 : */

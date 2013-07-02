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

#ifndef __RANDR_CONN_H__
#define __RANDR_CONN_H__

#include "edid.h"

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS


#define RANDR_TYPE_CONN \
	(randr_conn_get_type ())
#define RANDR_CONN(o) \
	(G_TYPE_CHECK_INSTANCE_CAST ((o), RANDR_TYPE_CONN, RandrConn))
#define RANDR_IS_CONN(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), RANDR_TYPE_CONN))
#define RANDR_CONN_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST ((klass), RANDR_TYPE_CONN, RandrConnClass))
#define RANDR_IS_CONN_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE ((klass), RANDR_TYPE_CONN))
#define RANDR_CONN_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS ((obj), RANDR_TYPE_CONN, RandrConnClass))

struct randr_display {
	int		id;
	const gchar	*name;

	const gchar	*xrandr_name;

	gboolean	is_laptop;
	gboolean	is_primary;
	struct edid	edid;
};


typedef struct _RandrConn {
	GObject parent;
	struct randr_conn *priv;
} RandrConn;


typedef struct _RandrConnClass {
	GObjectClass parent;
	void (*display_added) (RandrConn *conn, const struct randr_display *disp);
	void (*display_removed) (RandrConn *conn, const struct randr_display *disp);
} RandrConnClass;


GType randr_conn_get_type (void);


RandrConn *randr_conn_new (const gchar *display);

void randr_conn_update (RandrConn *conn);

struct randr_display *randr_conn_find_display (RandrConn *conn, const gchar *name);
struct randr_display *randr_conn_find_display_edid (RandrConn *conn, const gchar *edid_cksum);

void randr_display_apply_icc (struct randr_display *disp, const gchar *file);

G_END_DECLS

#endif /* __RANDR_CONN_H__ */

/* vim: set ts=8 sw=8 tw=0 : */

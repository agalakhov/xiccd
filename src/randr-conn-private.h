#ifndef __RANDR_CONN_PRIVATE_H__
#define __RANDR_CONN_PRIVATE_H__

#include <colord.h>
#include "randr-conn.h"
#include <glib.h>
#include <glib-object.h>
#include <X11/extensions/Xrandr.h>
#include <X11/Xlib.h>

G_BEGIN_DECLS

typedef struct randr_conn {
	GObject		*object;
	Display		*dpy;
	int		event_base;
	int		error_base;
	Atom		edid_atom;
	Atom		type_atom;
	GPtrArray	*displays;
} RandrConnPrivate;

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
void randr_conn_private_start (struct randr_conn *conn);
void randr_conn_private_update (struct randr_conn *conn);
struct randr_display *randr_conn_private_find_display (struct randr_conn *conn,
						       const gchar *key, guint offset);
void randr_display_private_apply_icc (struct randr_display *disp, CdIcc *icc);

G_END_DECLS

#endif /* __RANDR_CONN_PRIVATE_H__ */

/* vim: set ts=8 sw=8 tw=0 : */

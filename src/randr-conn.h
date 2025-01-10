#ifndef __RANDR_CONN_H__
#define __RANDR_CONN_H__

#include <colord.h>
#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define RANDR_TYPE_CONN \
	(randr_conn_get_type ())
G_DECLARE_FINAL_TYPE (RandrConn, randr_conn, RANDR, CONN, GObject)

struct randr_display {
	int		id;
	const gchar	*name;

	const gchar	*xrandr_name;

	gboolean	is_laptop;
	gboolean	is_primary;
	CdEdid		*edid;
};

GType randr_conn_get_type (void);
RandrConn *randr_conn_new (const gchar *display);
void randr_conn_start (RandrConn *conn);
struct randr_display *randr_conn_find_display_by_name (RandrConn *conn, const gchar *name);
struct randr_display *randr_conn_find_display_by_edid (RandrConn *conn, const gchar *edid_cksum);
void randr_display_apply_icc (struct randr_display *disp, CdIcc *icc);

G_END_DECLS

#endif /* __RANDR_CONN_H__ */

/* vim: set ts=8 sw=8 tw=0 : */

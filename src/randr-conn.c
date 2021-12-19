
#include "randr-conn.h"
#include "randr-conn-private.h"
#include <glib.h>
#include <glib-object.h>
#include <string.h>

guint randr_signals[N_SIG];

enum {
	PROP_0 = 0,
	PROP_DISPLAY,
	N_PROPERTIES
};

G_DEFINE_TYPE (RandrConn, randr_conn, G_TYPE_OBJECT)

#define RANDR_CONN_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((obj), RANDR_TYPE_CONN, \
	struct randr_conn))

static void
randr_conn_init (RandrConn *self)
{
	self->priv = RANDR_CONN_GET_PRIVATE (self);
	memset (self->priv, 0, sizeof (*self->priv));
}

static GObject *
randr_conn_constructor (GType type, guint n_params, GObjectConstructParam *params)
{
	GObject *obj = G_OBJECT_CLASS (randr_conn_parent_class)
		     ->constructor (type, n_params, params);

	guint i;
	const gchar *display_name = NULL;
	for (i = 0; i < n_params; ++i) {
		if (! strcmp(params[i].pspec->name, "display")) {
			display_name = g_value_get_string (params[i].value);
			break;
		}
	}

	RANDR_CONN (obj)->priv->object = obj;
	randr_conn_private_init (RANDR_CONN (obj)->priv, display_name);

	return obj;
}

static void
randr_conn_finalize (GObject *self)
{
	randr_conn_private_finalize (RANDR_CONN (self)->priv);
}

static void
randr_conn_set_property (GObject *object, guint prop_id, const GValue *val, GParamSpec *pspec)
{
	(void) object;
	(void) prop_id;
	(void) val;
	(void) pspec;
}

static void
randr_conn_class_init (RandrConnClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (struct randr_conn));

	obj_class->constructor = randr_conn_constructor;
	obj_class->finalize = randr_conn_finalize;
	obj_class->set_property = randr_conn_set_property;

	randr_signals[SIG_DISPLAY_ADDED] = g_signal_new ("display_added",
		G_TYPE_FROM_CLASS (obj_class), G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (RandrConnClass, display_added),
		NULL, NULL, NULL,
		G_TYPE_NONE, 1, G_TYPE_POINTER);

	randr_signals[SIG_DISPLAY_REMOVED] = g_signal_new ("display_removed",
		G_TYPE_FROM_CLASS (obj_class), G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (RandrConnClass, display_removed),
		NULL, NULL, NULL,
		G_TYPE_NONE, 1, G_TYPE_POINTER);

	randr_signals[SIG_DISPLAY_CHANGED] = g_signal_new ("display_changed",
		G_TYPE_FROM_CLASS (obj_class), G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (RandrConnClass, display_changed),
		NULL, NULL, NULL,
		G_TYPE_NONE, 1, G_TYPE_POINTER);

	g_object_class_install_property (obj_class, PROP_DISPLAY,
		g_param_spec_string ("display", NULL, "X Display", NULL,
				     G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY)
	);
}


RandrConn *
randr_conn_new (const gchar *display)
{
	RandrConn *obj = RANDR_CONN (g_object_new (RANDR_TYPE_CONN, "display", display, NULL));
	return obj;
}

void
randr_conn_start (RandrConn *conn)
{
	randr_conn_private_start (conn->priv);
}

struct randr_display *
randr_conn_find_display (RandrConn *conn, const gchar *name)
{
	return randr_conn_private_find_display (conn->priv, name,
						G_STRUCT_OFFSET (struct randr_display, name));
}

struct randr_display *randr_conn_find_display_edid (RandrConn *conn, const gchar *edid_cksum)
{
	return randr_conn_private_find_display (conn->priv, edid_cksum,
						G_STRUCT_OFFSET (struct randr_display, edid.cksum));
}

void
randr_display_apply_icc (struct randr_display *disp, CdIcc *icc)
{
	randr_display_private_apply_icc (disp, icc);
}

/* vim: set ts=8 sw=8 tw=0 : */

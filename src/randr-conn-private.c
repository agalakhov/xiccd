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

#include "randr-conn-private.h"

#include "randr-conn.h"

#include "edid.h"
#include "icc.h"

#include <string.h>

#include <glib.h>
#include <glib-object.h>
#include <glib-unix.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xrandr.h>

static void
print_x_error (struct randr_conn *conn, int err, const gchar *msg)
{
	char text[1024];
	XGetErrorText (conn->dpy, err, text, sizeof(text));
	g_critical ("X error: %s: %s", msg, text);
}

static GBytes *
get_output_property (struct randr_conn *conn, RROutput out, Atom prop, Atom type, int fmt)
{
	Atom act_type;
	int act_fmt;
	unsigned long size;
	unsigned long bytes_after;
	unsigned char *data;

	if (prop == None)
		return NULL;

	XRRGetOutputProperty (conn->dpy, out, prop, 0, 100, False, False,
			      AnyPropertyType, &act_type, &act_fmt,
			      &size, &bytes_after, &data);

	if (act_type != type || act_fmt != fmt || size == 0) {
		XFree (data);
		return NULL;
	}

	return g_bytes_new_with_free_func (data, size * (fmt / 8), (GDestroyNotify) XFree, data);
}

static gboolean
get_output_property_atom (struct randr_conn *conn, RROutput out, Atom prop, const gchar *value_eq)
{
	char *str;
	Atom atom;
	gboolean retval = FALSE;
	GBytes *raw = get_output_property (conn, out, prop, XA_ATOM, 32);
	if (! raw)
		return retval;
	if (g_bytes_get_size (raw) != 4)
		goto out;
	atom = *((Atom *) g_bytes_get_data (raw, NULL));
	str = XGetAtomName (conn->dpy, atom);
	if (str) {
		retval = (strcmp (value_eq, str) == 0);
		XFree (str);
	}
out:
	g_bytes_unref (raw);
	return retval;
}

static void
randr_display_free (struct randr_display_priv *disp)
{
	if (disp->pub.name)
		g_free ((gpointer) disp->pub.name);
	if (disp->pub.xrandr_name)
		g_free ((gpointer) disp->pub.xrandr_name);
	edid_free (&disp->pub.edid);
	g_free (disp);
}

static inline const gchar *
make_name (struct randr_display_priv *disp,
	   struct edid *edid, gboolean use_edid)
{
	int i = 0;
	const gchar *arr[16];
	const gchar *retval;
	const gchar *dmi_vendor = NULL;
	const gchar *dmi_product = NULL;

	memset (arr, 0, sizeof(arr));
	arr[i++] = "xrandr";

	if (use_edid && edid->vendor)
		arr[i++] = edid->vendor;

	if (use_edid && edid->model)
		arr[i++] = edid->model;

	if (use_edid && edid->serial)
		arr[i++] = edid->serial;

	/* last resort: use xrandr name */
	if (i <= 1)
		arr[i++] = disp->pub.xrandr_name;

	retval = g_strjoinv ("-", (gchar**)arr);

	if (dmi_vendor)
		g_free ((gpointer) dmi_vendor);
	if (dmi_product)
		g_free ((gpointer) dmi_product);

	return retval;
}

static inline gboolean
is_laptop_conn (struct randr_conn *conn, RROutput out)
{
	return get_output_property_atom (conn, out, conn->type_atom, "Panel");
}

static inline gboolean
is_laptop_name (const gchar *name)
{
	guint i;
	static const gchar *const laptop_names[] = {
		"LVDS", "Lvds", "lvds", "LCD", "eDP", "DFP"
	};
	for (i = 0; i < G_N_ELEMENTS (laptop_names); ++i) {
		if (strncmp (name, laptop_names[i], strlen (laptop_names [i])) == 0)
			return TRUE;
	}
	return FALSE;
}

static inline void
populate_display (struct randr_display_priv *disp, GBytes *edid, RROutput out)
{
	gsize edid_size = 0;
	gconstpointer edid_data = edid ? g_bytes_get_data (edid, &edid_size) : NULL;

	disp->pub.is_laptop = is_laptop_conn (disp->conn, out)
			   || is_laptop_name (disp->pub.xrandr_name);

	edid_parse (&disp->pub.edid, edid_data, edid_size, FALSE);

	disp->pub.name = make_name (disp, &disp->pub.edid, (edid_size != 0));
}

static inline struct randr_display_priv *
process_output (struct randr_conn *conn, XRRScreenResources *rsrc,
		RROutput out, int *outn)
{
	struct randr_display_priv *disp = NULL;

	XRROutputInfo *inf = XRRGetOutputInfo (conn->dpy, rsrc, out);
	if (! inf) {
		g_critical ("XRRGetOutputInfo() failed");
		return NULL;
	}

	if (inf->connection != RR_Disconnected) {
		GBytes *edid = get_output_property (conn, out, conn->edid_atom, XA_INTEGER, 8);

		disp = g_new0 (struct randr_display_priv, 1);
		disp->conn = conn;
		disp->pub.id = *outn;
		disp->pub.xrandr_name = g_strdup (inf->name);
		disp->crtc = inf->crtc;

		populate_display (disp, edid, out);

		if (edid)
			g_bytes_unref (edid);

		++*outn;
	}

	XRRFreeOutputInfo (inf);

	return disp;
}

static inline void
iterate_outputs (struct randr_conn *conn, int scr, GPtrArray *retval)
{
	int io;
	int outn = 0;
	Window root = RootWindow (conn->dpy, scr);
	RROutput primary = XRRGetOutputPrimary (conn->dpy, root);
	XRRScreenResources *rsrc =
		XRRGetScreenResourcesCurrent (conn->dpy, root);
	if (! rsrc) {
		g_critical ("XRRGetScreenResourcesCurrent() failed"
			    " at screen %i", scr);
		return;
	}

	for (io = 0; io < rsrc->noutput; ++io) {
		struct randr_display_priv *disp =
			process_output (conn, rsrc, rsrc->outputs[io], &outn);
		if (disp) {
			disp->root = root; /* more convenient to set it here */
			disp->pub.is_primary = (rsrc->outputs[io] == primary);
			g_ptr_array_add (retval, disp);
		}
	}

	XRRFreeScreenResources (rsrc);
}

static inline GPtrArray *
enum_displays (struct randr_conn *conn)
{
	int scr;
	GPtrArray *retval = g_ptr_array_new_full (4,
		(GDestroyNotify) randr_display_free);
	for (scr = 0; scr < ScreenCount (conn->dpy); ++scr) {
		iterate_outputs (conn, scr, retval);
	}
	return retval;
}

static inline gboolean
same_display (const struct randr_display *a, const struct randr_display *b)
{
	return ! strcmp (a->name, b->name);
}

void
randr_conn_private_update (struct randr_conn *conn)
{
	guint i, j;
	GPtrArray *disps;

	if (! conn->dpy)
		return;

	disps = enum_displays (conn);
	for (i = 0; i < disps->len; ++i) {
		gboolean found = FALSE;
		const struct randr_display *disp = (const struct randr_display *)
						   g_ptr_array_index (disps, i);
		for (j = 0; j < conn->displays->len; ++j) {
			const struct randr_display *odisp = (const struct randr_display *)
							    g_ptr_array_index (conn->displays, j);
			if (same_display (odisp, disp)) {
				g_ptr_array_remove_index_fast (conn->displays, j);
				found = TRUE;
				break;
			}
		}
		if (found)
			continue;
		g_signal_emit (conn->object,
			       randr_signals[SIG_DISPLAY_ADDED], 0, disp);
	}

	for (j = 0; j < conn->displays->len; ++j) {
		const struct randr_display *odisp = (const struct randr_display *)
						    g_ptr_array_index (conn->displays, j);
		g_signal_emit (conn->object,
			       randr_signals[SIG_DISPLAY_REMOVED], 0, odisp);
	}

	g_ptr_array_unref (conn->displays);
	conn->displays = disps;
}



static gboolean
poll_events (gint fd, GIOCondition condition, gpointer user_data)
{
	struct randr_conn *conn = (struct randr_conn *) user_data;
	gboolean happened = FALSE;
	(void) fd;

	if (condition != G_IO_IN)
		return TRUE;

	while (XPending (conn->dpy)) {
		XEvent ev;
		XNextEvent(conn->dpy, &ev);
		if (ev.xany.type - conn->event_base == RRScreenChangeNotify)
			happened = TRUE;
	}

	if (happened)
		randr_conn_private_update (conn);

	return TRUE;
}

static inline void
setup_events (struct randr_conn *conn)
{
	int s;
	for (s = 0; s < ScreenCount (conn->dpy); ++s) {
		Window w = RootWindow (conn->dpy, s);
		XRRSelectInput (conn->dpy, w, RROutputChangeNotifyMask);
	}
	while (XPending (conn->dpy)) {
		XEvent ev;
		XNextEvent (conn->dpy, &ev);
	}
	g_unix_fd_add (ConnectionNumber (conn->dpy), G_IO_IN, poll_events, conn);
}

void
randr_conn_private_init (struct randr_conn *conn, const gchar *disp_name)
{
	int major, minor;

	conn->displays = g_ptr_array_new ();

	g_debug ("opening display %s", disp_name);
	conn->dpy = XOpenDisplay (disp_name);
	if (conn->dpy == NULL) {
		g_critical ("Can't open display: %s", XDisplayName (disp_name));
		goto out;
	}

	if (! XRRQueryExtension (conn->dpy,&conn->event_base, &conn->error_base)
	    || ! XRRQueryVersion (conn->dpy, &major, &minor)) {
		g_critical ("RandR extension is not working on display %s",
			    DisplayString (conn->dpy));
		goto out;
	}

	/* We need RandR 1.3 to support multiple display color mgmt */
	if ((major < 1) || (major == 1 && minor < 3)) {
		g_critical ("RandR %i.%i found on display %s but 1.3 is needed",
			    major, minor, DisplayString (conn->dpy));
		goto out;
	}

	/* RandR 1.2 calls it "EDID_DATA" but we don't support 1.2 */
	conn->edid_atom = XInternAtom (conn->dpy, "EDID", FALSE);
	conn->type_atom = XInternAtom (conn->dpy, "ConnectorType", FALSE);

	setup_events (conn);

	return;

out:
	randr_conn_private_finalize (conn);
}



void
randr_conn_private_finalize (struct randr_conn *conn)
{
	if (conn->dpy)
		XCloseDisplay (conn->dpy);
	if (conn->displays)
		g_ptr_array_unref (conn->displays);
	conn->dpy = NULL;
}



static inline void
apply_gamma (struct randr_display_priv *disp, CdIcc *icc)
{
	Display *dpy = disp->conn->dpy;
	XRRCrtcGamma *gamma = NULL;
	XRRCrtcGamma *gamma2 = NULL;

	int gsize = XRRGetCrtcGammaSize (dpy, disp->crtc);
	if (gsize <= 0) {
		g_critical ("Gamma size is %i at output %s", gsize, disp->pub.name);
		return;
	}

	gamma = XRRAllocGamma (gsize);
	if (! gamma) {
		g_critical ("XRRAllocGamma() failed at output %s", disp->pub.name);
		return;
	}

	icc_to_gamma (gamma, icc);

	XRRSetCrtcGamma (dpy, disp->crtc, gamma);

	/* For some reason gamma may not apply without this */
	gamma2 = XRRGetCrtcGamma (dpy, disp->crtc);
	XRRFreeGamma (gamma2);

	XRRFreeGamma (gamma);
}

static inline void
apply_icc (struct randr_display_priv *disp, GBytes *icc_bytes)
{
	int res;
	Display *dpy = disp->conn->dpy;
	const gchar *oper = NULL;
	Atom at;
	if (disp->pub.id == 0) {
		at = XInternAtom (dpy, "_ICC_PROFILE", False);
	} else {
		gchar *atname = g_strdup_printf ("_ICC_PROFILE_%i", disp->pub.id);
		at = XInternAtom (dpy, atname, False);
		g_free (atname);
	}

	if (icc_bytes) {
		res = XChangeProperty (dpy, disp->root, at, XA_CARDINAL, 8, PropModeReplace,
				       (unsigned char *) g_bytes_get_data (icc_bytes, NULL),
				       g_bytes_get_size (icc_bytes));
		oper = "XChangeProperty()";
	} else {
		res = XDeleteProperty (dpy, disp->root, at);
		oper = "XDeleteProperty()";
	}
	/* Due to a bug in X this may "fail" with BadRequest but still work */
	if (res != Success && res != BadRequest) {
		print_x_error (disp->conn, res, oper);
	}

}

void
randr_display_private_apply_icc (struct randr_display *disp, CdIcc *icc)
{
	GBytes *icc_bytes = NULL;
	GError *err = NULL;
	struct randr_display_priv *pdisp = (struct randr_display_priv *) disp;
	if (! pdisp->crtc) /* is display currently off? */
		return;
	if (icc) {
		icc_bytes = cd_icc_save_data (icc, CD_ICC_SAVE_FLAGS_NONE, &err);
		if (! icc_bytes) {
			g_warning ("unable to get ICC data: %s", err->message);
			g_clear_error (&err);
		}
	}
	apply_gamma (pdisp, icc);
	apply_icc (pdisp, icc_bytes);
	if (icc_bytes)
		g_bytes_unref (icc_bytes);
}

struct randr_display *
randr_conn_private_find_display (struct randr_conn *conn,
				 const gchar *key, guint offset)
{
	guint i;

	if (! conn->dpy)
		return NULL;

	for (i = 0; i < conn->displays->len; ++i) {
		struct randr_display *cand = (struct randr_display *)
					     g_ptr_array_index (conn->displays, i);
		if (! strcmp (G_STRUCT_MEMBER (const gchar *, cand, offset), key))
			return cand;
	}
	return NULL;
}

/* vim: set ts=8 sw=8 tw=0 : */

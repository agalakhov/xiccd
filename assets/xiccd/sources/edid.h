
#ifndef __EDID_H__
#define __EDID_H__

#include <colord.h>
#include <glib.h>

struct edid {
	const gchar	*cksum;

	const gchar	*vendor;
	const gchar	*model;
	const gchar	*serial;

	gchar		pnpid[4];

	gboolean	srgb;

	CdColorYxy	red;
	CdColorYxy	green;
	CdColorYxy	blue;
	CdColorYxy	white;
	double		gamma;
};

void edid_parse (struct edid *edid, gconstpointer edid_data, gsize edid_size);
void edid_free (struct edid *edid);

#endif /* __EDID_H__ */

/* vim: set ts=8 sw=8 tw=0 : */

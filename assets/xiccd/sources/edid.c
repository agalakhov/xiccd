
#include "edid.h"
#include <colord.h>
#include <errno.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef PNP_IDS
#define PNP_IDS "/usr/share/hwdata/pnp.ids"
#endif

static const gchar *edid_resolve_pnpid (const gchar *id);

void
edid_free (struct edid *edid)
{
	if (edid->cksum)
		g_free ((gpointer) edid->cksum);
	if (edid->vendor)
		g_free ((gpointer) edid->vendor);
	if (edid->model)
		g_free ((gpointer) edid->model);
	if (edid->serial)
		g_free ((gpointer) edid->serial);
}

static inline gdouble
fraction (guint8 lo, guint lobit, guint8 hi)
{
	guint16 val = lo & (3 << lobit);
	val >>= lobit;
	val |= ((guint16) hi) << 2;
	return ((gdouble) val / 1024);
}

static inline void
edid_parse_string (const gchar **retval, const guint8 *s)
{
	guint badness = 0;
	gchar *p;
	gchar *str;
	/* Cleanup and strip string */
	str = g_strndup ((const gchar *)s, 12);
	g_strdelimit (str, "\r\n", '\0');
	g_strstrip (str);
	/* Remove unprintable characters */
	for (p = str; *p; ++p) {
		if (! g_ascii_isprint (*p)) {
			*p = '-';
			++badness;
		}
	}
	/* If nothing useful is left, delete everything */
	if (! *str || badness > 4) {
		g_free (str);
	} else {
		if (*retval)
			g_free ((gpointer) *retval);
		*retval = str;
	}
}

static inline void
edid_parse_descriptor (struct edid *edid, const guint8 *d)
{
	/* Header */
	if ((d[0] | d[1] | d[2] | d[4]) != 0)
		return; /* this is a timing descriptor */

	switch (d[3]) { /* descriptor type */
	case 0xFF: /* monitor serial */
		edid_parse_string (&edid->serial, d + 5);
		break;
	case 0xFE: /* unspecified text */
		break;
	case 0xFC: /* monitor name */
		edid_parse_string (&edid->model, d + 5);
		break;
	case 0xFB: /* white point */
		g_warning ("0xFB white point not implemented yet");
		break;
	case 0xF9: /* colour management data */
		g_warning ("0xF9 colour management not implemented yet");
		break;
	default:
		return; /* do not want to parse this */
	}
}

void
edid_parse (struct edid *edid, gconstpointer edid_data, gsize edid_size)
{
	const guint8 *d = (const guchar *) edid_data;
	guint i;
	guint8 cksum;
	guint32 s;

	memset (edid, 0, sizeof(*edid));

	if (! edid_data)
		edid_size = 0;
	edid->cksum = g_compute_checksum_for_data (G_CHECKSUM_MD5, edid_data, edid_size);

	if (edid_size == 0) {
		g_warning ("EDID is empty");
		return;
	}

	if (edid_size < 128) {
		g_warning ("EDID too short");
		return;
	}

	if (memcmp (d, "\x00\xFF\xFF\xFF\xFF\xFF\xFF\x00", 8)) {
		g_warning ("EDID header bad");
		return;
	}

	/* Sum of first 128 bytes of EDID should always be zero */
	cksum = 0;
	for (i = 0; i < 128; ++i)
		cksum += d[i];
	if (cksum) {
		g_warning ("EDID CRC bad");
		return;
	}

	/* Encoded PNP ID in bytes 8 and 9 */
	edid->pnpid[0] = 'A' - 1 + ((d[8] >> 2) & 0x1F);
	edid->pnpid[1] = 'A' - 1 + ((d[8] << 3) & 0x18) + ((d[9] >> 5) & 0x07);
	edid->pnpid[2] = 'A' - 1 + (d[9] & 0x1F);
	edid->pnpid[3] = '\0';

	/* maybe there isn't a ASCII serial number descriptor, so use this instead */
	s = (guint32) d[0x0C+0];
	s += (guint32) d[0x0C+1] * 0x100;
	s += (guint32) d[0x0C+2] * 0x10000;
	s += (guint32) d[0x0C+3] * 0x1000000;
	if (s > 0)
		edid->serial = g_strdup_printf ("%" G_GUINT32_FORMAT, s);

	/* Byte 23 = gamma * 100 - 100 */
	if (d[23] == 0xFF)
		edid->gamma = -1.0;
	else
		edid->gamma = 1 + ((gdouble) d[23]) / 100;

	/* Byte 24 bit 2 = sRGB */
	edid->srgb = (d[24] & (1 << 2)) ? TRUE : FALSE;

	/* Color management data */
	edid->red.x   = fraction (d[25], 6, d[27]);
	edid->red.y   = fraction (d[25], 4, d[28]);
	edid->green.x = fraction (d[25], 2, d[29]);
	edid->green.y = fraction (d[25], 0, d[30]);
	edid->blue.x  = fraction (d[26], 6, d[31]);
	edid->blue.y  = fraction (d[26], 4, d[32]);
	edid->white.x = fraction (d[26], 2, d[33]);
	edid->white.y = fraction (d[26], 0, d[34]);
	edid->white.Y = 1.0;

	/* Extended info */
	edid->vendor = edid_resolve_pnpid (edid->pnpid);
	g_debug ("PNP: vendor=[%s]'%s'", edid->pnpid, edid->vendor);

	/* start of first descriptor = 54 */
	/* end of descriptor block = 126 */
	/* size of each descriptor = 18 */
	for (i = 54; i < 126; i += 18) {
		edid_parse_descriptor (edid, d + i);
	}

	g_debug ("EDID: monitor vendor=[%s]'%s' product='%s' serial='%s'",
		 edid->pnpid, edid->vendor, edid->model, edid->serial);
	g_debug ("EDID: red=(%f,%f) green=(%f,%f) blue=(%f,%f) white=(%f,%f) gamma=%f",
		 edid->red.x, edid->red.y,
		 edid->green.x, edid->green.y,
		 edid->blue.x, edid->blue.y,
		 edid->white.x, edid->white.y,
		 edid->gamma);
}


static const gchar *
edid_resolve_pnpid (const gchar *id)
{
	const gchar *retval = NULL;

	FILE *fd = fopen (PNP_IDS, "r");
	if (! fd)
		return NULL;

	while (! feof (fd)) {
		gchar str[1024];
		errno = 0;
		if (! fgets (str, sizeof(str), fd)) {
			if (errno)
				g_critical ("error reading %s: %s",
					    PNP_IDS, strerror (errno));
			break;
		}
		if ((str[3] != '\t') || (str[strlen(str) - 1] != '\n')) {
			g_critical ("broken file %s", PNP_IDS);
			continue;
		}
		str[strlen(str) - 1] = '\0';
		str[3] = '\0';
		if (! strcmp (str, id)) {
			retval = g_strdup (str + 4);
			break;
		}
	}

	fclose (fd);

	return retval;
}

/* vim: set ts=8 sw=8 tw=0 : */

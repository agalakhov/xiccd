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

#ifndef __EDID_H__
#define __EDID_H__

#include <glib.h>
#include <colord.h>

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

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

#ifndef __ICC_H__
#define __ICC_H__

#include <glib.h>

#include <colord.h>

#include <X11/extensions/Xrandr.h>

struct edid;

void icc_to_gamma (XRRCrtcGamma *gamma, CdIcc *icc);
CdIcc *icc_from_edid (const struct edid *edid);
gchar *icc_identify (GFile *file);

#endif /* __ICC_H__ */

/* vim: set ts=8 sw=8 tw=0 : */

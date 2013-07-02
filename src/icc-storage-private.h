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

#ifndef __ICC_STORAGE_PRIVATE_H__
#define __ICC_STORAGE_PRIVATE_H__


#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

struct edid;

struct icc_storage {
	GObject		*object;
	GFile		*dir;
	GFileMonitor	*mon;
	GHashTable	*hash;
};

enum {
	SIG_FILE_ADDED,
	SIG_FILE_REMOVED,
	N_SIG
};

extern guint icc_signals[N_SIG];

void icc_storage_private_init (struct icc_storage *stor);
void icc_storage_private_finalize (struct icc_storage *stor);
void icc_storage_private_update (struct icc_storage *stor);
void icc_storage_private_push_edid (struct icc_storage *stor, const struct edid *edid);

G_END_DECLS

#endif /* __ICC_STORAGE_PRIVATE_H__ */

/* vim: set ts=8 sw=8 tw=0 : */

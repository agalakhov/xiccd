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

#ifndef __ICC_STORAGE_H__
#define __ICC_STORAGE_H__

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

struct edid;

#define ICC_TYPE_STORAGE \
	(icc_storage_get_type ())
#define ICC_STORAGE(o) \
	(G_TYPE_CHECK_INSTANCE_CAST ((o), ICC_TYPE_STORAGE, IccStorage))
#define ICC_IS_STORAGE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), ICC_TYPE_STORAGE))
#define ICC_STORAGE_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST ((klass), ICC_TYPE_STORAGE, IccStorageClass))
#define ICC_IS_STORAGE_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE ((klass), ICC_TYPE_STORAGE))
#define ICC_STORAGE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS ((obj), ICC_TYPE_STORAGE, IccStorageClass))

typedef struct _IccStorage {
	GObject parent;
	struct icc_storage *priv;
} IccStorage;


typedef struct _IccStorageClass {
	GObjectClass parent;
	void (*file_added) (IccStorage *stor, const gchar *filename, const gchar *id);
	void (*file_removed) (IccStorage *stor, const gchar *filename, const gchar *id);
} IccStorageClass;


GType icc_storage_get_type (void);

IccStorage *icc_storage_new (void);
void icc_storage_update (IccStorage *stor);
void icc_storage_push_edid (IccStorage *stor, const struct edid *edid);

G_END_DECLS

#endif /* __ICC_STORAGE_H__ */

/* vim: set ts=8 sw=8 tw=0 : */

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

#include "icc-storage.h"
#include "icc-storage-private.h"

#include <string.h>

#include <glib.h>
#include <glib-object.h>

guint icc_signals[N_SIG];

G_DEFINE_TYPE (IccStorage, icc_storage, G_TYPE_OBJECT)

#define ICC_STORAGE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE ((obj), ICC_TYPE_STORAGE, struct icc_storage))


static void
icc_storage_init (IccStorage *self)
{
	self->priv = ICC_STORAGE_GET_PRIVATE (self);
	memset (self->priv, 0, sizeof (*self->priv));
}

static GObject *
icc_storage_constructor (GType type, guint n_params, GObjectConstructParam *params)
{
	GObject *obj = G_OBJECT_CLASS (icc_storage_parent_class)
		     ->constructor (type, n_params, params);

	ICC_STORAGE (obj)->priv->object = obj;
	icc_storage_private_init (ICC_STORAGE (obj)->priv);

	return obj;
}

static void
icc_storage_finalize (GObject *self)
{
	icc_storage_private_finalize (ICC_STORAGE (self)->priv);
}

static void
icc_storage_class_init (IccStorageClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (struct icc_storage));

	obj_class->constructor = icc_storage_constructor;
	obj_class->finalize = icc_storage_finalize;

	icc_signals[SIG_FILE_ADDED] = g_signal_new ("file_added",
		G_TYPE_FROM_CLASS (obj_class), G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (IccStorageClass, file_added),
		NULL, NULL, NULL,
		G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_STRING);

	icc_signals[SIG_FILE_REMOVED] = g_signal_new ("file_removed",
		G_TYPE_FROM_CLASS (obj_class), G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (IccStorageClass, file_removed),
		NULL, NULL, NULL,
		G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_STRING);
}


IccStorage *
icc_storage_new (void)
{
	IccStorage *obj = ICC_STORAGE (g_object_new (ICC_TYPE_STORAGE, NULL));
	return obj;
}

void
icc_storage_update (IccStorage *stor)
{
	icc_storage_private_update (stor->priv);
}

void
icc_storage_push_edid (IccStorage *stor, const struct edid *edid)
{
	icc_storage_private_push_edid (stor->priv, edid);
}

/* vim: set ts=8 sw=8 tw=0 : */

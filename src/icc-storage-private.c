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

#include "icc-storage-private.h"

#include "icc.h"
#include "edid.h"

#include <glib.h>
#include <gio/gio.h>
#include <string.h>

#define STORAGE_PATH "icc"
#define PORTION_SIZE 16


static gboolean
file_is_icc (const gchar *name)
{
	gsize len = strlen (name);
	if (len < 4)
		return FALSE;
	if (strcmp (name + len - 4, ".icc") == 0)
		return TRUE;
	return FALSE;
}

static gchar *
identify_icc_file (GFile *file)
{
	gchar *retval, *tmp;
	gchar *data = NULL;
	gsize size = 0;
	GError *err = NULL;
	GBytes *icc;
	gboolean ret;

	ret = g_file_load_contents (file, NULL, &data, &size, NULL, &err);
	if (! ret) {
		g_critical ("unable to read %s: %s", g_file_get_path (file), err->message);
		g_error_free (err);
		return NULL;
	}

	icc = g_bytes_new_take (data, size);
	tmp = icc_identify (icc);
	g_bytes_unref (icc);
	retval = g_strdup_printf ("icc-%s", tmp);
	g_free (tmp);
	return retval;
}

static void
do_add_file (struct icc_storage *stor, GFile *file)
{
	GError *err = NULL;
	const gchar *name = g_file_get_path (file);
	const gchar *id = NULL;
	const gchar *content_type;
	GFileInfo *info;

	if (! file_is_icc (name))
		return;

	info = g_file_query_info (file, "standard::*", G_FILE_QUERY_INFO_NONE, NULL, &err);
	if (! info) {
		if (! g_error_matches (err, G_IO_ERROR, G_IO_ERROR_NOT_FOUND))
			g_critical ("unable to query info on file %s: %s", name, err->message);
		g_error_free (err);
		return;
	}

	content_type = g_file_info_get_content_type (info);
	g_debug ("found file %s (%s)", name, content_type);

	if (strcmp (content_type, "application/vnd.iccprofile") != 0)
		return;

	id = identify_icc_file (file);

	if (id && ! g_hash_table_contains (stor->hash, name)) {
		g_hash_table_insert (stor->hash, g_strdup (name), g_strdup (id));
		g_debug ("added profile '%s' from file %s", id, name);
		g_signal_emit (stor->object, icc_signals[SIG_FILE_ADDED], 0, name, id);
	}

	if (id)
		g_free ((gpointer) id);

	g_object_unref (info);
}

static void
do_remove_file (struct icc_storage *stor, GFile *file)
{
	const gchar *name = g_file_get_path (file);
	const gchar *id = NULL;

	if (! file_is_icc (name))
		return;

	g_debug ("disappeared file %s", name);

	id = (const gchar *) g_hash_table_lookup (stor->hash, name);
	if (! id)
		return;

	g_debug ("removed file %s containing profile '%s'", name, id);
	g_signal_emit (stor->object, icc_signals[SIG_FILE_REMOVED], 0, name, id);

	g_hash_table_remove (stor->hash, name);
}

static void
file_changed_sig (GFileMonitor *mon,
		  GFile *file, GFile *oth, GFileMonitorEvent event_type, gpointer user_data)
{
	struct icc_storage *stor = (struct icc_storage *) user_data;

	g_assert (mon == stor->mon);
	(void) oth;

	switch (event_type) {
	case G_FILE_MONITOR_EVENT_CREATED:
		do_add_file (stor, file);
		break;
	case G_FILE_MONITOR_EVENT_DELETED:
		do_remove_file (stor, file);
		break;
	default:
		break;
	}
}

void
icc_storage_private_init (struct icc_storage *stor)
{
	GError *err = NULL;
	gboolean ret;
	const gchar *name;
	GFile *dir;

	stor->hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

	name = g_build_filename (g_get_user_data_dir(), STORAGE_PATH, NULL);
	dir = g_file_new_for_path (name);
	g_debug ("looking for profiles in %s", name);
	g_free ((gpointer) name);

	ret = g_file_make_directory_with_parents (dir, NULL, &err);
	if (! ret) {
		if (err->code != G_IO_ERROR_EXISTS) {
			g_critical ("unable to create %s: %s", g_file_get_path (dir), err->message);
			g_error_free (err);
			return;
		} else {
			g_clear_error (&err);
		}
	}

	if (g_file_query_file_type (dir, G_FILE_QUERY_INFO_NONE, NULL) != G_FILE_TYPE_DIRECTORY) {
		g_critical ("not a directory: %s", g_file_get_path (dir));
		return;
	}

	stor->dir = dir;

	stor->mon = g_file_monitor_directory (stor->dir, G_FILE_MONITOR_NONE, NULL, &err);
	if (! stor->mon) {
		g_critical ("unable to monitor %s: %s", g_file_get_path (stor->dir), err->message);
		g_error_free (err);
		return;
	}

	g_signal_connect (stor->mon, "changed", G_CALLBACK (file_changed_sig), stor);
}


void
icc_storage_private_finalize (struct icc_storage *stor)
{
	if (stor->mon)
		g_object_unref (stor->mon);
	if (stor->dir)
		g_object_unref (stor->dir);
	if (stor->hash)
		g_hash_table_unref (stor->hash);
}


static void
next_files_cb (GObject *src, GAsyncResult *res, gpointer user_data)
{
	GFileEnumerator *enm = G_FILE_ENUMERATOR (src);
	struct icc_storage *stor = (struct icc_storage *) user_data;
	GError *err = NULL;
	GList *list;
	GList *it;
	guint n;

	list = g_file_enumerator_next_files_finish (enm, res, &err);
	if (! list && err) {
		g_critical ("g_file_enumerator_next_files_async() failed: %s", err->message);
		g_error_free (err);
		goto out;
	}

	n = 0;
	it = list;
	while (it) {
		GFile *child;
		GFileInfo *inf = G_FILE_INFO (it->data);
		it = g_list_next (it);
		++n;

		child = g_file_get_child (stor->dir, g_file_info_get_name (inf));
		do_add_file (stor, child);
		g_object_unref (child);

		g_object_unref (inf);
	}

	g_list_free (list);

	if (n == PORTION_SIZE) {
		g_object_ref (enm);
		g_file_enumerator_next_files_async (enm, PORTION_SIZE, G_PRIORITY_DEFAULT, NULL,
						    next_files_cb, stor);
	}

out:
	g_object_unref (enm);
}

static void
enumerate_children_cb (GObject *src, GAsyncResult *res, gpointer user_data)
{
	GFile *file = G_FILE (src);
	struct icc_storage *stor = (struct icc_storage *) user_data;
	GError *err = NULL;
	GFileEnumerator *enm;

	g_assert (file == stor->dir);

	enm = g_file_enumerate_children_finish (file, res, &err);
	if (! enm) {
		g_critical ("g_file_enumerate_children_async() failed: %s", err->message);
		g_error_free (err);
		return;
	}

	g_file_enumerator_next_files_async (enm, PORTION_SIZE, G_PRIORITY_DEFAULT, NULL,
					    next_files_cb, stor);
}

void
icc_storage_private_update (struct icc_storage *stor)
{
	if (! stor->dir)
		return;
	g_file_enumerate_children_async (stor->dir, "standard::*",
					 G_FILE_QUERY_INFO_NONE, G_PRIORITY_DEFAULT, NULL,
					 enumerate_children_cb, stor);
}

void
icc_storage_private_push_edid (struct icc_storage *stor, const struct edid *edid)
{
	gchar *tmpname;
	GFile *file;
	CdIcc *icc = NULL;
	GError *err = NULL;
	gboolean ret;

	tmpname = g_strdup_printf ("edid-%s.icc", edid->cksum);
	file = g_file_get_child (stor->dir, tmpname);
	g_free (tmpname);

	if (g_hash_table_contains (stor->hash, g_file_get_path (file))) {
		g_debug ("profile for edid %s already exists", edid->cksum);
		goto out;
	}

	icc = icc_from_edid (edid);
	if (! icc) {
		g_critical ("profile for EDID %s was not created", edid->cksum);
		goto out;
	}

	g_debug ("WRITING profile for EDID %s", edid->cksum);

	ret = cd_icc_save_file (icc, file, CD_ICC_SAVE_FLAGS_NONE, NULL, &err);
	if (! ret) {
		g_critical ("unable to write file %s: %s", g_file_get_path (file), err->message);
		g_error_free (err);
		goto out;
	}

	/* ensure we know about the file */
	do_add_file (stor, file);

out:
	if (icc)
		g_object_unref (icc);
	g_object_unref (file);
}

/* vim: set ts=8 sw=8 tw=0 : */

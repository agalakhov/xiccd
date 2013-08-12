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

#include "randr-conn.h"
#include "icc.h"

#include <glib.h>
#include <glib-unix.h>
#include <colord.h>
#include <string.h>

typedef struct _Daemon {
	GMainLoop	*loop;
	RandrConn	*rcon;
	CdClient	*cli;
	CdIccStore	*stor;
} Daemon;


static void
show_version (void)
{
	g_print ("%s - Version %s\n", g_get_application_name (), VERSION);
	exit (0);
}

static struct {
	const gchar	*display;
} config;

static GOptionEntry config_entries[] = {
	{ "version", 'V', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK,
		(void*)(intptr_t) show_version,
		"Show version", NULL },
	{ "display", 'd', 0, G_OPTION_ARG_STRING, &config.display,
		"X server to contact", NULL },
	{ NULL }
};

static void
config_free (void)
{
	if (config.display)
		g_free ((gpointer) config.display);
}


static inline gboolean
signal_term (gpointer loop)
{
	g_main_loop_quit ((GMainLoop *) loop);
	return FALSE;
}

static gchar *
profile_id (CdIcc *icc)
{
	return g_strdup_printf ("icc-%s", cd_icc_get_checksum (icc));
}

static gchar *
profile_filename (const struct edid *edid)
{
	return g_strdup_printf ("edid-%s.icc", edid->cksum);
}

static void
update_device_cb (GObject *src, GAsyncResult *res, gpointer user_data)
{
	CdDevice *device = CD_DEVICE (src);
	Daemon *daemon = (Daemon *) user_data;
	GError *err = NULL;
	const gchar *xrandr_id;
	struct randr_display *disp;
	CdProfile *profile = NULL;
	gboolean ret;

	ret = cd_device_connect_finish (device, res, &err);
	if (! ret) {
		g_critical ("unable to connect to device: %s", err->message);
		g_error_free (err);
		goto out;
	}

	if (cd_device_get_kind (device) != CD_DEVICE_KIND_DISPLAY) {
		g_debug ("ignoring device %s: not a display\n", cd_device_get_id (device));
		goto out;
        }

	xrandr_id = cd_device_get_id (device);
	disp = randr_conn_find_display (daemon->rcon, xrandr_id);
	if (! disp) {
		g_critical ("device '%s' does not exist", xrandr_id);
		goto out;
	}

	profile = cd_device_get_default_profile (device);

	if (profile) {
		/* Use sync mode. We do not want race conditions here. */
		ret = cd_profile_connect_sync (profile, NULL, &err);
		if (! ret) {
			g_critical ("unable to connect to profile: %s", err->message);
			g_error_free (err);
		} else {
			CdIcc *icc = cd_profile_load_icc (profile, CD_ICC_LOAD_FLAGS_FALLBACK_MD5,
							  NULL, &err);
			if (! icc) {
				g_critical ("can't get profile for display %s: %s", disp->name,
										    err->message);
				g_clear_error (&err);
			}
			g_debug ("loading profile '%s' for display %s",
				 icc ? cd_icc_get_filename (icc) : "(none)", disp->name);
			randr_display_apply_icc (disp, icc);
		}
	} else {
		g_debug ("unloading profile for display %s", disp->name);
		randr_display_apply_icc (disp, NULL);
	}

out:

	if (profile)
		g_object_unref (profile);
}

static void
cd_device_add_profile_cb (GObject *src, GAsyncResult *res, gpointer user_data)
{
	CdDevice *device = CD_DEVICE (src);
	GError *err = NULL;
	gboolean ret;
	(void) user_data;

	ret = cd_device_add_profile_finish (device, res, &err);
	if (! ret) {
		if (! g_error_matches (err, CD_DEVICE_ERROR, CD_DEVICE_ERROR_PROFILE_ALREADY_ADDED))
			g_critical ("unable to add device profile: %s", err->message);
		g_error_free (err);
		return;
	}

	g_object_unref (device);
}

static void
update_profile_cb (GObject *src, GAsyncResult *res, gpointer user_data)
{
	CdProfile *profile = CD_PROFILE (src);
	Daemon *daemon = (Daemon *) user_data;
	GError *err = NULL;
	const gchar *edid_md5;
	struct randr_display *disp;
	CdDevice *device = NULL;
	gboolean ret;

	ret = cd_profile_connect_finish (profile, res, &err);
	if (! ret) {
		g_critical ("unable to connect to profile: %s", err->message);
		g_error_free (err);
		goto out;
	}

	edid_md5 = cd_profile_get_metadata_item (profile, CD_PROFILE_METADATA_EDID_MD5);
	if (! edid_md5)
		goto out;

	disp = randr_conn_find_display_edid (daemon->rcon, edid_md5);
	if (! disp)
		goto out;

	g_debug ("profile %s matches display %s", cd_profile_get_id (profile), disp->name);

	/* Use sync mode. We do not want race conditions here. */
	device = cd_client_find_device_sync (daemon->cli, disp->name, NULL, &err);
	if (! device) {
		g_critical ("unable to find device %s: %s", disp->name, err->message);
		g_error_free (err);
		goto out;
	}

	ret = cd_device_connect_sync (device, NULL, &err);
	if (! ret) {
		g_critical ("unable to connect to device: %s", err->message);
		g_error_free (err);
		goto out;
	}

	g_object_ref (device);
	cd_device_add_profile (device, CD_DEVICE_RELATION_SOFT, profile, NULL,
			       cd_device_add_profile_cb, daemon);

out:

	if (device)
		g_object_unref (device);
	g_object_unref (profile);
}


static void
update_device (CdDevice *device, Daemon *daemon)
{
	cd_device_connect (device, NULL, update_device_cb, daemon);
}

static void
update_profile (CdProfile *profile, Daemon *daemon)
{
	cd_profile_connect (profile, NULL, update_profile_cb, daemon);
}


static void
cd_profile_added_sig (CdClient *client, CdProfile *profile, Daemon *daemon)
{
	g_assert (client == daemon->cli);
	update_profile (profile, daemon);
}

static void
cd_device_added_sig (CdClient *client, CdDevice *device, Daemon *daemon)
{
	g_assert (client == daemon->cli);
	update_device (device, daemon);
}

static void
cd_device_changed_sig (CdClient *client, CdDevice *device, Daemon *daemon)
{
	g_assert (client == daemon->cli);
	update_device (device, daemon);
}


static void
cd_create_device_cb (GObject *src, GAsyncResult *res, gpointer user_data)
{
	Daemon *daemon = (Daemon *) user_data;
	GError *err = NULL;
	CdDevice *dev;

	g_assert (CD_CLIENT (src) == daemon->cli);

	dev = cd_client_create_device_finish (daemon->cli, res, &err);
	if (! dev) {
		if (err->domain != CD_CLIENT_ERROR || err->code != CD_CLIENT_ERROR_ALREADY_EXISTS)
			g_critical ("failed to create colord device: %s", err->message);
		g_error_free (err);
		return;
	}

	g_object_unref (dev);
}


static void
create_profile_from_edid(CdIccStore *store, const struct edid *edid)
{
	gchar *filename;
	gchar *filepath;
	GFile *file;
	CdIcc *icc = NULL;
	GError *err = NULL;
	gboolean ret;

	filename = profile_filename (edid);
	filepath = g_build_filename (g_get_user_data_dir (), "icc", filename, NULL);
	g_free (filename);
	file = g_file_new_for_path (filepath);
	g_free (filepath);

	icc = cd_icc_store_find_by_filename (store, g_file_get_path (file));
	if (icc) {
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

out:
	if (icc)
		g_object_unref (icc);
	g_object_unref (file);
}


static void
randr_display_added_sig (RandrConn *conn, struct randr_display *disp, Daemon *daemon)
{
	GHashTable *props = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, NULL);

	g_assert (conn == daemon->rcon);

	g_debug ("added display: '%s'", disp->name);

	create_profile_from_edid (daemon->stor, &disp->edid);

	g_hash_table_insert (props, CD_DEVICE_PROPERTY_KIND,
			     (gchar *) cd_device_kind_to_string (CD_DEVICE_KIND_DISPLAY));
	g_hash_table_insert (props, CD_DEVICE_PROPERTY_MODE,
			     (gchar *) cd_device_mode_to_string (CD_DEVICE_MODE_PHYSICAL));
	g_hash_table_insert (props, CD_DEVICE_PROPERTY_COLORSPACE,
			     (gchar *) cd_colorspace_to_string (CD_COLORSPACE_RGB));

	g_hash_table_insert (props, CD_DEVICE_PROPERTY_VENDOR, (gchar *) disp->edid.vendor);
	g_hash_table_insert (props, CD_DEVICE_PROPERTY_MODEL, (gchar *) disp->edid.model);
	g_hash_table_insert (props, CD_DEVICE_PROPERTY_SERIAL, (gchar *) disp->edid.serial);

	g_hash_table_insert (props, CD_DEVICE_METADATA_XRANDR_NAME, (gchar *) disp->xrandr_name);

	g_hash_table_insert (props, CD_DEVICE_METADATA_OUTPUT_PRIORITY,
			     disp->is_primary ? CD_DEVICE_METADATA_OUTPUT_PRIORITY_PRIMARY
					      : CD_DEVICE_METADATA_OUTPUT_PRIORITY_SECONDARY);

	if (disp->is_laptop)
		g_hash_table_insert (props, CD_DEVICE_PROPERTY_EMBEDDED, NULL);

	cd_client_create_device (daemon->cli, disp->name, CD_OBJECT_SCOPE_TEMP,
				 props, NULL, cd_create_device_cb, daemon);

	g_hash_table_unref (props);
}

static void
cd_device_remove_cb (GObject *src, GAsyncResult *res, gpointer user_data)
{
	Daemon *daemon = (Daemon *) user_data;
	GError *err = NULL;
	gboolean ret;

	g_assert (CD_CLIENT (src) == daemon->cli);

	ret = cd_client_delete_device_finish (daemon->cli, res, &err);
	if (! ret) {
		g_critical ("device not removed: %s", err->message);
		g_error_free (err);
		return;
	}
}

static void
randr_display_removed_sig (RandrConn *conn, struct randr_display *disp, Daemon *daemon)
{
	CdDevice *device = NULL;
	GError *err = NULL;

	g_assert (conn == daemon->rcon);

	g_debug ("removed display: '%s'", disp->name);

	/* We do not want race conditions here */
	device = cd_client_find_device_sync (daemon->cli, disp->name,
					    NULL, &err);
	if (! device) {
		g_debug ("device %s not found so not removed: %s", disp->name, err->message);
		g_error_free (err);
		return;
	}

	cd_client_delete_device (daemon->cli, device, NULL,
				 cd_device_remove_cb, daemon);

	g_object_unref (device);
}


static void
cd_existing_devices_cb (GObject *src, GAsyncResult *res, gpointer user_data)
{
	Daemon *daemon = (Daemon *) user_data;
	GError *err = NULL;
	GPtrArray *devs;

	g_assert (CD_CLIENT (src) == daemon->cli);

	devs = cd_client_get_devices_finish (daemon->cli, res, &err);
	if (! devs) {
		g_critical ("Failed to get device list: %s", err->message);
		g_error_free (err);
		return;
	}

	g_ptr_array_foreach (devs, (GFunc) update_device, daemon);
	g_ptr_array_unref (devs);
}

static void
cd_existing_profiles_cb (GObject *src, GAsyncResult *res, gpointer user_data)
{
	Daemon *daemon = (Daemon *) user_data;
	GError *err = NULL;
	GPtrArray *profs;

	g_assert (CD_CLIENT (src) == daemon->cli);

	profs = cd_client_get_profiles_finish (daemon->cli, res, &err);
	if (! profs) {
		g_critical ("Failed to get profile list: %s", err->message);
		g_error_free (err);
		return;
	}

	g_ptr_array_foreach (profs, (GFunc) update_profile, daemon);
	g_ptr_array_unref (profs);
}

static void
cd_client_create_profile_cb (GObject *src, GAsyncResult *res, gpointer user_data)
{
	Daemon *daemon = (Daemon *) user_data;
	GError *err = NULL;
	CdProfile *profile;

	g_assert (CD_CLIENT (src) == daemon->cli);

	profile = cd_client_create_profile_finish (daemon->cli, res, &err);
	if (! profile) {
		if (! g_error_matches (err, CD_CLIENT_ERROR, CD_CLIENT_ERROR_ALREADY_EXISTS))
			g_critical ("unable to create profile: %s", err->message);
		g_error_free (err);
		return;
	}
	g_object_unref (profile);
}

static void
cd_client_delete_profile_cb (GObject *src, GAsyncResult *res, gpointer user_data)
{
	Daemon *daemon = (Daemon *) user_data;
	GError *err = NULL;
	gboolean ret;

	g_assert (CD_CLIENT (src) == daemon->cli);

	ret = cd_client_delete_profile_finish (daemon->cli, res, &err);
	if (! ret) {
		g_critical ("unable to remove profile: %s", err->message);
		g_error_free (err);
	}
}

static void
cd_icc_store_file_added_sig (CdIccStore *stor, CdIcc *icc, Daemon *daemon)
{
	GHashTable *props;
	gchar *id;

	g_assert (stor == daemon->stor);

	props = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, NULL);
	id = profile_id (icc);

	g_hash_table_insert (props, CD_PROFILE_PROPERTY_FILENAME,
			     (gchar *) cd_icc_get_filename (icc));
	g_hash_table_insert (props, CD_PROFILE_METADATA_FILE_CHECKSUM,
			     (gchar *) cd_icc_get_checksum (icc));

	cd_client_create_profile (daemon->cli, id, CD_OBJECT_SCOPE_TEMP, props,
			       NULL, cd_client_create_profile_cb, daemon);

	g_free (id);
	g_hash_table_unref (props);
}

static void
cd_icc_store_file_removed_sig (CdIccStore *stor, CdIcc *icc, Daemon *daemon)
{
	CdProfile *prof = NULL;
	GError *err = NULL;
	gchar *id;

	g_assert (stor == daemon->stor);

	id = profile_id (icc);

	prof = cd_client_find_profile_sync (daemon->cli, id, NULL, &err);
	if (! prof) {
		g_debug ("profile not found so not removed: %s: %s", id, err->message);
		g_error_free (err);
		goto out;
	}

	cd_client_delete_profile (daemon->cli, prof, NULL, cd_client_delete_profile_cb, daemon);
out:
	g_free (id);
}

static void
cd_connect_cb (GObject *src, GAsyncResult *res, gpointer user_data)
{
	Daemon *daemon = (Daemon *) user_data;
	GError *err = NULL;
	gboolean ret;

	g_assert (CD_CLIENT (src) == daemon->cli);

	ret = cd_client_connect_finish (daemon->cli, res, &err);
	if (! ret) {
		g_critical ("Failed to connect to colord: %s", err->message);
		g_error_free (err);
		return;
	}

	g_signal_connect (daemon->cli, "profile-added",
			  G_CALLBACK (cd_profile_added_sig), daemon);

	g_signal_connect (daemon->cli, "device-added",
			  G_CALLBACK (cd_device_added_sig), daemon);

	g_signal_connect (daemon->cli, "device-changed",
			  G_CALLBACK (cd_device_changed_sig), daemon);

	g_signal_connect (daemon->rcon, "display-added",
			  G_CALLBACK (randr_display_added_sig), daemon);

	g_signal_connect (daemon->rcon, "display-removed",
			  G_CALLBACK (randr_display_removed_sig), daemon);

	g_signal_connect (daemon->stor, "added",
			  G_CALLBACK (cd_icc_store_file_added_sig), daemon);

	g_signal_connect (daemon->stor, "removed",
			  G_CALLBACK (cd_icc_store_file_removed_sig), daemon);

	cd_icc_store_set_load_flags (daemon->stor, CD_ICC_LOAD_FLAGS_FALLBACK_MD5);

	ret = cd_icc_store_search_kind (daemon->stor, CD_ICC_STORE_SEARCH_KIND_USER,
				  CD_ICC_STORE_SEARCH_FLAGS_CREATE_LOCATION, NULL, &err);
	if (! ret) {
		g_critical ("unable to watch profile store: %s", err->message);
		g_clear_error (&err);
	}

	cd_client_get_devices_by_kind (daemon->cli,
				       CD_DEVICE_KIND_DISPLAY,
				       NULL,
				       cd_existing_devices_cb,
				       daemon);

	cd_client_get_profiles (daemon->cli,
				NULL,
				cd_existing_profiles_cb,
				daemon);

	randr_conn_update (daemon->rcon);
}



int
main (int argc, char *argv[])
{
	int retval = 1;

	Daemon daemon;
	GOptionContext *opt;
	GError *err = NULL;
	gboolean ret = 0;

	g_set_prgname (PACKAGE);

	memset (&config, 0, sizeof (config));
	opt = g_option_context_new (NULL);
	g_option_context_set_summary (opt, "X color management daemon");
	g_option_context_add_main_entries (opt, config_entries, 0);
	ret = g_option_context_parse (opt, &argc, &argv, &err);
	g_option_context_free (opt);
	if (! ret) {
		config_free ();
		g_print ("%s\n", err->message);
		g_error_free (err);
		return retval;
	}

	daemon.loop = g_main_loop_new (NULL, FALSE);
	daemon.rcon = randr_conn_new (config.display);
	daemon.cli = cd_client_new ();
	daemon.stor = cd_icc_store_new ();

	config_free ();

	g_unix_signal_add (SIGTERM, signal_term, daemon.loop);
	g_unix_signal_add (SIGINT, signal_term, daemon.loop);

	cd_client_connect (daemon.cli, NULL, cd_connect_cb, &daemon);

	g_main_loop_run (daemon.loop);

	g_warning ("Exiting");

	retval = 0;

	g_object_unref (daemon.stor);
	g_object_unref (daemon.cli);
	g_object_unref (daemon.rcon);
	g_main_loop_unref (daemon.loop);

	return retval;
}

/* vim: set ts=8 sw=8 tw=0 : */

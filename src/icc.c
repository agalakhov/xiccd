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

#include "icc.h"

#include "edid.h"

#include <glib.h>
#include <stdlib.h>

#include <colord.h>

#include <X11/extensions/Xrandr.h>

static void
reset_gamma (XRRCrtcGamma *gamma)
{
	int i;
	for (i = 0; i < gamma->size; ++i) {
		guint value = i * 0xFFFF / (gamma->size - 1);
		gamma->red[i]   = value;
		gamma->green[i] = value;
		gamma->blue[i]  = value;
	}
}

void
icc_to_gamma (XRRCrtcGamma *gamma, GBytes *iccraw)
{
	GError *err = NULL;
	CdIcc *icc = NULL;
	GPtrArray *vcgt = NULL;
	gboolean ret;
	guint i;

	if (! iccraw) {
		reset_gamma (gamma);
		goto out;
	}

	icc = cd_icc_new ();
	ret = cd_icc_load_data (icc, g_bytes_get_data (iccraw, NULL), g_bytes_get_size (iccraw),
				CD_ICC_LOAD_FLAGS_NONE, &err);
	if (! ret) {
		g_critical ("corrupt ICC profile: %s", err->message);
		g_error_free (err);
		reset_gamma (gamma);
		goto out;
	}

	vcgt = cd_icc_get_vcgt (icc, gamma->size, NULL);
	if (! vcgt) {
		g_debug ("ICC profile has no VCGT");
		reset_gamma (gamma);
		goto out;
	}

	g_assert (vcgt->len == (guint) gamma->size);

	for (i = 0; i < vcgt->len; ++i) {
		CdColorRGB *color = (CdColorRGB *) g_ptr_array_index (vcgt, i);
		gamma->red[i]   = color->R * 0xFFFF;
		gamma->green[i] = color->G * 0xFFFF;
		gamma->blue[i]  = color->B * 0xFFFF;
	}

out:
	if (vcgt)
		g_ptr_array_unref (vcgt);
	if (icc)
		g_object_unref (icc);
}


CdIcc *
icc_from_edid (const struct edid *edid)
{
	CdIcc *icc;
	GError *err = NULL;
	gboolean ret;

	if (! edid)
		return NULL;

	icc = cd_icc_new ();
	ret = cd_icc_create_from_edid (icc, edid->gamma,
				       &edid->red, &edid->green, &edid->blue, &edid->white,
				       &err);
	if (! ret) {
		g_critical ("unable to create profile from EDID: %s", err->message);
		g_error_free (err);
		return NULL;
	}

	cd_icc_set_kind (icc, CD_PROFILE_KIND_DISPLAY_DEVICE);

	/* deliberately not translated */
	cd_icc_set_copyright (icc, NULL, "This profile is free of known copyright restrictions.");

	cd_icc_set_description (icc, NULL, edid->model);
	cd_icc_set_model (icc, NULL, edid->model);
	cd_icc_set_manufacturer (icc, NULL, edid->vendor);

	cd_icc_add_metadata (icc, CD_PROFILE_METADATA_CMF_PRODUCT, PACKAGE);
	cd_icc_add_metadata (icc, CD_PROFILE_METADATA_CMF_BINARY, g_get_prgname ());
	cd_icc_add_metadata (icc, CD_PROFILE_METADATA_CMF_VERSION, VERSION);

	cd_icc_add_metadata (icc, CD_PROFILE_METADATA_EDID_MD5, edid->cksum);
	cd_icc_add_metadata (icc, CD_PROFILE_METADATA_EDID_MODEL, edid->model);
	if (edid->serial)
		cd_icc_add_metadata (icc, CD_PROFILE_METADATA_EDID_SERIAL, edid->serial);
	cd_icc_add_metadata (icc, CD_PROFILE_METADATA_EDID_MNFT, edid->pnpid);
	cd_icc_add_metadata (icc, CD_PROFILE_METADATA_EDID_VENDOR, edid->vendor);

	return icc;
}

gchar *
icc_identify (GFile *file)
{
	gchar *retval = NULL;
	CdIcc *icc = cd_icc_new ();
	gboolean ret;

	ret = cd_icc_load_file (icc, file, CD_ICC_LOAD_FLAGS_FALLBACK_MD5, NULL, NULL);
	if (! ret)
		goto out;

	retval = g_strdup (cd_icc_get_checksum (icc));

out:
	g_object_unref (icc);
	return retval;
}


/* vim: set ts=8 sw=8 tw=0 : */

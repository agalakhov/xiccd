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

/* TODO: newer versions of libcolord have ICC API. Remove this file if possible. */

#include "icc.h"

#include "edid.h"

#include <glib.h>
#include <stdlib.h>

#include <colord.h>
#include <lcms2.h>

#include <X11/extensions/Xrandr.h>

static wchar_t *
utf2wchar (const char *text)
{
	wchar_t *retval = NULL;
	size_t len;
	size_t clen;

	if (! text)
		return NULL;

	len = mbstowcs (NULL, text, 0);
	if (len == (size_t) -1) {
		g_critical ("invalid multibyte sequence");
		return retval;
	}

	retval = g_new0 (wchar_t, len + 1);
	clen = mbstowcs (retval, text, len);
	g_assert (clen == len);
	return retval;
}

static gboolean
write_cms_tag (cmsHPROFILE prof, cmsTagSignature tag, const gchar *val)
{
	gboolean retval = FALSE;
	cmsMLU *mlu = cmsMLUalloc (0, 1);
	cmsMLUsetASCII (mlu, "en", "US", val);
	retval = cmsWriteTag (prof, tag, mlu);
	cmsMLUfree (mlu);
	return retval;
}

static gboolean
add_cms_dict_entry (cmsHANDLE dict, const gchar *key, const gchar *val)
{
	gboolean retval = FALSE;
	wchar_t *wkey;
	wchar_t *wval;
	wkey = utf2wchar (key);
	wval = utf2wchar (val);
	if (wkey && wval)
		retval = cmsDictAddEntry (dict, wkey, wval, NULL, NULL);
	if (wval)
		g_free (wval);
	if (wkey)
		g_free (wkey);
	return retval;
}


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
icc_to_gamma (XRRCrtcGamma *gamma, GBytes *icc)
{
	cmsHPROFILE prof = NULL;
	const cmsToneCurve **vcgt;

	if (! icc) {
		reset_gamma (gamma);
		return;
	}

	prof = cmsOpenProfileFromMem (g_bytes_get_data (icc, NULL), g_bytes_get_size (icc));
	if (! prof) {
		g_critical ("corrupt ICC profile");
		reset_gamma (gamma);
		return;
	}

	vcgt = cmsReadTag (prof, cmsSigVcgtTag);
	if (vcgt && *vcgt) {
		int i;
		for (i = 0; i < gamma->size; ++i) {
			cmsFloat32Number in =
				(double) i / (double) (gamma->size - 1);
			gamma->red[i] =
				cmsEvalToneCurveFloat (vcgt[0], in) * 0xFFFF;
			gamma->green[i] =
				cmsEvalToneCurveFloat (vcgt[1], in) * 0xFFFF;
			gamma->blue[i]  =
				cmsEvalToneCurveFloat (vcgt[2], in) * 0xFFFF;
		}
	} else {
		g_debug ("ICC profile has no VCGT");
		reset_gamma (gamma);
	}

	cmsCloseProfile (prof);
}


static inline void
colord2lcms (cmsCIExyY *dest, const CdColorYxy *src)
{
	dest->x = src->x;
	dest->y = src->y;
	dest->Y = src->Y;
}

GBytes *
icc_from_edid (const struct edid *edid)
{
	GBytes *retval = NULL;
	cmsHPROFILE prof = NULL;
	cmsToneCurve *curve[3] = { NULL, NULL, NULL };
	cmsHANDLE dict = NULL;
	cmsUInt32Number size;
	cmsCIExyYTRIPLE chroma;
	cmsCIExyY white;
	gpointer data;
	gboolean ret;

	if (! edid)
		return NULL;

	colord2lcms (&chroma.Red, &edid->red);
	colord2lcms (&chroma.Green, &edid->green);
	colord2lcms (&chroma.Blue, &edid->blue);
	colord2lcms (&white, &edid->white);

	curve[0] = curve[1] = curve[2] = cmsBuildGamma (NULL, edid->gamma);
	prof = cmsCreateRGBProfile (&white, &chroma, curve);
	if (! prof) {
		g_critical ("unable to create profile from EDID");
		goto out;
	}

	cmsSetColorSpace (prof, cmsSigRgbData);
	cmsSetPCS (prof, cmsSigXYZData);
	cmsSetHeaderRenderingIntent (prof, INTENT_PERCEPTUAL);
	cmsSetDeviceClass (prof, cmsSigDisplayClass);

	ret = write_cms_tag (prof, cmsSigCopyrightTag,
			     /* deliberately not translated */
			     "This profile is free of known copyright restrictions.");
	if (! ret) {
		g_critical ("failed to write copyright");
		goto out;
	}

	write_cms_tag (prof, cmsSigDeviceModelDescTag, edid->model);
	write_cms_tag (prof, cmsSigProfileDescriptionTag, edid->model);
	write_cms_tag (prof, cmsSigDeviceMfgDescTag, edid->vendor);

	dict = cmsDictAlloc (NULL);

	add_cms_dict_entry (dict, CD_PROFILE_METADATA_CMF_PRODUCT, PACKAGE);
	add_cms_dict_entry (dict, CD_PROFILE_METADATA_CMF_BINARY, g_get_prgname ());
	add_cms_dict_entry (dict, CD_PROFILE_METADATA_CMF_VERSION, VERSION);

	add_cms_dict_entry (dict, CD_PROFILE_METADATA_DATA_SOURCE,
			    CD_PROFILE_METADATA_DATA_SOURCE_EDID);

	add_cms_dict_entry (dict, CD_PROFILE_METADATA_EDID_MD5, edid->cksum);
	add_cms_dict_entry (dict, CD_PROFILE_METADATA_EDID_MODEL, edid->model);
	add_cms_dict_entry (dict, CD_PROFILE_METADATA_EDID_SERIAL, edid->serial);
	add_cms_dict_entry (dict, CD_PROFILE_METADATA_EDID_MNFT, edid->pnpid);
	add_cms_dict_entry (dict, CD_PROFILE_METADATA_EDID_VENDOR, edid->vendor);

	ret = cmsWriteTag(prof, cmsSigMetaTag, dict);
	if (! ret) {
		g_critical ("unable to write profile metadata");
		goto out;
	}

	ret = cmsMD5computeID (prof);
	if (! ret) {
		g_critical ("unable to compute profile MD5");
		goto out;
	}

	ret = cmsSaveProfileToMem (prof, NULL, &size);
	if (! ret) {
		g_critical ("unable to compute profile size");
		goto out;
	}

	data = g_malloc (size);
	ret = cmsSaveProfileToMem (prof, data, &size);
	if (! ret) {
		g_critical ("unable to make profile in memory");
		goto out;
	}

	retval = g_bytes_new_take (data, size);

out:
	if (prof)
		cmsCloseProfile (prof);
	if (curve[0])
		cmsFreeToneCurve (curve[0]);
	if (dict)
		cmsDictFree (dict);

	return retval;
}

static gchar *
profile_id (GBytes *icc)
{
	cmsUInt8Number profid[16];
	cmsHPROFILE prof = NULL;
	gchar *retval = NULL;
	guint i;

	prof = cmsOpenProfileFromMem (g_bytes_get_data (icc, NULL), g_bytes_get_size (icc));
	if (! prof) {
		g_critical ("corrupt ICC profile");
		return NULL;
	}

	cmsGetHeaderProfileID (prof, profid);
	for (i = 0; i < 16; ++i) {
		if (profid[i] == 0)
			goto out;
	}

	retval = g_new0 (gchar, 32+1);
	for (i = 0; i < 16; ++i) {
		g_snprintf (retval + (2 * i), 3, "%02x", profid[i]);
	}

out:
	cmsCloseProfile (prof);
	return retval;
}

gchar *
icc_identify (GBytes *icc)
{
	gchar *retval = NULL;

	retval = profile_id (icc);
	if (! retval)
		retval = g_compute_checksum_for_data (G_CHECKSUM_MD5,
						      g_bytes_get_data (icc, NULL),
						      g_bytes_get_size (icc));

	return retval;
}


/* vim: set ts=8 sw=8 tw=0 : */

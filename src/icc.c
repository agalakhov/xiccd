#include "icc.h"
#include <colord.h>
#include <glib.h>
#include <stdlib.h>
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
icc_to_gamma (XRRCrtcGamma *gamma, CdIcc *icc)
{
	GPtrArray *vcgt = NULL;
	guint i;

	if (! icc) {
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
}


CdIcc *
icc_from_edid (CdEdid *edid)
{
	CdIcc *icc;
	GError *err = NULL;
	gboolean ret;

	if (! edid)
		return NULL;

	icc = cd_icc_new ();
	ret = cd_icc_create_from_edid_data (icc, edid, &err);
	if (! ret) {
		g_critical ("unable to create profile from EDID: %s", err->message);
		g_error_free (err);
		return NULL;
	}

	cd_icc_set_kind (icc, CD_PROFILE_KIND_DISPLAY_DEVICE);

	cd_icc_add_metadata (icc, CD_PROFILE_METADATA_CMF_PRODUCT, PACKAGE);
	cd_icc_add_metadata (icc, CD_PROFILE_METADATA_CMF_BINARY, g_get_prgname ());
	cd_icc_add_metadata (icc, CD_PROFILE_METADATA_CMF_VERSION, VERSION);

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

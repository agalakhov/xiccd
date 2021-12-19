#ifndef __ICC_H__
#define __ICC_H__

#include <colord.h>
#include <glib.h>
#include <X11/extensions/Xrandr.h>

struct edid;

void icc_to_gamma (XRRCrtcGamma *gamma, CdIcc *icc);
CdIcc *icc_from_edid (const struct edid *edid);
gchar *icc_identify (GFile *file);

#endif /* __ICC_H__ */

/* vim: set ts=8 sw=8 tw=0 : */

bin_PROGRAMS = xiccd

xiccd_SOURCES = xiccd.c \
    dmi.h dmi.c \
    edid.h edid.c \
    icc.h icc.c \
    icc-storage.h icc-storage.c \
    icc-storage-private.h icc-storage-private.c \
    randr-conn.h randr-conn.c \
    randr-conn-private.h randr-conn-private.c

AM_CFLAGS = -Wall -Wextra -pedantic \
    $(GLIB_CFLAGS) $(X11_CFLAGS) $(XRANDR_CFLAGS) $(LCMS_CFLAGS) $(COLORD_CFLAGS)
xiccd_LDADD = $(GLIB_LIBS) $(X11_LIBS) $(XRANDR_LIBS) $(LCMS_LIBS) $(COLORD_LIBS)

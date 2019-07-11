bin_PROGRAMS = xiccd

xiccd_SOURCES = xiccd.c \
    edid.h edid.c \
    icc.h icc.c \
    randr-conn.h randr-conn.c \
    randr-conn-private.h randr-conn-private.c

AM_CFLAGS = -Wall -Wextra -pedantic \
    $(GLIB_CFLAGS) $(X11_CFLAGS) $(XRANDR_CFLAGS) $(COLORD_CFLAGS)
xiccd_LDADD = $(GLIB_LIBS) $(X11_LIBS) $(XRANDR_LIBS) $(COLORD_LIBS)

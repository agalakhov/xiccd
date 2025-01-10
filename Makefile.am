AUTOMAKE_OPTIONS = subdir-objects

bin_PROGRAMS = xiccd

xiccd_SOURCES = \
    src/xiccd.c \
    src/icc.h src/icc.c \
    src/randr-conn.h src/randr-conn.c \
    src/randr-conn-private.h src/randr-conn-private.c

AM_CFLAGS = -Wall -Wextra -pedantic \
    $(GLIB_CFLAGS) $(X11_CFLAGS) $(XRANDR_CFLAGS) $(COLORD_CFLAGS)
xiccd_LDADD = $(GLIB_LIBS) $(X11_LIBS) $(XRANDR_LIBS) $(COLORD_LIBS)

dist_man_MANS = doc/xiccd.8
dist_doc_DATA = README.md

autostartdir = $(sysconfigdir)/xdg/autostart
autostart_DATA = etc/xdg/xiccd.desktop

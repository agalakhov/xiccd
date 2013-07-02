AC_PREREQ([2.69])
AC_INIT([xiccd], [0.1.0])

AC_CONFIG_SRCDIR([src/xiccd.c])

AM_INIT_AUTOMAKE([1.9 no-dist-gzip dist-xz tar-ustar])
AC_LANG([C])
AC_PROG_CC

PKG_CHECK_MODULES(X11, x11)
PKG_CHECK_MODULES(XRANDR, xrandr >= 1.3)
PKG_CHECK_MODULES(GLIB, glib-2.0 >= 2.36)
PKG_CHECK_MODULES(COLORD, colord >= 0.1.31)
PKG_CHECK_MODULES(LCMS, lcms2)

AC_CONFIG_FILES([Makefile
                 src/Makefile
                ])
AC_OUTPUT

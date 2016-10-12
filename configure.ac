# Copyright (C) 2010-2011 Richard Hughes <richard@hughsie.com>
AC_PREREQ(2.63)

AC_INIT([gnome-color-manager],[3.22.2],[http://bugzilla.gnome.org/enter_bug.cgi?product=gnome-color-manager])
AC_CONFIG_SRCDIR(src)
AM_INIT_AUTOMAKE([1.11 no-dist-gzip dist-xz tar-ustar foreign])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

# enable nice build output on automake1.11
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

AC_PROG_CC
AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_RANLIB
LT_INIT
AM_PROG_CC_C_O
IT_PROG_INTLTOOL([0.35.0])
YELP_HELP_INIT

# validate AppData
APPSTREAM_XML

WARN_CFLAGS_EXTRA="
	-Waggregate-return
	-Warray-bounds
	-Wcast-align
	-Wclobbered
	-Wdeclaration-after-statement
	-Wempty-body
	-Wextra
	-Wformat=2
	-Wformat-nonliteral
	-Wformat-security
	-Wformat-signedness
	-Wignored-qualifiers
	-Wimplicit-function-declaration
	-Winit-self
	-Winline
	-Wmissing-declarations
	-Wmissing-format-attribute
	-Wmissing-include-dirs
	-Wmissing-noreturn
	-Wmissing-parameter-type
	-Wmissing-prototypes
	-Wnested-externs
	-Wno-discarded-qualifiers
	-Wno-missing-field-initializers
	-Wno-strict-aliasing
	-Wno-suggest-attribute=format
	-Wno-unused-parameter
	-Wold-style-definition
	-Woverride-init
	-Wpacked
	-Wpointer-arith
	-Wredundant-decls
	-Wreturn-type
	-Wshadow
	-Wsign-compare
	-Wstrict-aliasing
	-Wstrict-prototypes
	-Wswitch-default
	-Wtype-limits
	-Wundef
	-Wuninitialized
	-Wunused-but-set-variable
	-Wwrite-strings"
AX_APPEND_COMPILE_FLAGS([$WARN_CFLAGS_EXTRA], [WARN_CFLAGS])
AC_SUBST(WARN_CFLAGS)

dnl ---------------------------------------------------------------------------
dnl - gettext stuff
dnl ---------------------------------------------------------------------------
GETTEXT_PACKAGE=AC_PACKAGE_NAME
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE(GETTEXT_PACKAGE, "AC_PACKAGE_NAME", [foo])

AM_GLIB_GNU_GETTEXT
GLIB_GSETTINGS

dnl ---------------------------------------------------------------------------
dnl - Check library dependencies
dnl ---------------------------------------------------------------------------
PKG_CHECK_MODULES(GLIB, glib-2.0 >= 2.31.10 gobject-2.0 gthread-2.0 gio-2.0 >= 2.25.9)
PKG_CHECK_MODULES(GTK, gtk+-3.0 >= 2.91.0)
PKG_CHECK_MODULES(LCMS, lcms2 >= 2.2)
PKG_CHECK_MODULES(COLORD, colord >= 1.3.1 colord-gtk >= 0.1.20)
AC_PATH_PROG(APPSTREAM_UTIL, [appstream-util], [unfound])

dnl **** Check for VTE ****
PKG_CHECK_MODULES(VTE, vte-2.91 >= 0.25.1, has_vte=yes, has_vte=no)
AM_CONDITIONAL(HAVE_VTE, test x$has_vte = xyes)
if test x$has_vte = xyes; then
	AC_DEFINE(HAVE_VTE,1,[Use VTE terminal widget])
fi

AC_ARG_ENABLE(exiv, AS_HELP_STRING([--enable-exiv],[Enable EXIV support for RAW support]), enable_exiv=$enableval,
				   enable_exiv=yes)
dnl **** Check for EXIV ****
if test x$enable_exiv != xno; then
	PKG_CHECK_MODULES(EXIV, exiv2)
fi
AM_CONDITIONAL(HAVE_EXIV, test x$enable_exiv = xyes)
if test x$enable_exiv = xyes; then
	AC_DEFINE(HAVE_EXIV,1,[Use EXIV support for detecting scanners])
fi

PKG_CHECK_MODULES(CANBERRA, libcanberra-gtk3 >= 0.10)

PKG_CHECK_MODULES(EXIF, libexif)
AC_CHECK_LIB(tiff, TIFFReadRGBAImageOriented,
	     TIFF_CFLAGS=""
	     TIFF_LIBS="-ltiff"
	     HAVE_TIFF="yes")
if test "x$HAVE_TIFF" != "xyes"; then
	AC_MSG_ERROR([libtiff-devel is required for gnome-color-manager])
fi
AC_SUBST(TIFF_CFLAGS)
AC_SUBST(TIFF_LIBS)

dnl ---------------------------------------------------------------------------
dnl - Generate man pages ? (default enabled)
dnl ---------------------------------------------------------------------------
AC_ARG_ENABLE(man_pages, AS_HELP_STRING([--disable-man-pages],[Disable man pages generation]), enable_man_pages=$enableval)
if test x$enable_man_pages != xno; then
	AC_PATH_PROG(XSLTPROC, xsltproc, no)
	if test x$enable_man_pages = xyes; then
		if test "$XSLTPROC" = "no" ; then
			AC_MSG_ERROR([xsltproc not found])
		fi
	fi
else
	XSLTPROC=no
fi
AM_CONDITIONAL(HAVE_XSLTPROC, [test "$XSLTPROC" != "no"])

dnl ---------------------------------------------------------------------------
dnl - Make paths available for source files
dnl ---------------------------------------------------------------------------
AC_SUBST(DATADIR, $datadir)

dnl ---------------------------------------------------------------------------
dnl - Where should we put documentation ?
dnl ---------------------------------------------------------------------------
AC_ARG_WITH(doc-dir,
            AS_HELP_STRING([--with-doc-dir=<dir>],
                           [directory to install documentation]))

if ! test -z "$with_doc_dir"; then
   DOCDIR="$with_doc_dir/gnome-color-manager-$VERSION"
else
   DOCDIR="$DATADIR/doc/gnome-color-manager-$VERSION"
fi
AC_SUBST(DOCDIR)

dnl ---------------------------------------------------------------------------
dnl - Is docbook2man available?
dnl ---------------------------------------------------------------------------
AC_PATH_PROG(DOCBOOK2MAN, docbook2man, no)
if test "$DOCBOOK2MAN" = "no" ; then
	AC_MSG_WARN([docbook2man not found, will not be able to build man documentation])
fi
AM_CONDITIONAL(HAVE_DOCBOOK2MAN, [test "$DOCBOOK2MAN" != "no"])

dnl ---------------------------------------------------------------------------
dnl - Use PackageKit to install missing tools
dnl ---------------------------------------------------------------------------
AC_ARG_ENABLE(packagekit, AS_HELP_STRING([--enable-packagekit],[enable PackageKit integration]),
	      enable_packagekit=$enableval,enable_packagekit=yes)
AM_CONDITIONAL(HAVE_PACKAGEKIT, test x$enable_packagekit = xyes)
if test x$enable_packagekit = xyes; then
	AC_DEFINE(HAVE_PACKAGEKIT,1,[Use PackageKit for installing packages])
fi

dnl ---------------------------------------------------------------------------
dnl - Build self tests
dnl ---------------------------------------------------------------------------
AC_ARG_ENABLE(tests, AS_HELP_STRING([--enable-tests],[enable unit test code]),
	      enable_tests=$enableval,enable_tests=yes)
AM_CONDITIONAL(HAVE_TESTS, test x$enable_tests = xyes)

AC_SUBST(GCM_SYSTEM_PROFILES_DIR, "\$(localstatedir)/lib/color")

dnl ---------------------------------------------------------------------------
dnl - Makefiles, etc.
dnl ---------------------------------------------------------------------------
AC_CONFIG_FILES([
Makefile
data/Makefile
data/appdata/Makefile
data/icons/Makefile
data/icons/16x16/Makefile
data/icons/22x22/Makefile
data/icons/24x24/Makefile
data/icons/32x32/Makefile
data/icons/48x48/Makefile
data/icons/64x64/Makefile
data/icons/256x256/Makefile
data/icons/scalable/Makefile
data/tests/Makefile
data/targets/Makefile
data/figures/Makefile
data/ti1/Makefile
help/Makefile
man/Makefile
po/Makefile.in
src/Makefile
])
AC_OUTPUT

dnl ==========================================================================
echo "
                    gnome-color-manager $VERSION
                  ================================

        prefix:                    ${prefix}
        datadir:                   ${datadir}
        compiler:                  ${CC}
        cflags:                    ${CFLAGS}
        cppflags:                  ${CPPFLAGS}
        PackageKit integration:    ${enable_packagekit}
        RAW support:               ${enable_exiv}
        VTE support:               ${has_vte}
        building unit tests:       ${enable_tests}
"


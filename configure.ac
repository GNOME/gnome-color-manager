# Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
AC_PREREQ(2.63)

m4_define([gcm_major_version], [2])
m4_define([gcm_minor_version], [91])
m4_define([gcm_micro_version], [3])
m4_define([gcm_version],
          [gcm_major_version.gcm_minor_version.gcm_micro_version])

AC_INIT([gnome-color-manager],[gcm_version],[http://bugzilla.gnome.org/enter_bug.cgi?product=gnome-color-manager])
AC_CONFIG_SRCDIR(src)
AM_INIT_AUTOMAKE([1.9 tar-ustar dist-bzip2])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

GOBJECT_INTROSPECTION_CHECK([0.6.7])

# enable nice build output on automake1.11
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

GCM_MAJOR_VERSION=gcm_major_version
GCM_MINOR_VERSION=gcm_minor_version
GCM_MICRO_VERSION=gcm_micro_version
GCM_VERSION=gcm_version
AC_SUBST(GCM_MAJOR_VERSION)
AC_SUBST(GCM_MINOR_VERSION)
AC_SUBST(GCM_MICRO_VERSION)
AC_SUBST(GCM_VERSION)

# libtool versioning - this applies to libcolor-glib
#
# See http://sources.redhat.com/autobook/autobook/autobook_91.html#SEC91 for details
#
# increment;
# CURRENT	If the API or ABI interface has changed (reset REVISION to 0)
# REVISION	If the API and ABI remains the same, but bugs are fixed.
# AGE		If libgcm can be linked into executables which can be
# 		built with previous versions of this library. Don't use.
LT_CURRENT=1
LT_REVISION=0
LT_AGE=0
AC_SUBST(LT_CURRENT)
AC_SUBST(LT_REVISION)
AC_SUBST(LT_AGE)

AC_PROG_CC
AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_RANLIB
LT_INIT
AM_PROG_CC_C_O
IT_PROG_INTLTOOL([0.35.0])

GNOME_COMPILE_WARNINGS(error)
GNOME_DOC_INIT

# set up gtk-doc
GTK_DOC_CHECK(1.9)

dnl ---------------------------------------------------------------------------
dnl - Use strict options (default enabled for devs, disabled in releases)
dnl ---------------------------------------------------------------------------
if test -d ".git"; then
	default_strict=yes
else
	default_strict=no
fi

AC_ARG_ENABLE(strict, AS_HELP_STRING([--enable-strict],[Enable strict compilation options]), enable_strict=$enableval,
					enable_strict=$default_strict)
if test x$enable_strict != xno; then
	CPPFLAGS="$CPPFLAGS -DG_DISABLE_SINGLE_INCLUDES -DGTK_DISABLE_SINGLE_INCLUDES"
	CPPFLAGS="$CPPFLAGS -DG_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED -DGDK_DISABLE_DEPRECATED"
	CPPFLAGS="$CPPFLAGS -DGSEAL_ENABLE"
	if test "$GCC" = "yes"; then
		CPPFLAGS="$CPPFLAGS -Werror"
	fi
fi

# udev rules go in /lib, not /usr/lib
if test "$prefix" = "/usr" -o "$prefix" = "/usr/local" ; then
  slashlibdir=/lib
else
  slashlibdir=$prefix/lib
fi
AC_SUBST(slashlibdir)

dnl ---------------------------------------------------------------------------
dnl - Extra verbose warning switches
dnl ---------------------------------------------------------------------------

if test "$GCC" = "yes"; then
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wall"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wcast-align -Wno-uninitialized"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wmissing-declarations"
#	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wredundant-decls"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wpointer-arith"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wcast-align"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wwrite-strings"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Winit-self"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wreturn-type"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wformat-nonliteral"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wformat-security"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wmissing-include-dirs"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wmissing-format-attribute"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wclobbered"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wempty-body"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wignored-qualifiers"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wsign-compare"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wtype-limits"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wuninitialized"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Waggregate-return"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wdeclaration-after-statement"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wshadow"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wno-strict-aliasing"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Winline"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wmissing-parameter-type"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Woverride-init"
else
	WARNINGFLAGS_C=""
fi
AC_SUBST(WARNINGFLAGS_C)

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
PKG_CHECK_MODULES(GLIB, glib-2.0 >= 2.14.0 gobject-2.0 gthread-2.0 gio-2.0 >= 2.25.9)
PKG_CHECK_MODULES(XORG, xrandr)
PKG_CHECK_MODULES(GTK, gtk+-3.0 >= 2.91.0)
PKG_CHECK_MODULES(GUDEV, gudev-1.0)
PKG_CHECK_MODULES(LCMS, lcms2)
PKG_CHECK_MODULES(X11, x11)
PKG_CHECK_MODULES(SETTINGS_DAEMON, gnome-settings-daemon)
PKG_CHECK_MODULES(USB, libusb-1.0 >= 1.0.9, has_new_libusb=yes, has_new_libusb=no)
if test x$has_new_libusb = xyes; then
	AC_DEFINE(HAVE_NEW_USB,1,[Got new libusb])
else
	PKG_CHECK_MODULES(USB, libusb-1.0 >= 1.0.0)
fi

dnl Required for the properties window
PKG_CHECK_MODULES(CONTROL_CENTER, [
 libgnome-control-center >= 2.31.4])
PANELS_DIR="${libdir}/control-center-1/panels"
AC_SUBST(PANELS_DIR)

dnl **** Check for VTE ****
PKG_CHECK_MODULES(VTE, vte-2.90 >= 0.25.1, has_vte=yes, has_vte=no)
AM_CONDITIONAL(HAVE_VTE, test x$has_vte = xyes)
if test x$has_vte = xyes; then
	AC_DEFINE(HAVE_VTE,1,[Use VTE terminal widget])
fi

dnl **** Check for libnotify ****
PKG_CHECK_MODULES(NOTIFY, libnotify >= 0.7.0)

dnl **** Check for SANE ****
AC_ARG_ENABLE(sane, AS_HELP_STRING([--enable-sane],[Enable SANE support for scanner support]), enable_sane=$enableval,
				   enable_sane=yes)
if test x$enable_sane != xno; then
	PKG_CHECK_MODULES(SANE, sane-backends, has_sane=yes, has_sane=no)
	if test $has_sane = "no"; then
		AC_CHECK_HEADER(sane/sane.h, has_sane=yes, has_sane=no)
		if test $has_sane = "no"; then
			AC_MSG_ERROR([Cannot fine SANE])
		fi
		SANE_CFLAGS=""
		SANE_LIBS="-lsane"
		AC_SUBST(SANE_CFLAGS)
		AC_SUBST(SANE_LIBS)
	fi
fi
AM_CONDITIONAL(HAVE_SANE, test x$enable_sane = xyes)
if test x$enable_sane = xyes; then
	AC_DEFINE(HAVE_SANE,1,[Use SANE support for detecting scanners])
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

dnl **** Check for cups ****
AC_SUBST(CUPS_CFLAGS,"")
AC_SUBST(CUPS_LIBS,"")
ac_save_CPPFLAGS="$CPPFLAGS"
ac_cups_cflags=`cups-config --cflags 2>/dev/null`
ac_cups_libs=`cups-config --ldflags 2>/dev/null`
CPPFLAGS="$CPPFLAGS $ac_cups_cflags"
AC_CHECK_HEADERS(cups/cups.h,
    [AC_CHECK_LIB(cups,cupsGetDefault,
		  [
			CUPS_CFLAGS="$ac_cups_cflags"
			CUPS_LIBS="-lcups $ac_cups_libs"
			HAVE_CUPS="yes"
		  ],,[$ac_cups_libs])])
CPPFLAGS="$ac_save_CPPFLAGS"
if test "x$HAVE_CUPS" != "xyes"; then
	AC_MSG_ERROR([cups-devel is required for gnome-color-manager])
fi

dnl ---------------------------------------------------------------------------
dnl - Generate man pages ? (default enabled)
dnl ---------------------------------------------------------------------------
AC_ARG_ENABLE(man_pages, AS_HELP_STRING([--disable-man-pages],[Disable man pages generation]), enable_man_pages=$enableval)
if test x$enable_man_pages != xno; then
	AC_PATH_PROG(XSLTPROC, xsltproc, no)
	if test x$enable_man_pages = xyes; then
		if test "$XSLTPROC" = "no" ; then
			AC_MSG_ERROR([xsltproc not found, it's needod to build man pages])
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
help/Makefile
man/Makefile
policy/Makefile
po/Makefile.in
rules/Makefile
src/Makefile
session/Makefile
tools/Makefile
docs/Makefile
docs/huey/Makefile
docs/api/Makefile
docs/api/version.xml
libcolor-glib/Makefile
libcolor-glib/gcm-version.h
libcolor-glib/libcolor-glib.pc
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
        SANE support:              ${enable_sane}
        RAW support:               ${enable_exiv}
        VTE support:               ${has_vte}
        New LIBUSB:                ${has_new_libusb}
        building unit tests:       ${enable_tests}
"


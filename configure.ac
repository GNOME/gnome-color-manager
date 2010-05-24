AC_PREREQ(2.63)

AC_INIT([gnome-color-manager],[2.31.2],[http://bugzilla.gnome.org/enter_bug.cgi?product=gnome-color-manager])
AC_CONFIG_SRCDIR(src)
AM_INIT_AUTOMAKE([1.9 tar-ustar dist-bzip2])
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

GNOME_COMPILE_WARNINGS
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
CPPFLAGS="$CPPFLAGS -DG_DISABLE_SINGLE_INCLUDES -DGTK_DISABLE_SINGLE_INCLUDES"
CPPFLAGS="$CPPFLAGS -DG_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED -DGDK_DISABLE_DEPRECATED"
CPPFLAGS="$CPPFLAGS -DGSEAL_ENABLE"

if test "$GCC" = "yes"; then
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wall"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Werror"
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
dnl - Library dependencies
dnl ---------------------------------------------------------------------------
GLIB_REQUIRED=2.14.0
GTK_REQUIRED=2.14.0
GNOMEDESKTOP_REQUIRED=2.14.0
UNIQUE_REQUIRED=1.0.0
VTE_REQUIRED=0.25.1
DBUS_GLIB_REQUIRED=0.73
CANBERRA_REQUIRED=0.10
GIO_REQUIRED=2.25.0

dnl ---------------------------------------------------------------------------
dnl - Check library dependencies
dnl ---------------------------------------------------------------------------
PKG_CHECK_MODULES(GLIB, glib-2.0 >= $GLIB_REQUIRED gobject-2.0 gthread-2.0 gio-2.0 >= $GIO_REQUIRED)
PKG_CHECK_MODULES(XORG, xxf86vm xrandr)
PKG_CHECK_MODULES(GTK, gtk+-2.0 >= $GTK_REQUIRED)
PKG_CHECK_MODULES(GNOMEDESKTOP, gnome-desktop-2.0 >= $GNOMEDESKTOP_REQUIRED)
PKG_CHECK_MODULES(UNIQUE, unique-1.0 >= $UNIQUE_REQUIRED)
PKG_CHECK_MODULES(VTE, vte >= $VTE_REQUIRED)
PKG_CHECK_MODULES(GUDEV, gudev-1.0)
PKG_CHECK_MODULES(LCMS, lcms)
PKG_CHECK_MODULES(DBUS_GLIB, dbus-glib-1 >= $DBUS_GLIB_REQUIRED)
PKG_CHECK_MODULES(X11, x11)
PKG_CHECK_MODULES(NOTIFY, libnotify)

AC_ARG_ENABLE(sane, AS_HELP_STRING([--enable-sane],[Enable SANE support for scanner support]), enable_sane=$enableval,
				   enable_sane=yes)
dnl **** Check for SANE ****
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
AM_CONDITIONAL(GCM_USE_SANE, test x$enable_sane = xyes)
if test x$enable_sane = xyes; then
	AC_DEFINE(GCM_USE_SANE,1,[Use SANE support for detecting scanners])
fi

AC_ARG_ENABLE(exiv, AS_HELP_STRING([--enable-exiv],[Enable EXIV support for RAW support]), enable_exiv=$enableval,
				   enable_exiv=yes)
dnl **** Check for EXIV ****
if test x$enable_exiv != xno; then
	PKG_CHECK_MODULES(EXIV, exiv2)
fi
AM_CONDITIONAL(GCM_USE_EXIV, test x$enable_exiv = xyes)
if test x$enable_exiv = xyes; then
	AC_DEFINE(GCM_USE_EXIV,1,[Use EXIV support for detecting scanners])
fi

PKG_CHECK_MODULES(CANBERRA, libcanberra-gtk >= $CANBERRA_REQUIRED)

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

#AC_SUBST(gsettingsupdateschemacache,
#	`pkg-config --variable gsettingsupdateschemacache gio-2.0`)

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
AM_CONDITIONAL(GCM_USE_PACKAGEKIT, test x$enable_packagekit = xyes)
if test x$enable_packagekit = xyes; then
	AC_DEFINE(GCM_USE_PACKAGEKIT,1,[Use PackageKit for installing packages])
fi

dnl ---------------------------------------------------------------------------
dnl - Build self tests
dnl ---------------------------------------------------------------------------
AC_ARG_ENABLE(tests, AS_HELP_STRING([--enable-tests],[enable unit test code]),
	      enable_tests=$enableval,enable_tests=yes)
AM_CONDITIONAL(EGG_BUILD_TESTS, test x$enable_tests = xyes)
if test x$enable_tests = xyes; then
	AC_DEFINE(EGG_BUILD_TESTS,1,[Build test code])
fi

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
        building unit tests:       ${enable_tests}
"


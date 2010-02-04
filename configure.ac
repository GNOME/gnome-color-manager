AC_PREREQ(2.65)

AC_INIT([gnome-color-manager],[2.29.4],[http://bugzilla.gnome.org/enter_bug.cgi?product=gnome-color-manager])
AC_CONFIG_SRCDIR(src)
AM_INIT_AUTOMAKE(AC_PACKAGE_NAME, AC_PACKAGE_VERSION)
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

# enable nice build output on automake1.11
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

AC_PROG_CC
AC_PROG_INSTALL
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
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wredundant-decls"
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

dnl ---------------------------------------------------------------------------
dnl - Library dependencies
dnl ---------------------------------------------------------------------------
GLIB_REQUIRED=2.14.0
GTK_REQUIRED=2.14.0
GNOMEDESKTOP_REQUIRED=2.14.0
GCONF_REQUIRED=0.22
UNIQUE_REQUIRED=1.0.0
VTE_REQUIRED=0.22.2
DBUS_GLIB_REQUIRED=0.73

dnl ---------------------------------------------------------------------------
dnl - Check library dependencies
dnl ---------------------------------------------------------------------------
PKG_CHECK_MODULES(GLIB, glib-2.0 >= $GLIB_REQUIRED gobject-2.0 gthread-2.0 gio-2.0)
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

PKG_CHECK_MODULES(XORG, xxf86vm xrandr)
AC_SUBST(XORG_CFLAGS)
AC_SUBST(XORG_LIBS)

PKG_CHECK_MODULES(GTK, gtk+-2.0 >= $GTK_REQUIRED)
AC_SUBST(GTK_CFLAGS)
AC_SUBST(GTK_LIBS)

PKG_CHECK_MODULES(GNOMEDESKTOP, gnome-desktop-2.0 >= $GNOMEDESKTOP_REQUIRED)
AC_SUBST(GNOMEDESKTOP_CFLAGS)
AC_SUBST(GNOMEDESKTOP_LIBS)

PKG_CHECK_MODULES(GCONF, \
 gconf-2.0 >= $GCONF_REQUIRED)
AC_SUBST(GCONF_CFLAGS)
AC_SUBST(GCONF_LIBS)

PKG_CHECK_MODULES(UNIQUE, unique-1.0 >= $UNIQUE_REQUIRED)
AC_SUBST(UNIQUE_CFLAGS)
AC_SUBST(UNIQUE_LIBS)

PKG_CHECK_MODULES(VTE, vte >= $VTE_REQUIRED)
AC_SUBST(VTE_CFLAGS)
AC_SUBST(VTE_LIBS)

PKG_CHECK_MODULES(GUDEV, gudev-1.0)
AC_SUBST(GUDEV_CFLAGS)
AC_SUBST(GUDEV_LIBS)

PKG_CHECK_MODULES(LCMS, lcms)
AC_SUBST(LCMS_CFLAGS)
AC_SUBST(LCMS_LIBS)
AC_DEFINE(GCM_HAVE_LCMS,1,[we have LCMS support])

PKG_CHECK_MODULES(DBUS_GLIB, dbus-glib-1 >= $DBUS_GLIB_REQUIRED)
AC_SUBST(DBUS_GLIB_CFLAGS)
AC_SUBST(DBUS_GLIB_LIBS)

PKG_CHECK_MODULES(X11, x11)
AC_SUBST(X11_CFLAGS)
AC_SUBST(X11_LIBS)

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
		  ],,[$ac_cups_libs])])
CPPFLAGS="$ac_save_CPPFLAGS"

AC_PATH_PROG(GCONFTOOL, gconftool-2)
AM_GCONF_SOURCE_2

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
dnl - Autodetect the hardware device
dnl - This requires 55-Argyll.rules to set COLOR_MEASUREMENT_DEVICE
dnl ---------------------------------------------------------------------------
AC_ARG_ENABLE(hardware_detection, AS_HELP_STRING([--enable-hardware-detection],[enable hardware auto-detection (requires fixed argyllcms)]),
	      enable_hardware_detection=$enableval,enable_hardware_detection=no)
AM_CONDITIONAL(GCM_HARDWARE_DETECTION, test x$enable_hardware_detection = xyes)
if test x$enable_hardware_detection = xyes; then
	AC_DEFINE(GCM_HARDWARE_DETECTION,1,[Build hardware autodetection])
fi

dnl ---------------------------------------------------------------------------
dnl - Use PackageKit to install missing tools
dnl ---------------------------------------------------------------------------
AC_ARG_ENABLE(packagekit, AS_HELP_STRING([--enable-packagekit],[enable PackageKit integration]),
	      enable_packagekit=$enableval,enable_packagekit=yes)
AM_CONDITIONAL(GCM_USE_PACKAGEKIT, test x$enable_packagekit = xyes)
if test x$enable_packagekit = xyes; then
	AC_DEFINE(GCM_USE_PACKAGEKIT,1,[Build hardware autodetection])
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
data/icons/scalable/Makefile
data/tests/Makefile
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
        hardware auto-detection:   ${enable_hardware_detection}
        PackageKit integration:    ${enable_packagekit}
        building unit tests:       ${enable_tests}
        gconf-schema dir:          $GCONF_SCHEMA_FILE_DIR
"


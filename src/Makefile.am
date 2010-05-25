INCLUDES =						\
	$(GLIB_CFLAGS)					\
	$(X11_CFLAGS)					\
	$(GTK_CFLAGS)					\
	$(GNOMEDESKTOP_CFLAGS)				\
	$(UNIQUE_CFLAGS)				\
	$(VTE_CFLAGS)					\
	$(LCMS_CFLAGS)					\
	$(XORG_CFLAGS)					\
	$(CUPS_CFLAGS)					\
	$(SANE_CFLAGS)					\
	$(TIFF_CFLAGS)					\
	$(EXIF_CFLAGS)					\
	$(EXIV_CFLAGS)					\
	$(NOTIFY_CFLAGS)				\
	$(CANBERRA_CFLAGS)				\
	-DG_UDEV_API_IS_SUBJECT_TO_CHANGE		\
	-DGNOME_DESKTOP_USE_UNSTABLE_API		\
	$(GUDEV_CFLAGS)					\
	-DBINDIR=\"$(bindir)\"				\
	-DLIBEXECDIR=\"$(libexecdir)\"			\
	-DSBINDIR=\"$(sbindir)\"			\
	-DDATADIR=\"$(datadir)\"			\
	-DSYSCONFDIR=\""$(sysconfdir)"\"		\
	-DVERSION="\"$(VERSION)\""			\
	-DLOCALEDIR=\""$(localedir)"\"			\
	-DGCM_SYSTEM_PROFILES_DIR="\"$(GCM_SYSTEM_PROFILES_DIR)"\" \
	-DGCM_DATA=\"$(pkgdatadir)\"

introspectiondir = $(datadir)/dbus-1/interfaces
dist_introspection_DATA = org.gnome.ColorManager.xml

noinst_LIBRARIES = libgcmshared.a
libgcmshared_a_SOURCES =				\
	egg-debug.c					\
	egg-debug.h					\
	gcm-image.c					\
	gcm-image.h					\
	gcm-screen.c					\
	gcm-screen.h					\
	gcm-xyz.c					\
	gcm-xyz.h					\
	gcm-exif.c					\
	gcm-exif.h					\
	gcm-print.c					\
	gcm-print.h					\
	gcm-utils.c					\
	gcm-utils.h					\
	gcm-enum.c					\
	gcm-enum.h					\
	gcm-clut.c					\
	gcm-clut.h					\
	gcm-edid.c					\
	gcm-edid.h					\
	gcm-dmi.c					\
	gcm-dmi.h					\
	gcm-tables.c					\
	gcm-tables.h					\
	gcm-xserver.c					\
	gcm-xserver.h					\
	gcm-client.c					\
	gcm-client.h					\
	gcm-colorimeter.c				\
	gcm-colorimeter.h				\
	gcm-device.c					\
	gcm-device.h					\
	gcm-device-xrandr.c				\
	gcm-device-xrandr.h				\
	gcm-device-udev.c				\
	gcm-device-udev.h				\
	gcm-device-cups.c				\
	gcm-device-cups.h				\
	gcm-device-virtual.c				\
	gcm-device-virtual.h				\
	gcm-cie-widget.c				\
	gcm-cie-widget.h				\
	gcm-trc-widget.c				\
	gcm-trc-widget.h				\
	gcm-gamma-widget.c				\
	gcm-gamma-widget.h				\
	gcm-profile-store.c				\
	gcm-profile-store.h				\
	gcm-profile-lcms1.c				\
	gcm-profile-lcms1.h				\
	gcm-profile.c					\
	gcm-profile.h					\
	gcm-calibrate.c					\
	gcm-calibrate.h					\
	gcm-calibrate-argyll.c				\
	gcm-calibrate-argyll.h				\
	gcm-calibrate-manual.c				\
	gcm-calibrate-manual.h				\
	gcm-calibrate-dialog.c				\
	gcm-calibrate-dialog.h				\
	gcm-brightness.c				\
	gcm-brightness.h

libgcmshared_a_SOURCES +=				\
	gcm-device-sane.c				\
	gcm-device-sane.h

libgcmshared_a_CFLAGS =					\
	$(WARNINGFLAGS_C)

sbin_PROGRAMS =						\
	gcm-install-system-wide

bin_PROGRAMS =						\
	gcm-inspect					\
	gcm-dump-profile				\
	gcm-fix-profile					\
	gcm-dump-edid					\
	gcm-apply					\
	gcm-prefs					\
	gcm-session					\
	gcm-picker					\
	gcm-import

if GCM_USE_EXIV
libexec_PROGRAMS = gcm-helper-exiv
gcm_helper_exiv_SOURCES = gcm-helper-exiv.cpp
gcm_helper_exiv_LDADD = $(EXIV_LIBS)
gcm_helper_exiv_CXXFLAGS = -Wall
endif

gcm_install_system_wide_SOURCES =			\
	gcm-install-system-wide.c

gcm_install_system_wide_LDADD =				\
	$(GLIB_LIBS)

gcm_install_system_wide_CFLAGS =			\
	$(WARNINGFLAGS_C)

gcm_dump_edid_SOURCES =					\
	gcm-dump-edid.c

gcm_dump_edid_LDADD =					\
	libgcmshared.a					\
	$(GLIB_LIBS)					\
	$(X11_LIBS)					\
	$(GNOMEDESKTOP_LIBS)				\
	$(GUDEV_LIBS)					\
	$(LCMS_LIBS)					\
	$(XORG_LIBS)					\
	$(GTK_LIBS)					\
	$(SANE_LIBS)					\
	$(CUPS_LIBS)					\
	-lm

gcm_dump_edid_CFLAGS =					\
	$(WARNINGFLAGS_C)

gcm_dump_profile_SOURCES =				\
	gcm-dump-profile.c

gcm_dump_profile_LDADD =				\
	libgcmshared.a					\
	$(GLIB_LIBS)					\
	$(X11_LIBS)					\
	$(GNOMEDESKTOP_LIBS)				\
	$(GUDEV_LIBS)					\
	$(LCMS_LIBS)					\
	$(XORG_LIBS)					\
	$(GTK_LIBS)					\
	$(SANE_LIBS)					\
	$(CUPS_LIBS)					\
	-lm

gcm_dump_profile_CFLAGS =				\
	$(WARNINGFLAGS_C)

gcm_fix_profile_SOURCES =				\
	gcm-fix-profile.c

gcm_fix_profile_LDADD =					\
	$(GLIB_LIBS)					\
	$(X11_LIBS)					\
	$(LCMS_LIBS)					\
	$(GTK_LIBS)					\
	$(SANE_LIBS)					\
	$(CUPS_LIBS)					\
	-lm

gcm_fix_profile_CFLAGS =				\
	$(WARNINGFLAGS_C)

gcm_inspect_SOURCES =					\
	gcm-inspect.c

gcm_inspect_LDADD =					\
	libgcmshared.a					\
	$(GLIB_LIBS)					\
	$(X11_LIBS)					\
	$(GNOMEDESKTOP_LIBS)				\
	$(GUDEV_LIBS)					\
	$(LCMS_LIBS)					\
	$(XORG_LIBS)					\
	$(GTK_LIBS)					\
	$(SANE_LIBS)					\
	$(CUPS_LIBS)					\
	-lm

gcm_inspect_CFLAGS =					\
	$(WARNINGFLAGS_C)

gcm_apply_SOURCES =					\
	gcm-apply.c

gcm_apply_LDADD =					\
	libgcmshared.a					\
	$(GLIB_LIBS)					\
	$(X11_LIBS)					\
	$(GNOMEDESKTOP_LIBS)				\
	$(GUDEV_LIBS)					\
	$(LCMS_LIBS)					\
	$(XORG_LIBS)					\
	$(GTK_LIBS)					\
	$(SANE_LIBS)					\
	$(CUPS_LIBS)					\
	-lm

gcm_apply_CFLAGS =					\
	$(WARNINGFLAGS_C)

gcm_import_SOURCES =					\
	gcm-import.c

gcm_import_LDADD =					\
	libgcmshared.a					\
	$(GLIB_LIBS)					\
	$(X11_LIBS)					\
	$(GNOMEDESKTOP_LIBS)				\
	$(GUDEV_LIBS)					\
	$(LCMS_LIBS)					\
	$(XORG_LIBS)					\
	$(GTK_LIBS)					\
	$(SANE_LIBS)					\
	$(CUPS_LIBS)					\
	-lm

gcm_import_CFLAGS =					\
	$(WARNINGFLAGS_C)

gcm_prefs_SOURCES =					\
	gcm-prefs.c

gcm_prefs_LDADD =					\
	libgcmshared.a					\
	$(GLIB_LIBS)					\
	$(X11_LIBS)					\
	$(GNOMEDESKTOP_LIBS)				\
	$(UNIQUE_LIBS)					\
	$(VTE_LIBS)					\
	$(GUDEV_LIBS)					\
	$(LCMS_LIBS)					\
	$(XORG_LIBS)					\
	$(GTK_LIBS)					\
	$(SANE_LIBS)					\
	$(CUPS_LIBS)					\
	$(TIFF_LIBS)					\
	$(EXIF_LIBS)					\
	$(CANBERRA_LIBS)				\
	-lm

gcm_prefs_CFLAGS =					\
	$(WARNINGFLAGS_C)

gcm_picker_SOURCES =					\
	gcm-picker.c

gcm_picker_LDADD =					\
	libgcmshared.a					\
	$(GLIB_LIBS)					\
	$(X11_LIBS)					\
	$(GNOMEDESKTOP_LIBS)				\
	$(UNIQUE_LIBS)					\
	$(VTE_LIBS)					\
	$(GUDEV_LIBS)					\
	$(LCMS_LIBS)					\
	$(XORG_LIBS)					\
	$(GTK_LIBS)					\
	$(SANE_LIBS)					\
	$(CUPS_LIBS)					\
	$(TIFF_LIBS)					\
	$(EXIF_LIBS)					\
	$(CANBERRA_LIBS)				\
	-lm

gcm_picker_CFLAGS =					\
	$(WARNINGFLAGS_C)

gcm_session_SOURCES =					\
	gcm-session.c

gcm_session_LDADD =					\
	libgcmshared.a					\
	$(GLIB_LIBS)					\
	$(X11_LIBS)					\
	$(GNOMEDESKTOP_LIBS)				\
	$(UNIQUE_LIBS)					\
	$(GUDEV_LIBS)					\
	$(LCMS_LIBS)					\
	$(XORG_LIBS)					\
	$(GTK_LIBS)					\
	$(TIFF_LIBS)					\
	$(EXIF_LIBS)					\
	$(SANE_LIBS)					\
	$(NOTIFY_LIBS)					\
	$(CUPS_LIBS)					\
	-lm

gcm_session_CFLAGS =					\
	$(WARNINGFLAGS_C)

if EGG_BUILD_TESTS

check_PROGRAMS =					\
	gcm-self-test

gcm_self_test_SOURCES =					\
	gcm-self-test.c					\
	$(NULL)

gcm_self_test_LDADD =					\
	libgcmshared.a					\
	$(GLIB_LIBS)					\
	$(X11_LIBS)					\
	$(GNOMEDESKTOP_LIBS)				\
	$(UNIQUE_LIBS)					\
	$(GUDEV_LIBS)					\
	$(LCMS_LIBS)					\
	$(XORG_LIBS)					\
	$(GTK_LIBS)					\
	$(SANE_LIBS)					\
	$(CUPS_LIBS)					\
	$(TIFF_LIBS)					\
	$(EXIF_LIBS)					\
	-lm

gcm_self_test_CFLAGS = $(AM_CFLAGS) $(WARNINGFLAGS_C)

TESTS = gcm-self-test

endif

install-data-hook:
	if test -w $(DESTDIR)$(prefix)/; then \
		mkdir -p $(DESTDIR)$(GCM_SYSTEM_PROFILES_DIR); \
	fi

clean-local:
	rm -f *~

CLEANFILES = $(BUILT_SOURCES)

MAINTAINERCLEANFILES =					\
	*~						\
	Makefile.in


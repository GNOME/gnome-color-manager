INCLUDES =						\
	$(GLIB_CFLAGS)					\
	$(X11_CFLAGS)					\
	$(GTK_CFLAGS)					\
	$(GNOMEDESKTOP_CFLAGS)				\
	$(VTE_CFLAGS)					\
	$(XORG_CFLAGS)					\
	$(CUPS_CFLAGS)					\
	$(SANE_CFLAGS)					\
	$(TIFF_CFLAGS)					\
	$(EXIF_CFLAGS)					\
	$(EXIV_CFLAGS)					\
	$(NOTIFY_CFLAGS)				\
	$(CANBERRA_CFLAGS)				\
	$(CONTROL_CENTER_CFLAGS)			\
	-DG_UDEV_API_IS_SUBJECT_TO_CHANGE		\
	-DGNOME_DESKTOP_USE_UNSTABLE_API		\
	$(GUDEV_CFLAGS)					\
	-I$(top_srcdir)/libcolor-glib			\
	-DLIBCOLOR_GLIB_COMPILATION			\
	-DBINDIR=\"$(bindir)\"				\
	-DLIBEXECDIR=\"$(libexecdir)\"			\
	-DSBINDIR=\"$(sbindir)\"			\
	-DDATADIR=\"$(datadir)\"			\
	-DSYSCONFDIR=\""$(sysconfdir)"\"		\
	-DVERSION="\"$(VERSION)\""			\
	-DLOCALEDIR=\""$(localedir)"\"			\
	-DTESTDATADIR=\""$(top_srcdir)/data/tests"\"	\
	-DGCM_SYSTEM_PROFILES_DIR="\"$(GCM_SYSTEM_PROFILES_DIR)"\" \
	-DI_KNOW_THE_LIBCOLOR_GLIB_API_IS_SUBJECT_TO_CHANGE	\
	-DGCM_DATA=\"$(pkgdatadir)\"

introspectiondir = $(datadir)/dbus-1/interfaces
dist_introspection_DATA = org.gnome.ColorManager.xml

ccpanelsdir = $(PANELS_DIR)
ccpanels_LTLIBRARIES = libcolor.la

noinst_LIBRARIES = libgcmshared.a
libgcmshared_a_SOURCES =				\
	egg-debug.c					\
	egg-debug.h					\
	gcm-screen.c					\
	gcm-screen.h					\
	gcm-exif.c					\
	gcm-exif.h					\
	gcm-print.c					\
	gcm-print.h					\
	gcm-utils.c					\
	gcm-utils.h					\
	gcm-client.c					\
	gcm-client.h					\
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
	gcm-calibrate.c					\
	gcm-calibrate.h					\
	gcm-calibrate-argyll.c				\
	gcm-calibrate-argyll.h				\
	gcm-calibrate-manual.c				\
	gcm-calibrate-manual.h				\
	gcm-calibrate-dialog.c				\
	gcm-calibrate-dialog.h

libgcmshared_a_SOURCES +=				\
	gcm-device-sane.c				\
	gcm-device-sane.h

libgcmshared_a_CFLAGS =					\
	$(WARNINGFLAGS_C)

libgcmshared_a_LIBADD =					\
	$(COLOR_GLIB_LIBS)

sbin_PROGRAMS =						\
	gcm-install-system-wide

bin_PROGRAMS =						\
	gcm-inspect					\
	gcm-dump-profile				\
	gcm-fix-profile					\
	gcm-dump-edid					\
	gcm-apply					\
	gcm-viewer					\
	gcm-session					\
	gcm-picker					\
	gcm-ddc-util					\
	gcm-import					\
	gcm-sensor-example

if HAVE_EXIV
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
	$(XORG_LIBS)					\
	$(GTK_LIBS)					\
	$(SANE_LIBS)					\
	$(CUPS_LIBS)					\
	$(COLOR_GLIB_LIBS)				\
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
	$(XORG_LIBS)					\
	$(GTK_LIBS)					\
	$(SANE_LIBS)					\
	$(CUPS_LIBS)					\
	$(COLOR_GLIB_LIBS)				\
	-lm

gcm_dump_profile_CFLAGS =				\
	$(WARNINGFLAGS_C)

gcm_fix_profile_SOURCES =				\
	gcm-fix-profile.c

gcm_fix_profile_LDADD =					\
	$(GLIB_LIBS)					\
	$(X11_LIBS)					\
	$(GTK_LIBS)					\
	$(SANE_LIBS)					\
	$(CUPS_LIBS)					\
	$(LCMS_LIBS)					\
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
	$(XORG_LIBS)					\
	$(GTK_LIBS)					\
	$(SANE_LIBS)					\
	$(CUPS_LIBS)					\
	$(COLOR_GLIB_LIBS)				\
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
	$(XORG_LIBS)					\
	$(GTK_LIBS)					\
	$(SANE_LIBS)					\
	$(CUPS_LIBS)					\
	$(COLOR_GLIB_LIBS)				\
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
	$(XORG_LIBS)					\
	$(GTK_LIBS)					\
	$(SANE_LIBS)					\
	$(CUPS_LIBS)					\
	$(COLOR_GLIB_LIBS)				\
	-lm

gcm_import_CFLAGS =					\
	$(WARNINGFLAGS_C)

gcm_viewer_SOURCES =					\
	gcm-cell-renderer-profile.c			\
	gcm-cell-renderer-profile.h			\
	gcm-viewer.c

gcm_viewer_LDADD =					\
	libgcmshared.a					\
	$(GLIB_LIBS)					\
	$(X11_LIBS)					\
	$(GNOMEDESKTOP_LIBS)				\
	$(VTE_LIBS)					\
	$(GUDEV_LIBS)					\
	$(XORG_LIBS)					\
	$(GTK_LIBS)					\
	$(SANE_LIBS)					\
	$(CUPS_LIBS)					\
	$(TIFF_LIBS)					\
	$(EXIF_LIBS)					\
	$(COLOR_GLIB_LIBS)				\
	-lm

gcm_viewer_CFLAGS =					\
	$(WARNINGFLAGS_C)

gcm_picker_SOURCES =					\
	gcm-picker.c

gcm_picker_LDADD =					\
	libgcmshared.a					\
	$(GLIB_LIBS)					\
	$(X11_LIBS)					\
	$(GNOMEDESKTOP_LIBS)				\
	$(VTE_LIBS)					\
	$(GUDEV_LIBS)					\
	$(XORG_LIBS)					\
	$(GTK_LIBS)					\
	$(SANE_LIBS)					\
	$(CUPS_LIBS)					\
	$(TIFF_LIBS)					\
	$(EXIF_LIBS)					\
	$(CANBERRA_LIBS)				\
	$(COLOR_GLIB_LIBS)				\
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
	$(GUDEV_LIBS)					\
	$(XORG_LIBS)					\
	$(GTK_LIBS)					\
	$(TIFF_LIBS)					\
	$(EXIF_LIBS)					\
	$(SANE_LIBS)					\
	$(NOTIFY_LIBS)					\
	$(CUPS_LIBS)					\
	$(COLOR_GLIB_LIBS)				\
	-lm

gcm_session_CFLAGS =					\
	$(WARNINGFLAGS_C)

libcolor_la_SOURCES =					\
	cc-color-panel.c				\
	cc-color-panel.h				\
	gcm-cell-renderer-profile.c			\
	gcm-cell-renderer-profile.h			\
	$(libgcmshared_a_SOURCES)			\
	$(NULL)

libcolor_la_LIBADD =					\
	$(GLIB_LIBS)					\
	$(X11_LIBS)					\
	$(GNOMEDESKTOP_LIBS)				\
	$(GUDEV_LIBS)					\
	$(XORG_LIBS)					\
	$(GTK_LIBS)					\
	$(TIFF_LIBS)					\
	$(EXIF_LIBS)					\
	$(SANE_LIBS)					\
	$(NOTIFY_LIBS)					\
	$(CUPS_LIBS)					\
	$(CANBERRA_LIBS)				\
	$(CONTROL_CENTER_LIBS)				\
	$(COLOR_GLIB_LIBS)

libcolor_la_LDFLAGS = -avoid-version -module
libcolor_la_CFLAGS = $(WARNINGFLAGS_C)

COLOR_GLIB_LIBS =						\
	$(top_builddir)/libcolor-glib/libcolor-glib.la

gcm_ddc_util_SOURCES =					\
	gcm-ddc-util.c

gcm_ddc_util_LDADD =					\
	$(GLIB_LIBS)					\
	$(COLOR_GLIB_LIBS)

gcm_ddc_util_CFLAGS =					\
	$(WARNINGFLAGS_C)

if HAVE_TESTS

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
	$(GUDEV_LIBS)					\
	$(XORG_LIBS)					\
	$(GTK_LIBS)					\
	$(SANE_LIBS)					\
	$(CUPS_LIBS)					\
	$(TIFF_LIBS)					\
	$(EXIF_LIBS)					\
	$(COLOR_GLIB_LIBS)				\
	-lm

gcm_self_test_CFLAGS = $(AM_CFLAGS) $(WARNINGFLAGS_C)

TESTS = gcm-self-test

endif

install-data-hook:
	if test -w $(DESTDIR)$(prefix)/; then \
		mkdir -p $(DESTDIR)$(GCM_SYSTEM_PROFILES_DIR); \
	fi

gcm_sensor_example_SOURCES =				\
	gcm-sensor-example.c

gcm_sensor_example_LDADD =				\
	libgcmshared.a					\
	$(GLIB_LIBS)					\
	$(COLOR_GLIB_LIBS)

gcm_sensor_example_CFLAGS =				\
	$(WARNINGFLAGS_C)

clean-local:
	rm -f *~

CLEANFILES = $(BUILT_SOURCES)

MAINTAINERCLEANFILES =					\
	*~						\
	Makefile.in


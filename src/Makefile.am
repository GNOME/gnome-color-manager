INCLUDES =						\
	$(GLIB_CFLAGS)					\
	$(X11_CFLAGS)					\
	$(GTK_CFLAGS)					\
	$(VTE_CFLAGS)					\
	$(XORG_CFLAGS)					\
	$(SANE_CFLAGS)					\
	$(TIFF_CFLAGS)					\
	$(EXIF_CFLAGS)					\
	$(EXIV_CFLAGS)					\
	$(COLORD_CFLAGS)				\
	$(NOTIFY_CFLAGS)				\
	$(CANBERRA_CFLAGS)				\
	$(CONTROL_CENTER_CFLAGS)			\
	$(SETTINGS_DAEMON_CFLAGS)			\
	-DG_UDEV_API_IS_SUBJECT_TO_CHANGE		\
	$(GUDEV_CFLAGS)					\
	-DG_LOG_DOMAIN=\"Gcm\"				\
	-DBINDIR=\"$(bindir)\"				\
	-DLIBEXECDIR=\"$(libexecdir)\"			\
	-DSBINDIR=\"$(sbindir)\"			\
	-DDATADIR=\"$(datadir)\"			\
	-DSYSCONFDIR=\""$(sysconfdir)"\"		\
	-DVERSION="\"$(VERSION)\""			\
	-DLOCALEDIR=\""$(localedir)"\"			\
	-DTESTDATADIR=\""$(top_srcdir)/data/tests"\"	\
	-DGCM_SYSTEM_PROFILES_DIR="\"$(GCM_SYSTEM_PROFILES_DIR)"\" \
	-DGCM_DATA=\"$(pkgdatadir)\"

introspectiondir = $(datadir)/dbus-1/interfaces
dist_introspection_DATA = org.gnome.ColorManager.xml

ccpanelsdir = $(PANELS_DIR)
ccpanels_LTLIBRARIES = libcolor.la
ccpanels_CFLAGS =					\
	$(WARNINGFLAGS_C)

noinst_LIBRARIES = libgcmshared.a
libgcmshared_a_SOURCES =				\
	gcm-debug.c					\
	gcm-debug.h					\
	gcm-sample-window.c				\
	gcm-sample-window.h				\
	gcm-math.c					\
	gcm-math.h					\
	gcm-color.c					\
	gcm-color.h					\
	gcm-sensor.c					\
	gcm-sensor.h					\
	gcm-sensor-client.c				\
	gcm-sensor-client.h				\
	gcm-sensor-huey.c				\
	gcm-sensor-huey.h				\
	gcm-sensor-huey-private.c			\
	gcm-sensor-huey-private.h			\
	gcm-sensor-colormunki.c				\
	gcm-sensor-colormunki.h				\
	gcm-sensor-colormunki-private.c			\
	gcm-sensor-colormunki-private.h			\
	gcm-sensor-dummy.c				\
	gcm-sensor-dummy.h				\
	gcm-tables.c					\
	gcm-tables.h					\
	gcm-edid.c					\
	gcm-edid.h					\
	gcm-dmi.c					\
	gcm-dmi.h					\
	gcm-buffer.c					\
	gcm-buffer.h					\
	gcm-profile.c					\
	gcm-profile.h					\
	gcm-clut.c					\
	gcm-clut.h					\
	gcm-image.c					\
	gcm-image.h					\
	gcm-enum.c					\
	gcm-enum.h					\
	gcm-usb.c					\
	gcm-usb.h					\
	gcm-profile-store.c				\
	gcm-profile-store.h				\
	gcm-x11-output.c				\
	gcm-x11-output.h				\
	gcm-x11-screen.c				\
	gcm-x11-screen.h				\
	gcm-version.h					\
	gcm-brightness.c				\
	gcm-brightness.h				\
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
	gcm-calibrate-native.c				\
	gcm-calibrate-native.h				\
	gcm-calibrate-dialog.c				\
	gcm-calibrate-dialog.h

libgcmshared_a_SOURCES +=				\
	gcm-device-sane.c				\
	gcm-device-sane.h

libgcmshared_a_CFLAGS =					\
	$(WARNINGFLAGS_C)

bin_PROGRAMS =						\
	gcm-inspect					\
	gcm-viewer					\
	gcm-session					\
	gcm-picker					\
	gcm-import

if HAVE_EXIV
libexec_PROGRAMS = gcm-helper-exiv
gcm_helper_exiv_SOURCES = gcm-helper-exiv.cpp
gcm_helper_exiv_LDADD = $(EXIV_LIBS)
gcm_helper_exiv_CXXFLAGS = -Wall
endif

gcm_inspect_SOURCES =					\
	gcm-inspect.c

gcm_inspect_LDADD =					\
	libgcmshared.a					\
	$(GLIB_LIBS)					\
	$(X11_LIBS)					\
	$(GUDEV_LIBS)					\
	$(XORG_LIBS)					\
	$(GTK_LIBS)					\
	$(LCMS_LIBS)					\
	$(SANE_LIBS)					\
	$(COLORD_LIBS)					\
	-lm

gcm_inspect_CFLAGS =					\
	$(WARNINGFLAGS_C)

gcm_import_SOURCES =					\
	gcm-import.c

gcm_import_LDADD =					\
	libgcmshared.a					\
	$(GLIB_LIBS)					\
	$(X11_LIBS)					\
	$(GUDEV_LIBS)					\
	$(XORG_LIBS)					\
	$(GTK_LIBS)					\
	$(LCMS_LIBS)					\
	$(SANE_LIBS)					\
	$(COLORD_LIBS)					\
	-lm

gcm_import_CFLAGS =					\
	$(WARNINGFLAGS_C)

gcm_viewer_SOURCES =					\
	gcm-cell-renderer-profile-text.c		\
	gcm-cell-renderer-profile-text.h		\
	gcm-viewer.c

gcm_viewer_LDADD =					\
	libgcmshared.a					\
	$(GLIB_LIBS)					\
	$(X11_LIBS)					\
	$(VTE_LIBS)					\
	$(GUDEV_LIBS)					\
	$(XORG_LIBS)					\
	$(GTK_LIBS)					\
	$(SANE_LIBS)					\
	$(TIFF_LIBS)					\
	$(LCMS_LIBS)					\
	$(EXIF_LIBS)					\
	$(COLORD_LIBS)					\
	-lm

gcm_viewer_CFLAGS =					\
	$(WARNINGFLAGS_C)

gcm_picker_SOURCES =					\
	gcm-picker.c

gcm_picker_LDADD =					\
	libgcmshared.a					\
	$(GLIB_LIBS)					\
	$(X11_LIBS)					\
	$(VTE_LIBS)					\
	$(GUDEV_LIBS)					\
	$(XORG_LIBS)					\
	$(GTK_LIBS)					\
	$(SANE_LIBS)					\
	$(TIFF_LIBS)					\
	$(USB_LIBS)					\
	$(LCMS_LIBS)					\
	$(EXIF_LIBS)					\
	$(CANBERRA_LIBS)				\
	$(COLORD_LIBS)				\
	-lm

gcm_picker_CFLAGS =					\
	$(WARNINGFLAGS_C)

gcm_session_SOURCES =					\
	gcm-session.c

gcm_session_LDADD =					\
	libgcmshared.a					\
	$(GLIB_LIBS)					\
	$(X11_LIBS)					\
	$(GUDEV_LIBS)					\
	$(XORG_LIBS)					\
	$(GTK_LIBS)					\
	$(TIFF_LIBS)					\
	$(EXIF_LIBS)					\
	$(SANE_LIBS)					\
	$(NOTIFY_LIBS)					\
	$(LCMS_LIBS)					\
	$(COLORD_LIBS)					\
	-lm

gcm_session_CFLAGS =					\
	$(WARNINGFLAGS_C)

libcolor_la_SOURCES =					\
	cc-color-panel.c				\
	cc-color-panel.h				\
	gcm-list-store-profiles.c			\
	gcm-list-store-profiles.h			\
	gcm-cell-renderer-profile-text.c		\
	gcm-cell-renderer-profile-text.h		\
	gcm-cell-renderer-profile-icon.c		\
	gcm-cell-renderer-profile-icon.h		\
	$(libgcmshared_a_SOURCES)			\
	$(NULL)

libcolor_la_LIBADD =					\
	$(GLIB_LIBS)					\
	$(X11_LIBS)					\
	$(GUDEV_LIBS)					\
	$(VTE_LIBS)					\
	$(XORG_LIBS)					\
	$(GTK_LIBS)					\
	$(TIFF_LIBS)					\
	$(EXIF_LIBS)					\
	$(LCMS_LIBS)					\
	$(SANE_LIBS)					\
	$(NOTIFY_LIBS)					\
	$(CANBERRA_LIBS)				\
	$(CONTROL_CENTER_LIBS)				\
	$(COLORD_LIBS)

libcolor_la_LDFLAGS = -avoid-version -module
libcolor_la_CFLAGS = $(WARNINGFLAGS_C)

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
	$(GUDEV_LIBS)					\
	$(LCMS_LIBS)					\
	$(XORG_LIBS)					\
	$(GTK_LIBS)					\
	$(SANE_LIBS)					\
	$(USB_LIBS)					\
	$(TIFF_LIBS)					\
	$(EXIF_LIBS)					\
	-lm

gcm_self_test_CFLAGS = $(AM_CFLAGS) $(WARNINGFLAGS_C)

TESTS = gcm-self-test

endif

clean-local:
	rm -f *~

CLEANFILES = $(BUILT_SOURCES)

MAINTAINERCLEANFILES =					\
	*~						\
	Makefile.in


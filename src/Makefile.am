AM_CPPFLAGS =						\
	$(GLIB_CFLAGS)					\
	$(X11_CFLAGS)					\
	$(GTK_CFLAGS)					\
	$(VTE_CFLAGS)					\
	$(XORG_CFLAGS)					\
	$(TIFF_CFLAGS)					\
	$(EXIF_CFLAGS)					\
	$(EXIV_CFLAGS)					\
	$(GNOME_DESKTOP_CFLAGS)				\
	$(COLORD_CFLAGS)				\
	$(CANBERRA_CFLAGS)				\
	$(CLUTTER_CFLAGS)				\
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
	-DCD_DISABLE_DEPRECATED				\
	-DGCM_DATA=\"$(pkgdatadir)\"

noinst_LIBRARIES = libgcmshared.a
libgcmshared_a_SOURCES =				\
	gcm-hull.c					\
	gcm-hull.h					\
	gcm-cie-widget.c				\
	gcm-cie-widget.h				\
	gcm-clut.c					\
	gcm-clut.h					\
	gcm-debug.c					\
	gcm-debug.h					\
	gcm-exif.c					\
	gcm-exif.h					\
	gcm-image.c					\
	gcm-image.h					\
	gcm-print.c					\
	gcm-print.h					\
	gcm-profile.c					\
	gcm-profile.h					\
	gcm-named-color.c				\
	gcm-named-color.h				\
	gcm-trc-widget.c				\
	gcm-trc-widget.h				\
	gcm-utils.c					\
	gcm-utils.h

if HAVE_CLUTTER
libgcmshared_a_SOURCES +=				\
	gcm-hull-widget.c				\
	gcm-hull-widget.h
endif

libgcmshared_a_CFLAGS =					\
	$(WARNINGFLAGS_C)

bin_PROGRAMS =						\
	gcm-inspect					\
	gcm-viewer					\
	gcm-calibrate					\
	gcm-picker					\
	gcm-import

if HAVE_EXIV
libexec_PROGRAMS = gcm-helper-exiv
gcm_helper_exiv_SOURCES = gcm-helper-exiv.cpp
gcm_helper_exiv_LDADD = $(EXIV_LIBS) $(GLIB_LIBS)
gcm_helper_exiv_CXXFLAGS = -Wall
endif

gcm_calibrate_SOURCES =					\
	gcm-brightness.c				\
	gcm-brightness.h				\
	gcm-calibrate-argyll.c				\
	gcm-calibrate-argyll.h				\
	gcm-calibrate.c					\
	gcm-calibrate.h					\
	gcm-cell-renderer-profile-text.c		\
	gcm-cell-renderer-profile-text.h		\
	gcm-gamma-widget.c				\
	gcm-gamma-widget.h				\
	gcm-calibrate-main.c

gcm_calibrate_LDADD =					\
	libgcmshared.a					\
	$(CANBERRA_LIBS)				\
	$(COLORD_LIBS)					\
	$(EXIF_LIBS)					\
	$(GLIB_LIBS)					\
	$(GTK_LIBS)					\
	$(LCMS_LIBS)					\
	$(TIFF_LIBS)					\
	$(VTE_LIBS)					\
	$(X11_LIBS)					\
	$(XORG_LIBS)					\
	$(GNOME_DESKTOP_LIBS)				\
	-lm

gcm_calibrate_CFLAGS =					\
	$(WARNINGFLAGS_C)

gcm_inspect_SOURCES =					\
	gcm-inspect.c

gcm_inspect_LDADD =					\
	libgcmshared.a					\
	$(GLIB_LIBS)					\
	$(X11_LIBS)					\
	$(XORG_LIBS)					\
	$(GTK_LIBS)					\
	$(LCMS_LIBS)					\
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
	$(XORG_LIBS)					\
	$(GTK_LIBS)					\
	$(LCMS_LIBS)					\
	$(COLORD_LIBS)					\
	-lm

gcm_import_CFLAGS =					\
	$(WARNINGFLAGS_C)

gcm_viewer_SOURCES =					\
	gcm-cell-renderer-profile-text.c		\
	gcm-cell-renderer-profile-text.h		\
	gcm-cell-renderer-color.c			\
	gcm-cell-renderer-color.h			\
	gcm-viewer-resources.c				\
	gcm-viewer-resources.h				\
	gcm-viewer.c

gcm_viewer_LDADD =					\
	libgcmshared.a					\
	$(GLIB_LIBS)					\
	$(X11_LIBS)					\
	$(VTE_LIBS)					\
	$(XORG_LIBS)					\
	$(GTK_LIBS)					\
	$(TIFF_LIBS)					\
	$(LCMS_LIBS)					\
	$(EXIF_LIBS)					\
	$(COLORD_LIBS)					\
	$(CLUTTER_LIBS)					\
	-lm

gcm_viewer_CFLAGS =					\
	$(WARNINGFLAGS_C)

gcm_picker_SOURCES =					\
	gcm-picker-resources.c				\
	gcm-picker-resources.h				\
	gcm-picker.c

gcm_picker_LDADD =					\
	libgcmshared.a					\
	$(GLIB_LIBS)					\
	$(X11_LIBS)					\
	$(VTE_LIBS)					\
	$(XORG_LIBS)					\
	$(GTK_LIBS)					\
	$(TIFF_LIBS)					\
	$(LCMS_LIBS)					\
	$(EXIF_LIBS)					\
	$(CANBERRA_LIBS)				\
	$(COLORD_LIBS)				\
	-lm

gcm_picker_CFLAGS =					\
	$(WARNINGFLAGS_C)

if HAVE_TESTS

check_PROGRAMS =					\
	gcm-self-test

gcm_self_test_SOURCES =					\
	gcm-brightness.c				\
	gcm-brightness.h				\
	gcm-calibrate-argyll.c				\
	gcm-calibrate-argyll.h				\
	gcm-calibrate.c					\
	gcm-calibrate.h					\
	gcm-gamma-widget.c				\
	gcm-gamma-widget.h				\
	gcm-named-color.c				\
	gcm-named-color.h				\
	gcm-self-test.c

gcm_self_test_LDADD =					\
	libgcmshared.a					\
	$(GLIB_LIBS)					\
	$(X11_LIBS)					\
	$(LCMS_LIBS)					\
	$(XORG_LIBS)					\
	$(GTK_LIBS)					\
	$(TIFF_LIBS)					\
	$(COLORD_LIBS)					\
	$(EXIF_LIBS)					\
	$(VTE_LIBS)					\
	$(CANBERRA_LIBS)				\
	$(GNOME_DESKTOP_LIBS)				\
	-lm

gcm_self_test_CFLAGS = $(AM_CFLAGS) $(WARNINGFLAGS_C)

TESTS = gcm-self-test

endif

gcm-picker-resources.c: gcm-picker.gresource.xml ../data/gcm-picker.ui
	glib-compile-resources --target=$@ --sourcedir=$(top_srcdir)/data --generate-source --c-name gcm $(srcdir)/gcm-picker.gresource.xml
gcm-picker-resources.h: gcm-picker.gresource.xml
	glib-compile-resources --target=$@ --sourcedir=$(top_srcdir)/data --generate-header --c-name gcm $(srcdir)/gcm-picker.gresource.xml

gcm-viewer-resources.c: gcm-viewer.gresource.xml ../data/gcm-viewer.ui
	glib-compile-resources --target=$@ --sourcedir=$(top_srcdir)/data --generate-source --c-name gcm $(srcdir)/gcm-viewer.gresource.xml
gcm-viewer-resources.h: gcm-viewer.gresource.xml
	glib-compile-resources --target=$@ --sourcedir=$(top_srcdir)/data --generate-header --c-name gcm $(srcdir)/gcm-viewer.gresource.xml

BUILT_SOURCES =						\
	gcm-picker-resources.c				\
	gcm-picker-resources.h				\
	gcm-viewer-resources.c				\
	gcm-viewer-resources.h				\
	$(NULL)

EXTRA_DIST =						\
	gcm-picker.gresource.xml			\
	gcm-viewer.gresource.xml

clean-local:
	rm -f *~

CLEANFILES = $(BUILT_SOURCES)

MAINTAINERCLEANFILES =					\
	*~						\
	Makefile.in


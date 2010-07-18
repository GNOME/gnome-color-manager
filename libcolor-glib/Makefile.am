NULL =

-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = --add-include-path=$(srcdir)
INTROSPECTION_COMPILER_ARGS = --includedir=$(srcdir)

INCLUDES = \
	$(GLIB_CFLAGS)						\
	$(GTK_CFLAGS)						\
	$(LCMS_CFLAGS)						\
	$(USB_CFLAGS)						\
	-I$(top_srcdir)/libcolor-glib				\
	-I$(top_builddir)/libcolor-glib				\
	-I.							\
	-DLIBCOLOR_GLIB_COMPILATION				\
	-DLOCALSTATEDIR=\""$(localstatedir)"\" 			\
	-DTESTDATADIR=\""$(top_srcdir)/data/tests"\"		\
	-DI_KNOW_THE_LIBCOLOR_GLIB_API_IS_SUBJECT_TO_CHANGE	\
	-DPACKAGE_DATA_DIR=\""$(datadir)"\"			\
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\"

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = 						\
	libcolor-glib.pc

lib_LTLIBRARIES =						\
	libcolor-glib.la

libcolor_glib_includedir = $(includedir)/libcolor-glib

libcolor_glib_include_HEADERS =					\
	libcolor-glib.h						\
	gcm-common.h						\
	gcm-ddc-client.h					\
	gcm-ddc-device.h					\
	gcm-ddc-control.h					\
	gcm-ddc-common.h					\
	gcm-sensor-huey.h					\
	gcm-sensor.h						\
	gcm-edid.h						\
	gcm-enum.h						\
	gcm-profile.h						\
	gcm-profile-store.h					\
	gcm-dmi.h						\
	gcm-xserver.h						\
	gcm-version.h						\
	$(NULL)

libcolor_glib_la_SOURCES =					\
	libcolor-glib.h						\
	egg-debug.c						\
	egg-debug.h						\
	gcm-common.c						\
	gcm-common.h						\
	gcm-sensor.c						\
	gcm-sensor.h						\
	gcm-sensor-huey.c					\
	gcm-sensor-huey.h					\
	gcm-sensor-dummy.c					\
	gcm-sensor-dummy.h					\
	gcm-tables.c						\
	gcm-tables.h						\
	gcm-edid.c						\
	gcm-edid.h						\
	gcm-dmi.c						\
	gcm-dmi.h						\
	gcm-profile.c						\
	gcm-profile.h						\
	gcm-clut.c						\
	gcm-clut.h						\
	gcm-ddc-client.c					\
	gcm-ddc-client.h					\
	gcm-ddc-device.c					\
	gcm-ddc-device.h					\
	gcm-ddc-control.c					\
	gcm-ddc-control.h					\
	gcm-ddc-common.c					\
	gcm-ddc-common.h					\
	gcm-enum.c						\
	gcm-enum.h						\
	gcm-xyz.c						\
	gcm-xyz.h						\
	gcm-profile-store.c					\
	gcm-profile-store.h					\
	gcm-xserver.c						\
	gcm-xserver.h						\
	gcm-version.h						\
	$(NULL)

libcolor_glib_la_LIBADD =					\
	$(USB_LIBS)						\
	$(LCMS_LIBS)						\
	$(GLIB_LIBS)						\
	$(GTK_LIBS)						\
	-lm

libcolor_glib_la_LDFLAGS =					\
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)	\
	-export-dynamic						\
	-no-undefined						\
	-export-symbols-regex '^gcm_.*'				\
	$(NULL)

libcolor_glib_la_CFLAGS =					\
	$(WARNINGFLAGS_C)					\
	$(NULL)

if HAVE_TESTS
check_PROGRAMS =						\
	gcm-self-test

gcm_self_test_SOURCES =						\
	gcm-self-test.c

gcm_self_test_LDADD =						\
	libcolor-glib.la					\
	$(GLIB_LIBS)

gcm_self_test_CFLAGS = $(AM_CFLAGS) $(WARNINGFLAGS_C)

TESTS = gcm-self-test
endif

EXTRA_DIST =							\
	libcolor-glib.pc.in					\
	gcm-version.h.in

CLEANFILES = $(BUILT_SOURCES)

if FALSE
introspection_sources = $(libcolor_glib_la_SOURCES)

gcmGlib-1.0.gir: libcolor-glib.la
gcmGlib_1_0_gir_INCLUDES = GObject-2.0
gcmGlib_1_0_gir_CFLAGS = $(INCLUDES) -DI_KNOW_THE_LIBCOLOR_GLIB_API_IS_SUBJECT_TO_CHANGE
gcmGlib_1_0_gir_LIBS = libcolor-glib.la
gcmGlib_1_0_gir_FILES = $(addprefix $(srcdir)/,$(introspection_sources))
INTROSPECTION_GIRS += gcmGlib-1.0.gir

girdir = $(datadir)/gir-1.0
dist_gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(dist_gir_DATA) $(typelib_DATA)
endif

clean-local:
	rm -f *~
	rm -f $(CLEANFILES)


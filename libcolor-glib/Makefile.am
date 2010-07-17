NULL =

-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = --add-include-path=$(srcdir)
INTROSPECTION_COMPILER_ARGS = --includedir=$(srcdir)

INCLUDES = \
	$(GLIB_CFLAGS)						\
	$(USB_CFLAGS)						\
	-I$(top_srcdir)/libcolor-glib				\
	-I$(top_builddir)/libcolor-glib				\
	-I.							\
	-DGCM_COMPILATION					\
	-DLOCALSTATEDIR=\""$(localstatedir)"\" 			\
	-DTESTDATADIR=\""$(top_srcdir)/data/tests"\"		\
	-DI_KNOW_THE_GCM_GLIB_API_IS_SUBJECT_TO_CHANGE		\
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
	gcm-version.h						\
	gcm-common.h						\
	$(NULL)

libcolor_glib_la_SOURCES =					\
	libcolor-glib.h						\
	gcm-common.h						\
	gcm-common.c						\
	gcm-version.h						\
	$(NULL)

libcolor_glib_la_LIBADD =					\
	$(USB_LIBS)						\
	$(GLIB_LIBS)

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
gcmGlib_1_0_gir_CFLAGS = $(INCLUDES) -DI_KNOW_THE_GCM_GLIB_API_IS_SUBJECT_TO_CHANGE
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


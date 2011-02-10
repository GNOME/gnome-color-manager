INCLUDES =						\
	$(GLIB_CFLAGS)					\
	$(X11_CFLAGS)					\
	$(GTK_CFLAGS)					\
	$(NOTIFY_CFLAGS)				\
	$(CANBERRA_CFLAGS)				\
	$(COLORD_CFLAGS)				\
	$(CONTROL_CENTER_CFLAGS)			\
	-DG_UDEV_API_IS_SUBJECT_TO_CHANGE		\
	$(GUDEV_CFLAGS)					\
	-I$(top_srcdir)/src				\
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

noinst_PROGRAMS =					\
	gcm-dump-profile				\
	gcm-dump-sensor					\
	gcm-fix-profile					\
	gcm-sensor-example

gcm_dump_profile_SOURCES =				\
	gcm-dump-profile.c

gcm_dump_profile_LDADD =				\
	$(GLIB_LIBS)					\
	$(COLORD_LIBS)					\
	$(LCMS_LIBS)					\
	$(top_builddir)/src/libgcmshared.a		\
	-lm

gcm_dump_profile_CFLAGS =				\
	$(WARNINGFLAGS_C)

gcm_dump_sensor_SOURCES =				\
	gcm-dump-sensor.c

gcm_dump_sensor_LDADD =					\
	$(GLIB_LIBS)					\
	$(LCMS_LIBS)					\
	$(USB_LIBS)					\
	$(GUDEV_LIBS)					\
	$(top_builddir)/src/libgcmshared.a		\
	-lm

gcm_dump_sensor_CFLAGS =				\
	$(WARNINGFLAGS_C)

gcm_fix_profile_SOURCES =				\
	gcm-fix-profile.c

gcm_fix_profile_LDADD =					\
	$(GLIB_LIBS)					\
	$(LCMS_LIBS)					\
	$(top_builddir)/src/libgcmshared.a		\
	-lm

gcm_fix_profile_CFLAGS =				\
	$(WARNINGFLAGS_C)

gcm_sensor_example_SOURCES =				\
	gcm-sensor-example.c

gcm_sensor_example_LDADD =				\
	$(GLIB_LIBS)					\
	$(USB_LIBS)					\
	$(GUDEV_LIBS)					\
	$(top_builddir)/src/libgcmshared.a		\
	$(LCMS_LIBS)

gcm_sensor_example_CFLAGS =				\
	$(WARNINGFLAGS_C)

clean-local:
	rm -f *~

CLEANFILES = $(BUILT_SOURCES)

MAINTAINERCLEANFILES =					\
	*~						\
	Makefile.in


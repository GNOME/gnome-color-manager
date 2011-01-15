INCLUDES =						\
	$(GLIB_CFLAGS)					\
	$(X11_CFLAGS)					\
	$(GTK_CFLAGS)					\
	$(NOTIFY_CFLAGS)				\
	$(CANBERRA_CFLAGS)				\
	$(CONTROL_CENTER_CFLAGS)			\
	-DG_UDEV_API_IS_SUBJECT_TO_CHANGE		\
	-DGNOME_DESKTOP_USE_UNSTABLE_API		\
	$(GUDEV_CFLAGS)					\
	-I$(top_srcdir)/libcolor-glib			\
	-DG_LOG_DOMAIN=\"Gcm\"				\
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

noinst_PROGRAMS =					\
	gcm-dump-profile				\
	gcm-dump-sensor					\
	gcm-dump-edid					\
	gcm-fix-profile					\
	gcm-sensor-example

gcm_dump_edid_SOURCES =					\
	gcm-dump-edid.c

gcm_dump_edid_LDADD =					\
	$(COLOR_GLIB_LIBS)				\
	-lm

gcm_dump_edid_CFLAGS =					\
	$(WARNINGFLAGS_C)

gcm_dump_profile_SOURCES =				\
	gcm-dump-profile.c

gcm_dump_profile_LDADD =				\
	$(COLOR_GLIB_LIBS)				\
	-lm

gcm_dump_profile_CFLAGS =				\
	$(WARNINGFLAGS_C)

gcm_dump_sensor_SOURCES =				\
	gcm-dump-sensor.c

gcm_dump_sensor_LDADD =					\
	$(COLOR_GLIB_LIBS)				\
	-lm

gcm_dump_sensor_CFLAGS =				\
	$(WARNINGFLAGS_C)

gcm_fix_profile_SOURCES =				\
	gcm-fix-profile.c

gcm_fix_profile_LDADD =					\
	$(GLIB_LIBS)					\
	$(LCMS_LIBS)					\
	-lm

gcm_fix_profile_CFLAGS =				\
	$(WARNINGFLAGS_C)

COLOR_GLIB_LIBS =					\
	$(top_builddir)/libcolor-glib/libcolor-glib.la

gcm_sensor_example_SOURCES =				\
	gcm-sensor-example.c

gcm_sensor_example_LDADD =				\
	$(COLOR_GLIB_LIBS)

gcm_sensor_example_CFLAGS =				\
	$(WARNINGFLAGS_C)

clean-local:
	rm -f *~

CLEANFILES = $(BUILT_SOURCES)

MAINTAINERCLEANFILES =					\
	*~						\
	Makefile.in


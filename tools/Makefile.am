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

noinst_PROGRAMS =					\
	gcm-dump-profile				\
	gcm-fix-profile					\
	gcm-ddc-util					\
	gcm-sensor-example

if FALSE
gcm_dump_edid_SOURCES =					\
	gcm-dump-edid.c

gcm_dump_edid_LDADD =					\
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
endif

gcm_dump_profile_SOURCES =				\
	gcm-dump-profile.c

gcm_dump_profile_LDADD =				\
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

COLOR_GLIB_LIBS =						\
	$(top_builddir)/libcolor-glib/libcolor-glib.la

gcm_ddc_util_SOURCES =					\
	gcm-ddc-util.c

gcm_ddc_util_LDADD =					\
	$(GLIB_LIBS)					\
	$(COLOR_GLIB_LIBS)

gcm_ddc_util_CFLAGS =					\
	$(WARNINGFLAGS_C)

gcm_sensor_example_SOURCES =				\
	egg-debug.c					\
	egg-debug.h					\
	gcm-sensor-colormunki.c				\
	gcm-sensor-colormunki.h				\
	gcm-sensor-example.c

gcm_sensor_example_LDADD =				\
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


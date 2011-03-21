SUBDIRS = 						\
	huey

INCLUDES =							\
	-I$(top_srcdir)/libcolor-glib				\
	-DI_KNOW_THE_LIBCOLOR_GLIB_API_IS_SUBJECT_TO_CHANGE	\
	-DG_UDEV_API_IS_SUBJECT_TO_CHANGE			\
	-DGCM_I_KNOW_THIS_IS_PRIVATE				\
	$(GUDEV_CFLAGS)						\
	$(GTK_CFLAGS)						\
	$(GLIB_CFLAGS)

COLOR_GLIB_LIBS =						\
	$(top_builddir)/libcolor-glib/libcolor-glib.la

noinst_PROGRAMS =					\
	gcm-parse-beagle

gcm_parse_beagle_SOURCES =				\
	gcm-parse-beagle.c

gcm_parse_beagle_LDADD =				\
	$(COLOR_GLIB_LIBS)					\
	$(GLIB_LIBS)

gcm_parse_beagle_CFLAGS =				\
	$(WARNINGFLAGS_C)

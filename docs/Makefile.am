SUBDIRS = 						\
	huey

INCLUDES =						\
	-I$(top_srcdir)/src				\
	-DG_UDEV_API_IS_SUBJECT_TO_CHANGE		\
	-DGCM_I_KNOW_THIS_IS_PRIVATE			\
	$(GUDEV_CFLAGS)					\
	$(GTK_CFLAGS)					\
	$(GLIB_CFLAGS)

noinst_PROGRAMS =					\
	gcm-parse-beagle

gcm_parse_beagle_SOURCES =				\
	gcm-parse-beagle.c

gcm_parse_beagle_LDADD =				\
	../src/libgcmshared.a				\
	$(GUDEV_LIBS)					\
	$(GLIB_LIBS)

gcm_parse_beagle_CFLAGS =				\
	$(WARNINGFLAGS_C)

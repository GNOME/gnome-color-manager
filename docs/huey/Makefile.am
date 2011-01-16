INCLUDES =							\
	-DG_UDEV_API_IS_SUBJECT_TO_CHANGE			\
	$(GUDEV_CFLAGS)						\
	$(GTK_CFLAGS)						\
	$(GLIB_CFLAGS)

COLOR_GLIB_LIBS =						\
	$(top_builddir)/src/libgcmshared.a

noinst_PROGRAMS =						\
	gcm-parse-huey						\
	gcm-dump-to-values

gcm_parse_huey_SOURCES =					\
	gcm-parse-huey.c

gcm_parse_huey_LDADD =						\
	$(COLOR_GLIB_LIBS)					\
	$(GLIB_LIBS)

gcm_parse_huey_CFLAGS =						\
	$(WARNINGFLAGS_C)

gcm_dump_to_values_SOURCES =					\
	gcm-dump-to-values.c

gcm_dump_to_values_LDADD =					\
	$(COLOR_GLIB_LIBS)					\
	$(GLIB_LIBS)

gcm_dump_to_values_CFLAGS =					\
	$(WARNINGFLAGS_C)

INCLUDES =						\
	$(GLIB_CFLAGS)					\
	$(GTK_CFLAGS)					\
	$(GNOMEDESKTOP_CFLAGS)				\
	-DBINDIR=\"$(bindir)\"			 	\
	-DSYSCONFDIR=\""$(sysconfdir)"\" 		\
	-DVERSION="\"$(VERSION)\"" 			\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"		\
	-DEGG_LOG_FILE=\""$(GCM_LOG_DIR)/gcm"\"		\
	-DEGG_VERBOSE="\"GCM_VERBOSE\""			\
	-DEGG_LOGGING="\"GCM_LOGGING\""			\
	-DEGG_CONSOLE="\"GCM_CONSOLE\""

bin_PROGRAMS =						\
	gcm-import

gcm_import_SOURCES =					\
	egg-debug.c					\
	egg-debug.h					\
	gcm-import.c

gcm_import_LDADD =					\
	$(GLIB_LIBS)					\
	$(GNOMEDESKTOP_LIBS)				\
	$(GTK_LIBS)

gcm_import_CFLAGS =					\
	$(WARNINGFLAGS_C)

clean-local:
	rm -f *~

CLEANFILES = $(BUILT_SOURCES)

MAINTAINERCLEANFILES =					\
	*~			      			\
	Makefile.in


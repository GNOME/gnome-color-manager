INCLUDES =						\
	$(GLIB_CFLAGS)					\
	$(GTK_CFLAGS)					\
	$(GNOMEDESKTOP_CFLAGS)				\
	$(UNIQUE_CFLAGS)				\
	-DBINDIR=\"$(bindir)\"			 	\
	-DSYSCONFDIR=\""$(sysconfdir)"\" 		\
	-DVERSION="\"$(VERSION)\"" 			\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"		\
	-DGCM_DATA=\"$(pkgdatadir)\"			\
	-DEGG_LOG_FILE=\""$(GCM_LOG_DIR)/gcm"\"		\
	-DEGG_VERBOSE="\"GCM_VERBOSE\""			\
	-DEGG_LOGGING="\"GCM_LOGGING\""			\
	-DEGG_CONSOLE="\"GCM_CONSOLE\""

bin_PROGRAMS =						\
	gcm-apply					\
	gcm-prefs					\
	gcm-import

gcm_apply_SOURCES =					\
	egg-debug.c					\
	egg-debug.h					\
	gcm-utils.c					\
	gcm-utils.h					\
	gcm-clut.c					\
	gcm-clut.h					\
	gcm-profile.c					\
	gcm-profile.h					\
	gcm-apply.c

gcm_apply_LDADD =					\
	$(GLIB_LIBS)					\
	$(GNOMEDESKTOP_LIBS)				\
	$(GTK_LIBS)

gcm_apply_CFLAGS =					\
	$(WARNINGFLAGS_C)

gcm_import_SOURCES =					\
	egg-debug.c					\
	egg-debug.h					\
	gcm-profile.c					\
	gcm-profile.h					\
	gcm-import.c

gcm_import_LDADD =					\
	$(GLIB_LIBS)					\
	$(GNOMEDESKTOP_LIBS)				\
	$(GTK_LIBS)

gcm_import_CFLAGS =					\
	$(WARNINGFLAGS_C)

gcm_prefs_SOURCES =					\
	egg-debug.c					\
	egg-debug.h					\
	gcm-prefs.c

gcm_prefs_LDADD =					\
	$(GLIB_LIBS)					\
	$(GNOMEDESKTOP_LIBS)				\
	$(UNIQUE_LIBS)					\
	$(GTK_LIBS)

gcm_prefs_CFLAGS =					\
	$(WARNINGFLAGS_C)

clean-local:
	rm -f *~

CLEANFILES = $(BUILT_SOURCES)

MAINTAINERCLEANFILES =					\
	*~			      			\
	Makefile.in


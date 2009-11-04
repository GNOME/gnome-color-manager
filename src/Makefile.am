INCLUDES =						\
	$(GLIB_CFLAGS)					\
	$(GTK_CFLAGS)					\
	$(GNOMEDESKTOP_CFLAGS)				\
	$(UNIQUE_CFLAGS)				\
	$(GCONF_CFLAGS)					\
	$(VTE_CFLAGS)					\
	$(DBUS_GLIB_CFLAGS)				\
	-DG_UDEV_API_IS_SUBJECT_TO_CHANGE		\
	-DGNOME_DESKTOP_USE_UNSTABLE_API		\
	$(GUDEV_CFLAGS)					\
	-DBINDIR=\"$(bindir)\"			 	\
	-DSYSCONFDIR=\""$(sysconfdir)"\" 		\
	-DVERSION="\"$(VERSION)\"" 			\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"		\
	-DGCM_DATA=\"$(pkgdatadir)\"			\
	-DEGG_LOG_FILE=\""$(GCM_LOG_DIR)/gcm"\"		\
	-DEGG_VERBOSE="\"GCM_VERBOSE\""			\
	-DEGG_LOGGING="\"GCM_LOGGING\""			\
	-DEGG_CONSOLE="\"GCM_CONSOLE\""

noinst_LIBRARIES = libgcmshared.a
libgcmshared_a_SOURCES =				\
	egg-debug.c					\
	egg-debug.h					\
	gcm-utils.c					\
	gcm-utils.h					\
	gcm-clut.c					\
	gcm-clut.h					\
	gcm-edid.c					\
	gcm-edid.h					\
	gcm-xserver.c					\
	gcm-xserver.h					\
	gcm-client.c					\
	gcm-client.h					\
	gcm-device.c					\
	gcm-device.h					\
	gcm-profile.c					\
	gcm-profile.h

libgcmshared_a_CFLAGS =					\
	$(WARNINGFLAGS_C)

bin_PROGRAMS =						\
	gcm-apply					\
	gcm-prefs					\
	gcm-import

noinst_PROGRAMS =					\
	gcm-inspect

gcm_inspect_SOURCES =					\
	gcm-inspect.c

gcm_inspect_LDADD =					\
	libgcmshared.a					\
	$(GLIB_LIBS)					\
	$(GNOMEDESKTOP_LIBS)				\
	$(GCONF_LIBS)					\
	$(GUDEV_LIBS)					\
	$(GTK_LIBS)

gcm_inspect_CFLAGS =					\
	$(WARNINGFLAGS_C)

gcm_apply_SOURCES =					\
	gcm-apply.c

gcm_apply_LDADD =					\
	libgcmshared.a					\
	$(GLIB_LIBS)					\
	$(GNOMEDESKTOP_LIBS)				\
	$(GCONF_LIBS)					\
	$(GUDEV_LIBS)					\
	$(GTK_LIBS)

gcm_apply_CFLAGS =					\
	$(WARNINGFLAGS_C)

gcm_import_SOURCES =					\
	gcm-import.c

gcm_import_LDADD =					\
	libgcmshared.a					\
	$(GLIB_LIBS)					\
	$(GNOMEDESKTOP_LIBS)				\
	$(GCONF_LIBS)					\
	$(GUDEV_LIBS)					\
	$(GTK_LIBS)

gcm_import_CFLAGS =					\
	$(WARNINGFLAGS_C)

gcm_prefs_SOURCES =					\
	gcm-calibrate.c					\
	gcm-calibrate.h					\
	gcm-brightness.c				\
	gcm-brightness.h				\
	gcm-prefs.c

gcm_prefs_LDADD =					\
	libgcmshared.a					\
	$(GLIB_LIBS)					\
	$(GNOMEDESKTOP_LIBS)				\
	$(UNIQUE_LIBS)					\
	$(GCONF_LIBS)					\
	$(VTE_LIBS)					\
	$(GUDEV_LIBS)					\
	$(DBUS_GLIB_LIBS)				\
	$(GTK_LIBS)

gcm_prefs_CFLAGS =					\
	$(WARNINGFLAGS_C)

clean-local:
	rm -f *~

CLEANFILES = $(BUILT_SOURCES)

MAINTAINERCLEANFILES =					\
	*~			      			\
	Makefile.in


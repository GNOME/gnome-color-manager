INCLUDES =						\
	$(GLIB_CFLAGS)					\
	$(GTK_CFLAGS)					\
	$(GNOMEDESKTOP_CFLAGS)				\
	$(UNIQUE_CFLAGS)				\
	$(GCONF_CFLAGS)					\
	$(VTE_CFLAGS)					\
	$(LCMS_CFLAGS)					\
	$(XORG_CFLAGS)					\
	$(DBUS_GLIB_CFLAGS)				\
	-DG_UDEV_API_IS_SUBJECT_TO_CHANGE		\
	-DGNOME_DESKTOP_USE_UNSTABLE_API		\
	$(GUDEV_CFLAGS)					\
	-DBINDIR=\"$(bindir)\"			 	\
	-DSYSCONFDIR=\""$(sysconfdir)"\" 		\
	-DVERSION="\"$(VERSION)\"" 			\
	-DLOCALEDIR=\""$(localedir)"\"			\
	-DGCM_DATA=\"$(pkgdatadir)\"

noinst_LIBRARIES = libgcmshared.a
libgcmshared_a_SOURCES =				\
	egg-debug.c					\
	egg-debug.h					\
	gcm-xyz.c					\
	gcm-xyz.h					\
	gcm-utils.c					\
	gcm-utils.h					\
	gcm-clut.c					\
	gcm-clut.h					\
	gcm-edid.c					\
	gcm-edid.h					\
	gcm-dmi.c					\
	gcm-dmi.h					\
	gcm-tables.c					\
	gcm-tables.h					\
	gcm-xserver.c					\
	gcm-xserver.h					\
	gcm-client.c					\
	gcm-client.h					\
	gcm-color-device.c				\
	gcm-color-device.h				\
	gcm-device.c					\
	gcm-device.h					\
	gcm-cie-widget.c				\
	gcm-cie-widget.h				\
	gcm-trc-widget.c				\
	gcm-trc-widget.h				\
	gcm-gamma-widget.c				\
	gcm-gamma-widget.h				\
	gcm-profile.c					\
	gcm-profile.h

libgcmshared_a_CFLAGS =					\
	$(WARNINGFLAGS_C)

bin_PROGRAMS =						\
	gcm-inspect					\
	gcm-dump-profile				\
	gcm-dump-edid					\
	gcm-apply					\
	gcm-prefs					\
	gcm-session					\
	gcm-import

gcm_dump_edid_SOURCES =					\
	gcm-dump-edid.c

gcm_dump_edid_LDADD =					\
	libgcmshared.a					\
	$(GLIB_LIBS)					\
	$(GNOMEDESKTOP_LIBS)				\
	$(GCONF_LIBS)					\
	$(GUDEV_LIBS)					\
	$(LCMS_LIBS)					\
	$(XORG_LIBS)					\
	$(DBUS_GLIB_LIBS)				\
	$(GTK_LIBS)

gcm_dump_edid_CFLAGS =					\
	$(WARNINGFLAGS_C)

gcm_dump_profile_SOURCES =				\
	gcm-dump-profile.c

gcm_dump_profile_LDADD =				\
	libgcmshared.a					\
	$(GLIB_LIBS)					\
	$(GNOMEDESKTOP_LIBS)				\
	$(GCONF_LIBS)					\
	$(GUDEV_LIBS)					\
	$(LCMS_LIBS)					\
	$(XORG_LIBS)					\
	$(DBUS_GLIB_LIBS)				\
	$(GTK_LIBS)

gcm_dump_profile_CFLAGS =				\
	$(WARNINGFLAGS_C)

gcm_inspect_SOURCES =					\
	gcm-inspect.c

gcm_inspect_LDADD =					\
	libgcmshared.a					\
	$(GLIB_LIBS)					\
	$(GNOMEDESKTOP_LIBS)				\
	$(GCONF_LIBS)					\
	$(GUDEV_LIBS)					\
	$(LCMS_LIBS)					\
	$(XORG_LIBS)					\
	$(DBUS_GLIB_LIBS)				\
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
	$(LCMS_LIBS)					\
	$(XORG_LIBS)					\
	$(DBUS_GLIB_LIBS)				\
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
	$(LCMS_LIBS)					\
	$(XORG_LIBS)					\
	$(DBUS_GLIB_LIBS)				\
	$(GTK_LIBS)

gcm_import_CFLAGS =					\
	$(WARNINGFLAGS_C)

gcm_prefs_SOURCES =					\
	gcm-calibrate.c					\
	gcm-calibrate.h					\
	gcm-calibrate-argyll.c				\
	gcm-calibrate-argyll.h				\
	gcm-calibrate-manual.c				\
	gcm-calibrate-manual.h				\
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
	$(LCMS_LIBS)					\
	$(DBUS_GLIB_LIBS)				\
	$(XORG_LIBS)					\
	$(GTK_LIBS)

gcm_prefs_CFLAGS =					\
	$(WARNINGFLAGS_C)

gcm_session_SOURCES =					\
	gcm-dbus.c					\
	gcm-dbus.h					\
	gcm-session.c

gcm_session_LDADD =					\
	libgcmshared.a					\
	$(GLIB_LIBS)					\
	$(GNOMEDESKTOP_LIBS)				\
	$(UNIQUE_LIBS)					\
	$(GCONF_LIBS)					\
	$(GUDEV_LIBS)					\
	$(LCMS_LIBS)					\
	$(DBUS_GLIB_LIBS)				\
	$(XORG_LIBS)					\
	$(GTK_LIBS)

gcm_session_CFLAGS =					\
	$(WARNINGFLAGS_C)

if EGG_BUILD_TESTS

check_PROGRAMS =					\
	gcm-self-test

gcm_self_test_SOURCES =					\
	gcm-self-test.c					\
	gcm-edid.c					\
	gcm-tables.c					\
	gcm-utils.c					\
	gcm-device.c					\
	gcm-profile.c					\
	gcm-brightness.c				\
	gcm-clut.c					\
	gcm-dmi.c					\
	gcm-xyz.c					\
	gcm-cie-widget.c				\
	gcm-trc-widget.c				\
	gcm-gamma-widget.c				\
	gcm-calibrate.c					\
	gcm-calibrate-manual.c				\
	egg-test.h					\
	egg-test.c					\
	$(NULL)

gcm_self_test_LDADD =					\
	libgcmshared.a					\
	$(GLIB_LIBS)					\
	$(GNOMEDESKTOP_LIBS)				\
	$(UNIQUE_LIBS)					\
	$(GCONF_LIBS)					\
	$(GUDEV_LIBS)					\
	$(LCMS_LIBS)					\
	$(DBUS_GLIB_LIBS)				\
	$(XORG_LIBS)					\
	$(GTK_LIBS)

gcm_self_test_CFLAGS = -DEGG_TEST $(AM_CFLAGS)

TESTS = gcm-self-test

endif

org.gnome.ColorManager.h: org.gnome.ColorManager.xml
	$(LIBTOOL) --mode=execute dbus-binding-tool	\
		--prefix=gcm_dbus			\
		--mode=glib-server			\
		--output=org.gnome.ColorManager.h	\
		$(srcdir)/org.gnome.ColorManager.xml

clean-local:
	rm -f *~

CLEANFILES = $(BUILT_SOURCES)

MAINTAINERCLEANFILES =					\
	*~			      			\
	Makefile.in

EXTRA_DIST =						\
	org.gnome.ColorManager.xml

BUILT_SOURCES = 					\
	org.gnome.ColorManager.h


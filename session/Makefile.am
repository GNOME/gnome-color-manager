INCLUDES =							\
	-DI_KNOW_THE_LIBCOLOR_GLIB_API_IS_SUBJECT_TO_CHANGE	\
	-DG_UDEV_API_IS_SUBJECT_TO_CHANGE			\
	-DBINDIR=\"$(bindir)\"					\
	-I$(top_srcdir)/libcolor-glib				\
	$(GLIB_CFLAGS)						\
	$(GTK_CFLAGS)						\
	$(SETTINGS_DAEMON_CFLAGS)				\
	$(GUDEV_CFLAGS)

plugindir = $(PLUGINDIR)
plugin_LTLIBRARIES =						\
	libcolor.la
plugin_CFLAGS =							\
	$(WARNINGFLAGS_C)

libcolor_la_SOURCES =						\
	egg-console-kit.h					\
	egg-console-kit.c					\
	gsd-color-plugin.h					\
	gsd-color-plugin.c					\
	gsd-color-manager.h					\
	gsd-color-manager.c

libcolor_la_CPPFLAGS =						\
	-DGNOME_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"

libcolor_la_LDFLAGS = -avoid-version -module

libcolor_la_CFLAGS =						\
	$(SETTINGS_PLUGIN_CFLAGS)				\
	$(GTK_CFLAGS)						\
	$(GNOME_CFLAGS)

COLOR_GLIB_LIBS =						\
	$(top_builddir)/libcolor-glib/libcolor-glib.la

libcolor_la_LIBADD =						\
	$(COLOR_GLIB_LIBS)

plugin_DATA = color.gnome-settings-plugin

EXTRA_DIST =							\
	$(plugin_DATA)


SUBDIRS = 						\
	tests						\
	targets						\
	figures						\
	icons

@INTLTOOL_DESKTOP_RULE@
desktopdir = $(datadir)/applications
desktop_in_files =					\
	gcm-viewer.desktop.in				\
	gcm-picker.desktop.in				\
	gcm-calibrate.desktop.in			\
	gcm-import.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@
autostartdir = $(sysconfdir)/xdg/autostart
autostart_in_files = gcm-session.desktop.in
autostart_DATA = $(autostart_in_files:.desktop.in=.desktop)

servicedir       = $(datadir)/dbus-1/services
service_in_files = org.gnome.ColorManager.service.in
service_DATA     = $(service_in_files:.service.in=.service)

$(service_DATA): $(service_in_files) Makefile
	@sed -e "s|\@servicedir\@|$(bindir)|" $< > $@

pkgdata_DATA =						\
	gcm-spawn.ui					\
	gcm-picker.ui					\
	gcm-viewer.ui

EXTRA_DIST =						\
	$(service_in_files)				\
	$(autostart_in_files)				\
	$(desktop_in_files)				\
	$(pkgdata_DATA)

clean-local :
	rm -f *~

DISTCLEANFILES =					\
	org.gnome.ColorManager.service			\
	gcm-session.desktop				\
	gcm-calibrate.desktop				\
	gcm-picker.desktop				\
	gcm-viewer.desktop				\
	gcm-import.desktop


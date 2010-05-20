SUBDIRS = 						\
	tests						\
	targets						\
	figures						\
	icons

@INTLTOOL_DESKTOP_RULE@
desktopdir = $(datadir)/applications
desktop_in_files =					\
	gcm-prefs.desktop.in				\
	gcm-import.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

@INTLTOOL_DESKTOP_RULE@
autostartdir = $(sysconfdir)/xdg/autostart
autostart_in_files = gcm-apply.desktop.in
autostart_DATA = $(autostart_in_files:.desktop.in=.desktop)

servicedir       = $(datadir)/dbus-1/services
service_in_files = org.gnome.ColorManager.service.in
service_DATA     = $(service_in_files:.service.in=.service)

$(service_DATA): $(service_in_files) Makefile
	@sed -e "s|\@servicedir\@|$(bindir)|" $< > $@

@GSETTINGS_RULES@
gsettings_SCHEMAS = org.gnome.color-manager.gschema.xml

pkgdata_DATA =						\
	gcm-calibrate.ui				\
	gcm-spawn.ui					\
	gcm-picker.ui					\
	gcm-prefs.ui

EXTRA_DIST =						\
	$(gsettings_SCHEMAS)				\
	$(service_in_files)				\
	$(autostart_in_files)				\
	$(desktop_in_files)				\
	$(pkgdata_DATA)

clean-local :
	rm -f *~

DISTCLEANFILES =					\
	org.gnome.ColorManager.service			\
	gcm-apply.desktop				\
	gcm-prefs.desktop				\
	gcm-import.desktop


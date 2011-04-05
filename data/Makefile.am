SUBDIRS = 						\
	tests						\
	targets						\
	figures						\
	icons

migrationdir = $(datadir)/GConf/gsettings
dist_migration_DATA = org.gnome.color-manager.gschema.migrate

@INTLTOOL_DESKTOP_RULE@
desktopdir = $(datadir)/applications
desktop_in_files =					\
	gcm-viewer.desktop.in				\
	gcm-picker.desktop.in				\
	gcm-prefs.desktop.in				\
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

@GSETTINGS_RULES@
gsettings_SCHEMAS =					\
	org.gnome.color-manager.gschema.xml		\
	org.gnome.settings-daemon.plugins.color.gschema.xml

pkgdata_DATA =						\
	gcm-calibrate.ui				\
	gcm-spawn.ui					\
	gcm-picker.ui					\
	gcm-viewer.ui					\
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
	gcm-session.desktop				\
	gcm-prefs.desktop				\
	gcm-picker.desktop				\
	gcm-viewer.desktop				\
	gcm-import.desktop


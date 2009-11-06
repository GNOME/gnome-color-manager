SUBDIRS = 						\
	icons						\
	profiles

@INTLTOOL_SCHEMAS_RULE@
schemadir = $(sysconfdir)/gconf/schemas
schema_in_files = gnome-color-manager.schemas.in
schema_DATA = $(schema_in_files:.schemas.in=.schemas)

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

pkgdata_DATA =						\
	gcm-calibrate.ui				\
	gcm-prefs.ui

EXTRA_DIST =						\
	$(schema_in_files)				\
	$(autostart_in_files)				\
	$(desktop_in_files)				\
	$(pkgdata_DATA)

if GCONF_SCHEMAS_INSTALL
install-data-local:
	-GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) \
	$(GCONFTOOL) --makefile-install-rule $(schema_DATA)
endif

if GCONF_SCHEMAS_INSTALL
uninstall-local:
	-GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) \
	$(GCONFTOOL) --makefile-uninstall-rule $(schema_DATA)
endif

clean-local :
	rm -f *~

DISTCLEANFILES =					\
	gnome-color-manager.schemas			\
	gcm-apply.desktop				\
	gcm-prefs.desktop				\
	gcm-import.desktop


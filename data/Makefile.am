SUBDIRS = 						\
	tests						\
	targets						\
	figures						\
	icons

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

gsettingsschema_DATA = org.gnome.ColorManager.gschema.xml
gsettingsschemadir = $(datadir)/glib-2.0/schemas
org.gnome.ColorManager.gschema.xml: gnome-color-manager.schemas
	gsettings-schema-convert --force --gconf --xml --schema-id $(@:.gschema.xml=) $< -o $@
if GSETTINGS_SCHEMAS_INSTALL
# --allow-any-name should be removed once the move from gconf is complete
install-data-hook:
	$(GLIB_COMPILE_SCHEMAS) --allow-any-name $(DESTDIR)$(gsettingsschemadir)
endif

pkgdata_DATA =						\
	gcm-calibrate.ui				\
	gcm-spawn.ui					\
	gcm-picker.ui					\
	gcm-prefs.ui

EXTRA_DIST =						\
	$(schema_in_files)				\
	$(service_in_files)				\
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

CLEANFILES =						\
	$(gsettingsschema_DATA)

DISTCLEANFILES =					\
	org.gnome.ColorManager.service			\
	gnome-color-manager.schemas			\
	gcm-apply.desktop				\
	gcm-prefs.desktop				\
	gcm-import.desktop


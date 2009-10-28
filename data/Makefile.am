SUBDIRS = 						\
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

pkgdata_DATA =						\
	gcm-prefs.ui

EXTRA_DIST =						\
	$(schema_in_files)				\
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
	gcm-prefs.desktop				\
	gcm-import.desktop


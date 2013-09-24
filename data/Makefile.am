SUBDIRS = 						\
	ti1						\
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

@INTLTOOL_XML_NOMERGE_RULE@
appdatadir = $(datadir)/appdata
dist_appdata_DATA = $(appdata_in_files:.xml.in=.xml)
appdata_in_files = gcm-viewer.appdata.xml.in

UI_FILES =						\
	gcm-picker.ui					\
	gcm-viewer.ui

EXTRA_DIST =						\
	$(service_in_files)				\
	$(appdata_in_files)				\
	$(autostart_in_files)				\
	$(desktop_in_files)				\
	$(UI_FILES)

clean-local :
	rm -f *~

DISTCLEANFILES =					\
	org.gnome.ColorManager.service			\
	gcm-calibrate.desktop				\
	gcm-picker.desktop				\
	gcm-viewer.desktop				\
	gcm-import.desktop


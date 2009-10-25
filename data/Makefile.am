@INTLTOOL_DESKTOP_RULE@
desktopdir = $(datadir)/applications
desktop_in_files =					\
	gcm-import.desktop.in

desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

pkgdata_DATA =						\
	gcm-prefs.ui

EXTRA_DIST =						\
	$(desktop_in_files)				\
	$(pkgdata_DATA)

clean-local :
	rm -f *~

DISTCLEANFILES =					\
	gcm-import.desktop


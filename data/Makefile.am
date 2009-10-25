@INTLTOOL_DESKTOP_RULE@
desktopdir = $(datadir)/applications
desktop_in_files =					\
	gcm-import.desktop.in

desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

EXTRA_DIST =						\
	$(desktop_in_files)

clean-local :
	rm -f *~

DISTCLEANFILES =					\
	gcm-import.desktop


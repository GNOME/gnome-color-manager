SUBDIRS = 						\
	appdata						\
	ti1						\
	tests						\
	targets						\
	figures						\
	icons

@INTLTOOL_DESKTOP_RULE@
desktopdir = $(datadir)/applications
desktop_in_files =					\
	org.gnome.ColorProfileViewer.desktop.in		\
	gcm-picker.desktop.in				\
	gcm-calibrate.desktop.in			\
	gcm-import.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

UI_FILES =						\
	gcm-picker.ui					\
	gcm-viewer.ui

EXTRA_DIST =						\
	$(service_in_files)				\
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

-include $(top_srcdir)/git.mk

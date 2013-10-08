themedir = $(datadir)/icons/hicolor
size = 48x48
context = apps

iconsdir = $(themedir)/$(size)/$(context)

icons_DATA = 			  	\
	gnome-color-manager.png

install-data-hook:
	mkdir -p $(DESTDIR)$(DATADIR)/icons/hicolor/$(size)/mimetypes;
	ln -fs $(iconsdir)/gnome-color-manager.png $(DESTDIR)$(DATADIR)/icons/hicolor/$(size)/mimetypes/application-vnd.iccprofile.png;

EXTRA_DIST =		\
	$(icons_DATA)

-include $(top_srcdir)/git.mk

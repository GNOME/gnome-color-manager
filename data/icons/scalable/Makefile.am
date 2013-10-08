themedir = $(datadir)/icons/hicolor
size = scalable
context = apps

iconsdir = $(themedir)/$(size)/$(context)
icons_DATA = 		 		\
	gnome-color-manager.svg

appiconsdir = $(datadir)/gnome-color-manager/icons
appicons_DATA = 	 		\
	lcd.svg	  			\
	crt.svg 			\
	projector.svg

install-data-hook:
	mkdir -p $(DESTDIR)$(DATADIR)/icons/hicolor/$(size)/mimetypes;
	ln -fs $(iconsdir)/gnome-color-manager.svg $(DESTDIR)$(DATADIR)/icons/hicolor/$(size)/mimetypes/application-vnd.iccprofile.svg;

EXTRA_DIST =				\
	$(icons_DATA)			\
	$(appicons_DATA)

-include $(top_srcdir)/git.mk

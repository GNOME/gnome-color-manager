INCLUDES =							\
	$(GLIB_CFLAGS)						\
	$(GUDEV_CFLAGS)						\
	$(GTK_CFLAGS)						\
	-I$(top_srcdir)/libcolor-glib				\
	-DG_UDEV_API_IS_SUBJECT_TO_CHANGE			\
	-DI_KNOW_THE_LIBCOLOR_GLIB_API_IS_SUBJECT_TO_CHANGE

COLOR_GLIB_LIBS =						\
	$(top_builddir)/libcolor-glib/libcolor-glib.la

udevrulesdir = $(slashlibdir)/udev/rules.d
udevrules_DATA =						\
	69-gcm-colorimeters.rules				\
	95-gcm-devices.rules					\
	55-gcm-i2c.rules

udevproberdir = $(slashlibdir)/udev
udevprober_PROGRAMS =						\
	gcm-udev-ddc

gcm_udev_ddc_SOURCES =						\
	gcm-udev-ddc.c

gcm_udev_ddc_LDADD =						\
	$(COLOR_GLIB_LIBS)					\
	-lm

gcm_udev_ddc_CFLAGS =						\
	$(WARNINGFLAGS_C)

EXTRA_DIST =							\
	$(udevrules_DATA)

clean-local :
	rm -f *~


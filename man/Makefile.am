EXTRA_DIST =						\
	gcm-calibrate.sgml				\
	gcm-import.sgml					\
	gcm-inspect.sgml				\
	gcm-picker.sgml					\
	gcm-viewer.sgml

if HAVE_DOCBOOK2MAN
man_MANS =						\
	gcm-calibrate.1					\
	gcm-import.1					\
	gcm-inspect.1					\
	gcm-picker.1					\
	gcm-viewer.1
endif

if HAVE_DOCBOOK2MAN
gcm-calibrate.1: gcm-calibrate.sgml
	docbook2man $? > /dev/null
gcm-import.1: gcm-import.sgml
	docbook2man $? > /dev/null
gcm-inspect.1: gcm-inspect.sgml
	docbook2man $? > /dev/null
gcm-picker.1: gcm-picker.sgml
	docbook2man $? > /dev/null
gcm-viewer.1: gcm-viewer.sgml
	docbook2man $? > /dev/null
endif

clean-local :
	rm -f *~
	rm -f *.1
	rm -f manpage.*


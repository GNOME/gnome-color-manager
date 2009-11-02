EXTRA_DIST =						\
	gcm-apply.sgml					\
	gcm-prefs.sgml					\
	gcm-import.sgml

if HAVE_DOCBOOK2MAN
man_MANS =						\
	gcm-apply.1					\
	gcm-prefs.1					\
	gcm-import.1
endif

if HAVE_DOCBOOK2MAN
gcm-apply.1: gcm-apply.sgml
	docbook2man $? > /dev/null
gcm-prefs.1: gcm-prefs.sgml
	docbook2man $? > /dev/null
gcm-import.1: gcm-import.sgml
	docbook2man $? > /dev/null
endif

clean-local :
	rm -f *~
	rm -f *.1
	rm -f manpage.*


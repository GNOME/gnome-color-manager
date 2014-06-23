@APPSTREAM_XML_RULES@
@INTLTOOL_XML_RULE@
appstream_XML = $(appdata_in_files:.xml.in=.xml)
appdata_in_files = gcm-viewer.appdata.xml.in
EXTRA_DIST = $(appdata_in_files)
CLEANFILES = $(appstream_XML)

-include $(top_srcdir)/git.mk

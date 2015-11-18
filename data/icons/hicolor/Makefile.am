DISTCLEANFILES =
CLEANFILES = 
BUILT_SOURCES =
EXTRA_DIST =

noinst_LTLIBRARIES = libicons.la

nodist_libicons_la_SOURCES = \
	ide-icons-resources.c \
	ide-icons-resources.h

libicons_la_CFLAGS = $(ICONS_CFLAGS)
libicons_la_LIBADD = $(ICONS_LIBS)

glib_resources_c = ide-icons-resources.c
glib_resources_h = ide-icons-resources.h
glib_resources_xml = icons.gresource.xml
glib_resources_namespace = ide_icons
include $(top_srcdir)/build/autotools/Makefile.am.gresources

-include $(top_srcdir)/git.mk

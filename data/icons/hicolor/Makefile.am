DISTCLEANFILES =
CLEANFILES = 
BUILT_SOURCES =
EXTRA_DIST =

noinst_LTLIBRARIES = libicons.la

nodist_libicons_la_SOURCES = \
	gb-icons-resources.c \
	gb-icons-resources.h

libicons_la_CFLAGS = $(ICONS_CFLAGS)
libicons_la_LIBADD = $(ICONS_LIBS)

glib_resources_c = gb-icons-resources.c
glib_resources_h = gb-icons-resources.h
glib_resources_xml = icons.gresource.xml
glib_resources_namespace = gb_icons
include $(top_srcdir)/build/autotools/Makefile.am.gresources

-include $(top_srcdir)/git.mk

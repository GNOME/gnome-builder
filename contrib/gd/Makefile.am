DISTCLEANFILES =
CLEANFILES =
BUILT_SOURCES =
EXTRA_DIST =

pkglibdir = $(libdir)/gnome-builder
pkglib_LTLIBRARIES = libgd-private.la

libgd_private_la_SOURCES =        \
	gd-tagged-entry.c         \
	gd-tagged-entry.h         \
	$(NULL)

nodist_libgd_private_la_SOURCES = \
	gd-resources.c            \
	gd-resources.h

glib_resources_c = gd-resources.c
glib_resources_h = gd-resources.h
glib_resources_xml = gd-tagged-entry.gresource.xml
glib_resources_namespace = gd
include $(top_srcdir)/build/autotools/Makefile.am.gresources

libgd_private_la_CFLAGS = $(GD_CFLAGS)
libgd_private_la_LIBADD = $(GD_LIBS)

-include $(top_srcdir)/git.mk

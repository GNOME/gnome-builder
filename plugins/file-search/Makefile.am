DISTCLEANFILES =
BUILT_SOURCES =
CLEANFILES =
EXTRA_DIST =

noinst_LTLIBRARIES = libfile-search.la

libfile_search_la_SOURCES = \
	gb-file-search-plugin.c \
	gb-file-search-provider.c \
	gb-file-search-provider.h \
	gb-file-search-result.c \
	gb-file-search-result.h \
	gb-file-search-index.c \
	gb-file-search-index.h \
	$(NULL)

nodist_libfile_search_la_SOURCES = \
	gb-file-search-resources.c \
	gb-file-search-resources.h \
	$(NULL)

libfile_search_la_CFLAGS = \
	$(BUILDER_CFLAGS) \
	-I$(top_srcdir)/libide \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/contrib/search \
	$(NULL)

libfile_search_la_LIBADD = \
	$(BUILDER_LIBS) \
	$(top_builddir)/contrib/search/libsearch.la \
	$(NULL)

libfile_search_la_LDFLAGS = -module

glib_resources_c = gb-file-search-resources.c
glib_resources_h = gb-file-search-resources.h
glib_resources_xml = gb-file-search.gresource.xml
glib_resources_namespace = gb_file_search
include $(top_srcdir)/build/autotools/Makefile.am.gresources

-include $(top_srcdir)/git.mk

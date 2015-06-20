plugindir = $(libdir)/gnome-builder/plugins
plugin_LTLIBRARIES = libfile-search.la
plugin_DATA = file-search.plugin

libfile_search_la_SOURCES = \
	gb-file-search-provider.c \
	gb-file-search-provider.h \
	gb-file-search-result.c \
	gb-file-search-result.h \
	gb-file-search-index.c \
	gb-file-search-index.h \
	$(NULL)

libfile_search_la_CFLAGS = \
	$(BUILDER_CFLAGS) \
	-I$(top_srcdir)/libide \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/search \
	-I$(top_srcdir)/src/workbench \
	-I$(top_srcdir)/contrib/search \
	$(NULL)

libfile_search_la_LIBADD = \
	$(BUILDER_LIBS) \
	$(top_builddir)/contrib/search/libsearch.la \
	$(NULL)

libfile_search_la_LDFLAGS = \
	-module \
	-avoid-version \
	$(NULL)

-include $(top_srcdir)/git.mk

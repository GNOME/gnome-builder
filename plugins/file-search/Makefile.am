EXTRA_DIST = $(plugin_DATA)

plugindir = $(libdir)/gnome-builder/plugins
plugin_LTLIBRARIES = libfile-search.la
dist_plugin_DATA = file-search.plugin

libfile_search_la_SOURCES =       \
	gb-file-search-provider.c \
	gb-file-search-provider.h \
	gb-file-search-result.c   \
	gb-file-search-result.h   \
	gb-file-search-index.c    \
	gb-file-search-index.h    \
	$(NULL)

libfile_search_la_CFLAGS = $(PLUGIN_CFLAGS)
libfile_search_la_LIBADD = $(top_builddir)/contrib/search/libsearch.la
libfile_search_la_LDFLAGS = $(PLUGIN_LDFLAGS)

-include $(top_srcdir)/git.mk

include $(top_srcdir)/plugins/Makefile.plugin

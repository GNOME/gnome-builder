DISTCLEANFILES =
BUILT_SOURCES =
CLEANFILES =
EXTRA_DIST = $(plugin_DATA)

plugindir = $(libdir)/gnome-builder/plugins
plugin_LTLIBRARIES = libdevhelp-plugin.la
plugin_DATA = devhelp.plugin

libdevhelp_plugin_la_SOURCES = \
	gb-devhelp-document.c \
	gb-devhelp-document.h \
	gb-devhelp-panel.c \
	gb-devhelp-panel.h \
	gb-devhelp-plugin.c \
	gb-devhelp-private.h \
	gb-devhelp-resources.c \
	gb-devhelp-resources.h \
	gb-devhelp-search-provider.c \
	gb-devhelp-search-provider.h \
	gb-devhelp-search-result.c \
	gb-devhelp-search-result.h \
	gb-devhelp-view.c \
	gb-devhelp-view.h \
	$(NULL)

libdevhelp_plugin_la_CFLAGS = \
	$(BUILDER_CFLAGS) \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/documents \
	-I$(top_srcdir)/src/search \
	-I$(top_srcdir)/src/views \
	-I$(top_srcdir)/src/util \
	-I$(top_srcdir)/src/workbench \
	-I$(top_srcdir)/src/workspace \
	-I$(top_srcdir)/libide \
	$(NULL)

libdevhelp_plugin_la_LIBADD = \
	$(BUILDER_LIBS) \
	$(top_builddir)/libide/libide-1.0.la \
	$(NULL)

libdevhelp_plugin_la_LDFLAGS = \
	$(OPTIMIZE_LDFLAGS) \
	-avoid-version \
	-module \
	$(NULL)

glib_resources_c = gb-devhelp-resources.c
glib_resources_h = gb-devhelp-resources.h
glib_resources_xml = gb-devhelp.gresource.xml
glib_resources_namespace = gb_devhelp
include $(top_srcdir)/build/autotools/Makefile.am.gresources

-include $(top_srcdir)/git.mk

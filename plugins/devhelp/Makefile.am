if ENABLE_DEVHELP_PLUGIN

DISTCLEANFILES =
BUILT_SOURCES =
CLEANFILES =
EXTRA_DIST = $(plugin_DATA)

plugindir = $(libdir)/gnome-builder/plugins
plugin_LTLIBRARIES = libdevhelp-plugin.la
dist_plugin_DATA = devhelp.plugin

libdevhelp_plugin_la_SOURCES = \
	gbp-devhelp-panel.c \
	gbp-devhelp-panel.h \
	gbp-devhelp-plugin.c \
	gbp-devhelp-search-provider.c \
	gbp-devhelp-search-provider.h \
	gbp-devhelp-search-result.c \
	gbp-devhelp-search-result.h \
	gbp-devhelp-view.c \
	gbp-devhelp-view.h \
	gbp-devhelp-workbench-addin.c \
	gbp-devhelp-workbench-addin.h \
	$(NULL)

nodist_libdevhelp_plugin_la_SOURCES = \
	gbp-devhelp-resources.c \
	gbp-devhelp-resources.h

libdevhelp_plugin_la_CFLAGS = \
	$(LIBIDE_CFLAGS) \
	$(DEVHELP_CFLAGS) \
	$(OPTIMIZE_CFLAGS) \
	-I$(top_srcdir)/libide \
	$(NULL)

libdevhelp_plugin_la_LIBADD = $(DEVHELP_LIBS)

libdevhelp_plugin_la_LDFLAGS = \
	$(OPTIMIZE_LDFLAGS) \
	-avoid-version \
	-module \
	-export-regex peas_register_types \
	$(NULL)

glib_resources_c = gbp-devhelp-resources.c
glib_resources_h = gbp-devhelp-resources.h
glib_resources_xml = gbp-devhelp-resources.gresource.xml
glib_resources_namespace = gbp_devhelp
include $(top_srcdir)/build/autotools/Makefile.am.gresources

include $(top_srcdir)/plugins/Makefile.plugin

endif

-include $(top_srcdir)/git.mk

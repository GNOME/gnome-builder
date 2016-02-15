if ENABLE_BUILD_TOOLS_PLUGIN

DISTCLEANFILES =
BUILT_SOURCES =
CLEANFILES =
EXTRA_DIST = $(plugin_DATA)

plugindir = $(libdir)/gnome-builder/plugins
plugin_LTLIBRARIES = libbuild-tools-plugin.la
dist_plugin_DATA = build-tools.plugin

libbuild_tools_plugin_la_SOURCES = \
	gbp-build-configuration-row.c \
	gbp-build-configuration-row.h \
	gbp-build-configuration-view.c \
	gbp-build-configuration-view.h \
	gbp-build-log-panel.c \
	gbp-build-log-panel.h \
	gbp-build-panel.c \
	gbp-build-panel.h \
	gbp-build-panel-row.c \
	gbp-build-panel-row.h \
	gbp-build-perspective.c \
	gbp-build-perspective.h \
	gbp-build-plugin.c \
	gbp-build-tool.c \
	gbp-build-tool.h \
	gbp-build-workbench-addin.c \
	gbp-build-workbench-addin.h \
	$(NULL)

nodist_libbuild_tools_plugin_la_SOURCES = \
	gbp-build-resources.c \
	gbp-build-resources.h

libbuild_tools_plugin_la_CFLAGS = \
	$(LIBIDE_CFLAGS) \
	$(OPTIMIZE_CFLAGS) \
	-I$(top_srcdir)/libide \
	-I$(top_srcdir)/contrib/egg \
	$(NULL)

libbuild_tools_plugin_la_LDFLAGS = \
	$(OPTIMIZE_LDFLAGS) \
	-avoid-version \
	-module \
	-export-regex peas_register_types \
	$(NULL)

glib_resources_c = gbp-build-resources.c
glib_resources_h = gbp-build-resources.h
glib_resources_xml = gbp-build-tools.gresource.xml
glib_resources_namespace = gbp_build_tools
include $(top_srcdir)/build/autotools/Makefile.am.gresources

include $(top_srcdir)/plugins/Makefile.plugin

endif

-include $(top_srcdir)/git.mk

if ENABLE_RUN_TOOLS_PLUGIN

DISTCLEANFILES =
BUILT_SOURCES =
CLEANFILES =
EXTRA_DIST = $(plugin_DATA)

plugindir = $(libdir)/gnome-builder/plugins
plugin_LTLIBRARIES = librun-tools-plugin.la
dist_plugin_DATA = run-tools.plugin

librun_tools_plugin_la_CFLAGS = \
	$(PLUGIN_CFLAGS) \
	-DG_LOG_DOMAIN="\"run-tools\"" \
	$(NULL)
librun_tools_plugin_la_LDFLAGS = $(PLUGIN_LDFLAGS)
librun_tools_plugin_la_SOURCES = \
	gbp-run-plugin.c \
	gbp-run-workbench-addin.c \
	gbp-run-workbench-addin.h \
	$(NULL)

include $(top_srcdir)/plugins/Makefile.plugin

endif

-include $(top_srcdir)/git.mk

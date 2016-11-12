if ENABLE_TERMINAL_PLUGIN

DISTCLEANFILES =
BUILT_SOURCES =
CLEANFILES =
EXTRA_DIST = $(plugin_DATA)

plugindir = $(libdir)/gnome-builder/plugins
plugin_LTLIBRARIES = libterminal.la
dist_plugin_DATA = terminal.plugin

libterminal_la_SOURCES = \
	gb-terminal.c \
	gb-terminal.h \
	gb-terminal-application-addin.c \
	gb-terminal-application-addin.h \
	gb-terminal-plugin.c \
	gb-terminal-private.h \
	gb-terminal-util.c \
	gb-terminal-util.h \
	gb-terminal-view.c \
	gb-terminal-view.h \
	gb-terminal-view-private.h \
	gb-terminal-view-actions.c \
	gb-terminal-view-actions.h \
	gb-terminal-workbench-addin.c \
	gb-terminal-workbench-addin.h \
	$(NULL)

nodist_libterminal_la_SOURCES = \
	gb-terminal-resources.c \
	gb-terminal-resources.h \
	$(NULL)

libterminal_la_CFLAGS = $(PLUGIN_CFLAGS) $(TERMINAL_CFLAGS)
libterminal_la_LIBADD = $(TERMINAL_LIBS)
libterminal_la_LDFLAGS = $(PLUGIN_LDFLAGS)

glib_resources_c = gb-terminal-resources.c
glib_resources_h = gb-terminal-resources.h
glib_resources_xml = gb-terminal.gresource.xml
glib_resources_namespace = gb_terminal
include $(top_srcdir)/build/autotools/Makefile.am.gresources

include $(top_srcdir)/plugins/Makefile.plugin

endif

-include $(top_srcdir)/git.mk

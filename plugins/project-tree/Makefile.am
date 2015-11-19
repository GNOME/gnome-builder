if ENABLE_DEVHELP_PLUGIN

DISTCLEANFILES =
BUILT_SOURCES =
CLEANFILES =
EXTRA_DIST = $(plugin_DATA)

plugindir = $(libdir)/gnome-builder/plugins
plugin_LTLIBRARIES = libproject-tree-plugin.la
dist_plugin_DATA = project-tree.plugin

libproject_tree_plugin_la_SOURCES = \
	gb-new-file-popover.c \
	gb-new-file-popover.h \
	gb-project-file.c \
	gb-project-file.h \
	gb-project-tree-actions.c \
	gb-project-tree-actions.h \
	gb-project-tree-builder.c \
	gb-project-tree-builder.h \
	gb-project-tree.c \
	gb-project-tree.h \
	gb-project-tree-private.h \
	gb-rename-file-popover.c \
	gb-rename-file-popover.h \
	gb-project-tree-addin.c \
	gb-project-tree-addin.h \
	project-tree-plugin.c \
	$(NULL)

nodist_libproject_tree_plugin_la_SOURCES = \
	gb-project-tree-resources.c \
	gb-project-tree-resources.h

libproject_tree_plugin_la_CFLAGS = \
	$(LIBIDE_CFLAGS) \
	$(OPTIMIZE_CFLAGS) \
	$(DEBUG_CFLAGS) \
	-I$(top_srcdir)/libide \
	$(NULL)

libproject_tree_plugin_la_LIBADD =

if ENABLE_TERMINAL_PLUGIN
libproject_tree_plugin_la_LIBADD += $(TERMINAL_LIBS)
libproject_tree_plugin_la_CFLAGS += \
	$(TERMINAL_CFLAGS) \
	-DHAVE_VTE
endif

libproject_tree_plugin_la_LDFLAGS = \
	$(OPTIMIZE_LDFLAGS) \
	-avoid-version \
	-module \
	$(NULL)

glib_resources_c = gb-project-tree-resources.c
glib_resources_h = gb-project-tree-resources.h
glib_resources_xml = gb-project-tree.gresource.xml
glib_resources_namespace = gb_project_tree
include $(top_srcdir)/build/autotools/Makefile.am.gresources

include $(top_srcdir)/plugins/Makefile.plugin

endif

-include $(top_srcdir)/git.mk

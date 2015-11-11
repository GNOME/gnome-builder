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

libproject_tree_plugin_la_CFLAGS = \
	$(LIBIDE_CFLAGS) \
	$(BUILDER_CFLAGS) \
	$(OPTIMIZE_CFLAGS) \
	$(DEBUG_CFLAGS) \
	-I$(top_srcdir)/libide \
	-I$(top_srcdir)/src/tree \
	-I$(top_srcdir)/src/util \
	$(NULL)

libproject_tree_plugin_la_LIBADD = \
	$(LIBIDE_LIBS) \
	$(BUILDER_LIBS) \
	$(top_builddir)/libide/libide-1.0.la \
	$(NULL)

libproject_tree_plugin_la_LDFLAGS = \
	$(OPTIMIZE_LDFLAGS) \
	-avoid-version \
	-module \
	$(NULL)

include $(top_srcdir)/plugins/Makefile.plugin

endif

-include $(top_srcdir)/git.mk

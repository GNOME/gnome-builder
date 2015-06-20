DISTCLEANFILES =
BUILT_SOURCES =
CLEANFILES =
EXTRA_DIST =

plugindir = $(libdir)/gnome-builder/plugins
plugin_LTLIBRARIES = libcommand-bar.la
plugin_DATA = command-bar.plugin

libcommand_bar_la_SOURCES = \
	gb-command-bar-resources.c \
	gb-command-bar-resources.h \
	gb-command-bar.c \
	gb-command-bar.h \
	gb-command-gaction-provider.c \
	gb-command-gaction-provider.h \
	gb-command-gaction.c \
	gb-command-gaction.h \
	gb-command-manager.c \
	gb-command-manager.h \
	gb-command-provider.c \
	gb-command-provider.h \
	gb-command-result.c \
	gb-command-result.h \
	gb-command-vim-provider.c \
	gb-command-vim-provider.h \
	gb-command-vim.c \
	gb-command-vim.h \
	gb-command.c \
	gb-command.h \
	gb-vim.c \
	gb-vim.h \
	$(NULL)

libcommand_bar_la_CFLAGS = \
	-I$(top_srcdir)/libide \
	-I$(top_srcdir)/src/workspace \
	-I$(top_srcdir)/src/workbench \
	-I$(top_srcdir)/src/views \
	-I$(top_srcdir)/src/documents \
	-I$(top_srcdir)/src/editor \
	-I$(top_srcdir)/src/util \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/contrib/gd \
	-I$(top_srcdir)/contrib/nautilus \
	$(BUILDER_CFLAGS) \
	$(DEBUG_CFLAGS) \
	$(OPTIMIZE_CFLAGS) \
	$(NULL)

libcommand_bar_la_LIBADD = \
	$(BUILDER_LIBS) \
	$(NULL)

libcommand_bar_la_LDFLAGS = \
	$(OPTIMIZE_LDFLAGS) \
	-avoid-version \
	-module \
	$(NULL)

glib_resources_c = gb-command-bar-resources.c
glib_resources_h = gb-command-bar-resources.h
glib_resources_xml = gb-command-bar.gresource.xml
glib_resources_namespace = gb_command_bar
include $(top_srcdir)/build/autotools/Makefile.am.gresources

-include $(top_srcdir)/git.mk

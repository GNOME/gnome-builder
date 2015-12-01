if ENABLE_GIT_PLUGIN

DISTCLEANFILES =
BUILT_SOURCES =
CLEANFILES =
EXTRA_DIST = $(plugin_DATA)

plugindir = $(libdir)/gnome-builder/plugins
plugin_LTLIBRARIES = libgit-plugin.la
dist_plugin_DATA = git.plugin

libgit_plugin_la_SOURCES = \
	ide-git-buffer-change-monitor.c \
	ide-git-buffer-change-monitor.h \
	ide-git-clone-widget.c \
	ide-git-clone-widget.h \
	ide-git-genesis-addin.c \
	ide-git-genesis-addin.h \
	ide-git-plugin.c \
	ide-git-remote-callbacks.c \
	ide-git-remote-callbacks.h \
	ide-git-vcs.c \
	ide-git-vcs.h \
	$(NULL)

nodist_libgit_plugin_la_SOURCES = \
	ide-git-resources.c \
	ide-git-resources.h

libgit_plugin_la_CFLAGS = \
	$(LIBIDE_CFLAGS) \
	$(OPTIMIZE_CFLAGS) \
	-I$(top_srcdir)/libide \
	-I$(top_srcdir)/contrib/egg \
	$(NULL)

libgit_plugin_la_LIBADD = $(GIT_LIBS)

libgit_plugin_la_LDFLAGS = \
	$(OPTIMIZE_LDFLAGS) \
	-avoid-version \
	-module \
	-export-regex peas_register_types \
	$(NULL)

glib_resources_c = ide-git-resources.c
glib_resources_h = ide-git-resources.h
glib_resources_xml = ide-git-resources.gresource.xml
glib_resources_namespace = ide_git
include $(top_srcdir)/build/autotools/Makefile.am.gresources

include $(top_srcdir)/plugins/Makefile.plugin

endif

-include $(top_srcdir)/git.mk

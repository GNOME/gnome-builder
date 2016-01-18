if ENABLE_DEVHELP_PLUGIN

DISTCLEANFILES =
BUILT_SOURCES =
CLEANFILES =
EXTRA_DIST = $(plugin_DATA)

plugindir = $(libdir)/gnome-builder/plugins
plugin_LTLIBRARIES = libsupport-plugin.la
dist_plugin_DATA = support.plugin

libsupport_plugin_la_SOURCES = \
	ide-support-application-addin.c \
	ide-support-application-addin.h \
	ide-support.c \
	ide-support.h \
	ide-support-plugin.c \
	$(NULL)

nodist_libsupport_plugin_la_SOURCES = \
	ide-support-resources.c \
	ide-support-resources.h

libsupport_plugin_la_CFLAGS = \
	$(LIBIDE_CFLAGS) \
	$(OPTIMIZE_CFLAGS) \
	-I$(top_srcdir)/libide \
	-I$(top_srcdir)/contrib/egg \
	-I$(top_srcdir)/contrib/doap \
	$(NULL)

libsupport_plugin_la_LDFLAGS = \
	$(OPTIMIZE_LDFLAGS) \
	-avoid-version \
	-module \
	-export-regex peas_register_types \
	$(NULL)

glib_resources_c = ide-support-resources.c
glib_resources_h = ide-support-resources.h
glib_resources_xml = ide-support-resources.gresource.xml
glib_resources_namespace = ide_support
include $(top_srcdir)/build/autotools/Makefile.am.gresources

include $(top_srcdir)/plugins/Makefile.plugin

endif

-include $(top_srcdir)/git.mk

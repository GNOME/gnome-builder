DISTCLEANFILES =
BUILT_SOURCES =
CLEANFILES =
EXTRA_DIST =

plugindir = $(libdir)/gnome-builder/plugins
plugin_LTLIBRARIES = libdevice-manager.la
plugin_DATA = device-manager.plugin

libdevice_manager_la_SOURCES = \
	gb-device-manager-panel.c \
	gb-device-manager-panel.h \
	gb-device-manager-tree-builder.c \
	gb-device-manager-tree-builder.h \
	$(NULL)

libdevice_manager_la_CFLAGS = \
	$(BUILDER_CFLAGS) \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/tree \
	-I$(top_srcdir)/src/workbench \
	-I$(top_srcdir)/src/workspace \
	-I$(top_srcdir)/libide \
	$(NULL)

libdevice_manager_la_LIBADD = \
	$(BUILDER_LIBS) \
	$(top_builddir)/libide/libide-1.0.la \
	$(NULL)

libdevice_manager_la_LDFLAGS = \
	-avoid-version \
	-module \
	$(NULL)

nodist_libdevice_manager_la_SOURCES = \
	gb-device-manager-resources.c \
	gb-device-manager-resources.h


glib_resources_c = gb-device-manager-resources.c
glib_resources_h = gb-device-manager-resources.h
glib_resources_xml = gb-device-manager.gresource.xml
glib_resources_namespace = gb_device_manager
include $(top_srcdir)/build/autotools/Makefile.am.gresources


-include $(top_srcdir)/git.mk

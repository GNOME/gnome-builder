if ENABLE_DEVICE_MANAGER_PLUGIN

DISTCLEANFILES =
BUILT_SOURCES =
CLEANFILES =
EXTRA_DIST = $(plugin_DATA)

plugindir = $(libdir)/gnome-builder/plugins
plugin_LTLIBRARIES = libdevice-manager.la
dist_plugin_DATA = device-manager.plugin

libdevice_manager_la_SOURCES = \
	gb-device-manager-panel.c \
	gb-device-manager-panel.h \
	gb-device-manager-tree-builder.c \
	gb-device-manager-tree-builder.h \
	$(NULL)

libdevice_manager_la_CFLAGS = \
	$(LIBIDE_CFLAGS) \
	-I$(top_srcdir)/libide \
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

include $(top_srcdir)/plugins/Makefile.plugin

endif

-include $(top_srcdir)/git.mk

if ENABLE_SYSMON_PLUGIN

DISTCLEANFILES =
BUILT_SOURCES =
CLEANFILES =
EXTRA_DIST = $(plugin_DATA)

plugindir = $(libdir)/gnome-builder/plugins
plugin_LTLIBRARIES = libsysmon.la
dist_plugin_DATA = sysmon.plugin

libsysmon_la_SOURCES = \
	gb-sysmon-panel.c \
	gb-sysmon-panel.h \
	gb-sysmon-addin.c \
	gb-sysmon-addin.h \
	$(NULL)

nodist_libsysmon_la_SOURCES = \
	gb-sysmon-resources.c \
	gb-sysmon-resources.h \
	$(NULL)

libsysmon_la_CFLAGS = $(PLUGIN_CFLAGS)
libsysmon_la_LIBADD = $(top_builddir)/contrib/rg/librg.la
libsysmon_la_LDFLAGS = $(PLUGIN_LDFLAGS)

glib_resources_c = gb-sysmon-resources.c
glib_resources_h = gb-sysmon-resources.h
glib_resources_xml = gb-sysmon.gresource.xml
glib_resources_namespace = gb_sysmon
include $(top_srcdir)/build/autotools/Makefile.am.gresources

include $(top_srcdir)/plugins/Makefile.plugin

endif

-include $(top_srcdir)/git.mk

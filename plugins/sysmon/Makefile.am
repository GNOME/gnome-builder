DISTCLEANFILES =
BUILT_SOURCES =
CLEANFILES =
EXTRA_DIST =

plugindir = $(libdir)/gnome-builder/plugins
plugin_LTLIBRARIES = libsysmon.la
plugin_DATA = sysmon.plugin

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

libsysmon_la_CFLAGS = \
	$(BUILDER_CFLAGS) \
	-I$(top_srcdir)/libide \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/workbench \
	-I$(top_srcdir)/src/workspace \
	-I$(top_srcdir)/contrib/egg \
	-I$(top_srcdir)/contrib/rg \
	$(NULL)

libsysmon_la_LIBADD = \
	$(BUILDER_LIBS) \
	$(top_builddir)/contrib/rg/librg.la \
	$(NULL)

libsysmon_la_LDFLAGS = \
	-avoid-version \
	-module \
	$(NULL)

glib_resources_c = gb-sysmon-resources.c
glib_resources_h = gb-sysmon-resources.h
glib_resources_xml = gb-sysmon.gresource.xml
glib_resources_namespace = gb_sysmon
include $(top_srcdir)/build/autotools/Makefile.am.gresources


-include $(top_srcdir)/git.mk

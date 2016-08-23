if ENABLE_SYSPROF_PLUGIN

DISTCLEANFILES =
BUILT_SOURCES =
CLEANFILES =
EXTRA_DIST = $(plugin_DATA)

plugindir = $(libdir)/gnome-builder/plugins
plugin_LTLIBRARIES = libsysprof-plugin.la
dist_plugin_DATA = sysprof.plugin

libsysprof_plugin_la_SOURCES = \
	gbp-sysprof-plugin.c \
	gbp-sysprof-perspective.c \
	gbp-sysprof-perspective.h \
	gbp-sysprof-workbench-addin.c \
	gbp-sysprof-workbench-addin.h \
	$(NULL)

nodist_libsysprof_plugin_la_SOURCES = \
	gbp-sysprof-resources.c \
	gbp-sysprof-resources.h

libsysprof_plugin_la_CFLAGS = $(PLUGIN_CFLAGS) $(SYSPROF_CFLAGS)
libsysprof_plugin_la_LIBADD = $(PLUGIN_LIBS) $(SYSPROF_LIBS)

glib_resources_c = gbp-sysprof-resources.c
glib_resources_h = gbp-sysprof-resources.h
glib_resources_xml = sysprof.gresource.xml
glib_resources_namespace = gbp_sysprof
include $(top_srcdir)/build/autotools/Makefile.am.gresources

include $(top_srcdir)/plugins/Makefile.plugin

endif

-include $(top_srcdir)/git.mk

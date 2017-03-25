if ENABLE_RETAB_PLUGIN

DISTCLEANFILES =
BUILT_SOURCES =
CLEANFILES =
EXTRA_DIST = $(plugin_DATA)

plugindir = $(libdir)/gnome-builder/plugins
plugin_LTLIBRARIES = libretab-plugin.la
dist_plugin_DATA = retab.plugin

libretab_plugin_la_SOURCES =        \
	gbp-retab-plugin.c          \
	gbp-retab-view-addin.c      \
	gbp-retab-view-addin.h      \
	$(NULL)

nodist_libretab_plugin_la_SOURCES = \
	gbp-retab-resources.c       \
	gbp-retab-resources.h

libretab_plugin_la_CFLAGS = $(PLUGIN_CFLAGS)
libretab_plugin_la_LDFLAGS = $(PLUGIN_LDFLAGS)

glib_resources_c = gbp-retab-resources.c
glib_resources_h = gbp-retab-resources.h
glib_resources_xml = gbp-retab.gresource.xml
glib_resources_namespace = gbp_retab
include $(top_srcdir)/build/autotools/Makefile.am.gresources

include $(top_srcdir)/plugins/Makefile.plugin

endif

-include $(top_srcdir)/git.mk

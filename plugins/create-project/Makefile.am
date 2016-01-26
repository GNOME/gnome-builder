if ENABLE_CREATE_PROJECT_PLUGIN

DISTCLEANFILES =
BUILT_SOURCES =
CLEANFILES =
EXTRA_DIST = $(plugin_DATA)

plugindir = $(libdir)/gnome-builder/plugins
plugin_LTLIBRARIES = libcreate-project-plugin.la
dist_plugin_DATA = create-project.plugin

libcreate_project_plugin_la_SOURCES = \
	gbp-create-project-plugin.c \
	gbp-create-project-tool.c \
	gbp-create-project-tool.h \
	$(NULL)

nodist_libcreate_project_plugin_la_SOURCES = \
	gbp-create-project-resources.c \
	gbp-create-project-resources.h

libcreate_project_plugin_la_CFLAGS = \
	$(LIBIDE_CFLAGS) \
	$(OPTIMIZE_CFLAGS) \
	-I$(top_srcdir)/libide \
	-I$(top_srcdir)/contrib/egg \
	$(NULL)

libcreate_project_plugin_la_LDFLAGS = \
	$(OPTIMIZE_LDFLAGS) \
	-avoid-version \
	-module \
	-export-regex peas_register_types \
	$(NULL)

glib_resources_c = gbp-create-project-resources.c
glib_resources_h = gbp-create-project-resources.h
glib_resources_xml = gbp-create-project.gresource.xml
glib_resources_namespace = gbp_create_project
include $(top_srcdir)/build/autotools/Makefile.am.gresources

include $(top_srcdir)/plugins/Makefile.plugin

endif

-include $(top_srcdir)/git.mk

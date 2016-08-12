if ENABLE_GOBJECT_TEMPLATES

DISTCLEANFILES =
BUILT_SOURCES =
CLEANFILES =
EXTRA_DIST = $(plugin_DATA)

plugindir = $(libdir)/gnome-builder/plugins
plugin_LTLIBRARIES = libgobject-templates.la
dist_plugin_DATA = gobject-templates.plugin

libgobject_templates_la_SOURCES = \
	gobject-templates-plugin.c \
	gbp-gobject-dialog.c \
	gbp-gobject-dialog.h \
	gbp-gobject-property.c \
	gbp-gobject-property.h \
	gbp-gobject-property-editor.c \
	gbp-gobject-property-editor.h \
	gbp-gobject-signal.c \
	gbp-gobject-signal.h \
	gbp-gobject-signal-editor.c \
	gbp-gobject-signal-editor.h \
	gbp-gobject-spec.c \
	gbp-gobject-spec.h \
	gbp-gobject-spec-editor.c \
	gbp-gobject-spec-editor.h \
	gbp-gobject-template.c \
	gbp-gobject-template.h \
	gbp-gobject-workbench-addin.c \
	gbp-gobject-workbench-addin.h \
	$(NULL)

nodist_libgobject_templates_la_SOURCES = \
	resources.c \
	resources.h

libgobject_templates_la_CFLAGS = \
	$(PLUGIN_CFLAGS) \
	$(NULL)

libgobject_templates_la_LDFLAGS = $(PLUGIN_LDFLAGS)

glib_resources_c = resources.c
glib_resources_h = resources.h
glib_resources_xml = gobject-templates.gresource.xml
glib_resources_namespace = gbp_gobject_templates
include $(top_srcdir)/build/autotools/Makefile.am.gresources

include $(top_srcdir)/plugins/Makefile.plugin

endif

-include $(top_srcdir)/git.mk

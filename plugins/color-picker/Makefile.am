if ENABLE_COLOR_PICKER_PLUGIN

CLEANFILES =
DISTCLEANFILES =
BUILT_SOURCES =
EXTRA_DIST = $(plugin_DATA)

plugindir = $(libdir)/gnome-builder/plugins
plugin_LTLIBRARIES = libcolor-picker-plugin.la
dist_plugin_DATA = color-picker.plugin

libcolor_picker_plugin_la_SOURCES =               \
	gb-color-picker-helper.c                  \
	gb-color-picker-helper.h                  \
	gb-color-picker-plugin.c                  \
	gb-color-picker-document-monitor.c        \
	gb-color-picker-document-monitor.h        \
	gb-color-picker-prefs.c                   \
	gb-color-picker-prefs.h                   \
	gb-color-picker-prefs-list.c              \
	gb-color-picker-prefs-palette-list.h      \
	gb-color-picker-prefs-palette-list.c      \
	gb-color-picker-prefs-list.h              \
	gb-color-picker-prefs-palette-row.c       \
	gb-color-picker-prefs-palette-row.h       \
	gb-color-picker-private.h                 \
	gb-color-picker-workbench-addin.c         \
	gb-color-picker-workbench-addin.h         \
	gb-color-picker-workbench-addin-private.h \
	$(NULL)

libcolor_picker_plugin_la_includes =   \
	-I$(top_srcdir)/contrib/gstyle \
	$(NULL)

nodist_libcolor_picker_plugin_la_SOURCES = \
	gb-color-picker-resources.c        \
	gb-color-picker-resources.h        \
	$(NULL)

libcolor_picker_plugin_la_CFLAGS = $(PLUGIN_CFLAGS) $(GSTYLE_CFLAGS)
libcolor_picker_plugin_la_LIBADD = \
	$(GSTYLE_LIBS) \
	$(top_builddir)/contrib/gstyle/libgstyle-private.la \
	$(NULL)

libcolor_picker_plugin_la_LDFLAGS = $(PLUGIN_LDFLAGS)

glib_resources_c = gb-color-picker-resources.c
glib_resources_h = gb-color-picker-resources.h
glib_resources_xml = gb-color-picker.gresource.xml
glib_resources_namespace = gb_color_picker
include $(top_srcdir)/build/autotools/Makefile.am.gresources

include $(top_srcdir)/plugins/Makefile.plugin

gsettings_SCHEMAS =                                                         \
	gsettings/org.gnome.builder.plugins.color_picker_plugin.gschema.xml \
	$(NULL)

.PRECIOUS: $(gsettings_SCHEMAS)

@GSETTINGS_RULES@

endif

-include $(top_srcdir)/git.mk

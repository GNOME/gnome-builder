if ENABLE_BEAUTIFIER_PLUGIN

CLEANFILES =
DISTCLEANFILES =
BUILT_SOURCES =
EXTRA_DIST = $(plugin_DATA)

beautifier_cflags =                                                                                  \
	$(PLUGIN_CFLAGS)                                                                             \
	-DGB_BEAUTIFIER_PLUGIN_DATADIR="\"$(datadir)/gnome-builder/plugins/beautifier_plugin/data\"" \
	$(NULL)

plugindir = $(libdir)/gnome-builder/plugins
plugin_LTLIBRARIES = libbeautifier-plugin.la
dist_plugin_DATA = beautifier.plugin

libbeautifier_plugin_la_SOURCES =          \
	gb-beautifier-config.c             \
	gb-beautifier-config.h             \
	gb-beautifier-helper.c             \
	gb-beautifier-helper.h             \
	gb-beautifier-plugin.c             \
	gb-beautifier-private.h            \
	gb-beautifier-process.c            \
	gb-beautifier-process.h            \
	gb-beautifier-workbench-addin.c    \
	gb-beautifier-workbench-addin.h    \
	$(NULL)

nodist_libbeautifier_plugin_la_SOURCES =   \
	gb-beautifier-resources.c          \
	gb-beautifier-resources.h          \
	$(NULL)

resourcedir = $(datadir)/gnome-builder/plugins/beautifier_plugin
nobase_resource_DATA =             \
	data/global.ini            \
	data/c/config.ini          \
	data/c/gnu-indent.cfg      \
	data/c/k&r.cfg             \
	data/c/linux-kernel.cfg    \
	data/c-sharp/config.ini    \
	data/c-sharp/mono.cfg      \
	data/d/config.ini          \
	data/d/d.cfg               \
	data/objc/config.ini       \
	data/objc/objc.cfg         \
	$(NULL)

EXTRA_DIST += $(nobase_resource_DATA)

libbeautifier_plugin_la_CFLAGS = $(beautifier_cflags)
libbeautifier_plugin_la_LDFLAGS = $(PLUGIN_LDFLAGS)

glib_resources_c = gb-beautifier-resources.c
glib_resources_h = gb-beautifier-resources.h
glib_resources_xml = gb-beautifier.gresource.xml
glib_resources_namespace = gb_beautifier
include $(top_srcdir)/build/autotools/Makefile.am.gresources

include $(top_srcdir)/plugins/Makefile.plugin

endif

-include $(top_srcdir)/git.mk

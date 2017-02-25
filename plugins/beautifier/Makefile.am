if ENABLE_BEAUTIFIER_PLUGIN

CLEANFILES =
DISTCLEANFILES =
BUILT_SOURCES =
EXTRA_DIST = $(plugin_DATA)

beautifier_cflags =                                                                                  \
	$(PLUGIN_CFLAGS)                                                                             \
	$(NULL)

plugindir = $(libdir)/gnome-builder/plugins
plugin_LTLIBRARIES = libbeautifier_plugin.la
dist_plugin_DATA = beautifier.plugin

libbeautifier_plugin_la_SOURCES =                                                                    \
	gb-beautifier-config.c                                                                       \
	gb-beautifier-config.h                                                                       \
	gb-beautifier-helper.c                                                                       \
	gb-beautifier-helper.h                                                                       \
	gb-beautifier-plugin.c                                                                       \
	gb-beautifier-private.h                                                                      \
	gb-beautifier-process.c                                                                      \
	gb-beautifier-process.h                                                                      \
	gb-beautifier-workbench-addin.c                                                              \
	gb-beautifier-workbench-addin.h                                                              \
	$(NULL)

nodist_libbeautifier_plugin_la_SOURCES =                                                             \
	gb-beautifier-resources.c                                                                    \
	gb-beautifier-resources.h                                                                    \
	$(NULL)

resourcedir = $(datadir)/gnome-builder/plugins
nobase_resource_DATA =                                                                               \
	beautifier_plugin/data/global.ini                                                            \
	beautifier_plugin/data/c/config.ini                                                          \
	beautifier_plugin/data/c/gnu-indent.cfg                                                      \
	beautifier_plugin/data/c/k&r.cfg                                                             \
	beautifier_plugin/data/c/linux-kernel.cfg                                                    \
	beautifier_plugin/data/c-sharp/config.ini                                                    \
	beautifier_plugin/data/c-sharp/mono.cfg                                                      \
	beautifier_plugin/data/d/config.ini                                                          \
	beautifier_plugin/data/d/d.cfg                                                               \
	beautifier_plugin/data/html/config.ini                                                       \
	beautifier_plugin/data/html/tidy-autoindent.cfg                                              \
	beautifier_plugin/data/html/tidy-indent.cfg                                                  \
	beautifier_plugin/data/objc/config.ini                                                       \
	beautifier_plugin/data/objc/objc.cfg                                                         \
	beautifier_plugin/data/python/config.ini                                                     \
	beautifier_plugin/data/xml/config.ini                                                        \
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

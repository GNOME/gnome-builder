if ENABLE_XML_PACK_PLUGIN

EXTRA_DIST = $(plugin_DATA)

plugindir = $(libdir)/gnome-builder/plugins
plugin_LTLIBRARIES = libxml-pack-plugin.la
dist_plugin_DATA = xml-pack.plugin

libxml_pack_plugin_la_SOURCES = \
	ide-xml-analysis.c \
	ide-xml-analysis.h \
	ide-xml-diagnostic-provider.c \
	ide-xml-diagnostic-provider.h \
	ide-xml-highlighter.c \
	ide-xml-highlighter.h \
	ide-xml-indenter.c \
	ide-xml-indenter.h \
	ide-xml-sax.c \
	ide-xml-sax.h \
	ide-xml-service.c \
	ide-xml-service.h \
	ide-xml-stack.c \
	ide-xml-stack.h \
	ide-xml-symbol-node.c \
	ide-xml-symbol-node.h \
	ide-xml-symbol-resolver.c \
	ide-xml-symbol-resolver.h \
	ide-xml-symbol-tree.c \
	ide-xml-symbol-tree.h \
	ide-xml-tree-builder.c \
	ide-xml-tree-builder.h \
	ide-xml-tree-builder-generic.c \
	ide-xml-tree-builder-generic.h \
	ide-xml-tree-builder-ui.c \
	ide-xml-tree-builder-ui.h \
	ide-xml-tree-builder-utils.c \
	ide-xml-tree-builder-utils-private.h \
	ide-xml.c \
	ide-xml.h \
	xml-pack-plugin.c \
	$(NULL)

libxml_pack_plugin_la_CFLAGS = $(PLUGIN_CFLAGS)
libxml_pack_plugin_la_LDFLAGS = $(PLUGIN_LDFLAGS)

include $(top_srcdir)/plugins/Makefile.plugin

endif

-include $(top_srcdir)/git.mk

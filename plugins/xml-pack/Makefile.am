EXTRA_DIST = $(plugin_DATA)

plugindir = $(libdir)/gnome-builder/plugins
plugin_LTLIBRARIES = libxml-pack-plugin.la
plugin_DATA = xml-pack.plugin

libxml_pack_plugin_la_SOURCES = \
	ide-xml-highlighter.c \
	ide-xml-highlighter.h \
	ide-xml-indenter.c \
	ide-xml-indenter.h \
	ide-xml.c \
	ide-xml.h \
	xml-pack-plugin.c \
	$(NULL)

libxml_pack_plugin_la_CFLAGS = \
	$(BUILDER_CFLAGS) \
	-I$(top_srcdir)/libide \
	-I$(top_srcdir)/contrib/egg \
	-I$(top_srcdir)/contrib/xml \
	$(NULL)

libxml_pack_plugin_la_LDFLAGS = \
	$(OPTIMIZE_LDFLAGS) \
	-avoid-version \
	-module \
	$(NULL)

-include $(top_srcdir)/git.mk

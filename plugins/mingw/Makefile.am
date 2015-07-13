EXTRA_DIST = $(plugin_DATA)

plugindir = $(libdir)/gnome-builder/plugins
plugin_LTLIBRARIES = libmingw-plugin.la
plugin_DATA = mingw.plugin

libmingw_plugin_la_SOURCES = \
	mingw-plugin.c \
	ide-mingw-device.c \
	ide-mingw-device.h \
	ide-mingw-device-provider.c \
	ide-mingw-device-provider.h \
	$(NULL)

libmingw_plugin_la_CFLAGS = \
	$(BUILDER_CFLAGS) \
	-I$(top_srcdir)/libide \
	$(NULL)

libmingw_plugin_la_LDFLAGS = \
	$(OPTIMIZE_LDFLAGS) \
	-avoid-version \
	-module \
	$(NULL)

-include $(top_srcdir)/git.mk

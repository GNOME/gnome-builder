plugindir = $(libdir)/gnome-builder/plugins
plugin_LTLIBRARIES = libfallback-plugin.la
dist_plugin_DATA = fallback.plugin

libfallback_plugin_la_SOURCES = \
	fallback-plugin.c \
	ide-directory-build-system.c \
	ide-directory-build-system.h \
	$(NULL)

libfallback_plugin_la_CFLAGS = \
	$(LIBIDE_CFLAGS) \
	$(OPTIMIZE_CFLAGS) \
	-I$(top_srcdir)/libide \
	$(NULL)

libfallback_plugin_la_LDFLAGS = \
	$(OPTIMIZE_LDFLAGS) \
	-avoid-version \
	-module \
	$(NULL)

-include $(top_srcdir)/git.mk

include $(top_srcdir)/plugins/Makefile.plugin

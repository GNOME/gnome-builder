if ENABLE_GCC_PLUGIN

EXTRA_DIST = $(plugin_DATA)

plugindir = $(libdir)/gnome-builder/plugins
plugin_LTLIBRARIES = libgcc-plugin.la
dist_plugin_DATA = gcc.plugin

libgcc_plugin_la_SOURCES = \
	gbp-gcc-pipeline-addin.c \
	gbp-gcc-pipeline-addin.h \
	gbp-gcc-plugin.c

libgcc_plugin_la_CFLAGS = $(PLUGIN_CFLAGS)
libgcc_plugin_la_LDFLAGS = $(PLUGIN_LDFLAGS)

include $(top_srcdir)/plugins/Makefile.plugin

endif

-include $(top_srcdir)/git.mk

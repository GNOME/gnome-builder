if ENABLE_GCC_PLUGIN

EXTRA_DIST = $(plugin_DATA)

plugindir = $(libdir)/gnome-builder/plugins
plugin_LTLIBRARIES = libgcc-plugin.la
dist_plugin_DATA = gcc.plugin

libgcc_plugin_la_SOURCES = \
	gbp-gcc-build-result-addin.c \
	gbp-gcc-build-result-addin.h \
	gbp-gcc-plugin.c

libgcc_plugin_la_CFLAGS = \
	$(LIBIDE_CFLAGS) \
	-I$(top_srcdir)/contrib/egg \
	-I$(top_srcdir)/libide \
	-I$(top_builddir)/libide

libgcc_plugin_la_LDFLAGS = \
	$(OPTIMIZE_LDFLAGS) \
	-avoid-version \
	-export-regex peas_register_types \
	-module

include $(top_srcdir)/plugins/Makefile.plugin

endif

-include $(top_srcdir)/git.mk

if ENABLE_PYTHON_PACK_PLUGIN

EXTRA_DIST = $(plugin_DATA)

plugindir = $(libdir)/gnome-builder/plugins
plugin_LTLIBRARIES = libpython-pack-plugin.la
dist_plugin_DATA = python-pack.plugin

libpython_pack_plugin_la_SOURCES = \
	ide-python-indenter.c \
	ide-python-indenter.h \
	ide-python-format-provider.c \
	ide-python-format-provider.h \
	python-pack-plugin.c \
	$(NULL)

libpython_pack_plugin_la_CFLAGS = \
	$(LIBIDE_CFLAGS) \
	-I$(top_srcdir)/libide \
	$(NULL)

libpython_pack_plugin_la_LDFLAGS = \
	$(OPTIMIZE_LDFLAGS) \
	-avoid-version \
	-module \
	$(NULL)

include $(top_srcdir)/plugins/Makefile.plugin

endif

-include $(top_srcdir)/git.mk

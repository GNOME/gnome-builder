if ENABLE_C_PACK_PLUGIN

EXTRA_DIST = $(plugin_DATA)

plugindir = $(libdir)/gnome-builder/plugins
plugin_LTLIBRARIES = libc-pack-plugin.la
dist_plugin_DATA = c-pack.plugin

libc_pack_plugin_la_SOURCES = \
	c-pack-plugin.c \
	c-parse-helper.c \
	c-parse-helper.h \
	ide-c-indenter.c \
	ide-c-indenter.h \
	ide-c-format-provider.c \
	ide-c-format-provider.h \
	$(NULL)

libc_pack_plugin_la_CFLAGS = \
	$(LIBIDE_CFLAGS) \
	-I$(top_srcdir)/libide \
	$(NULL)

libc_pack_plugin_la_LDFLAGS = \
	$(OPTIMIZE_LDFLAGS) \
	-avoid-version \
	-module \
	$(NULL)

include $(top_srcdir)/plugins/Makefile.plugin

endif

-include $(top_srcdir)/git.mk

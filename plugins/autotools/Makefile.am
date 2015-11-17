if ENABLE_AUTOTOOLS_PLUGIN

plugindir = $(libdir)/gnome-builder/plugins
plugin_LTLIBRARIES = libautotools-plugin.la
dist_plugin_DATA = autotools.plugin

libautotools_plugin_la_SOURCES = \
	autotools-plugin.c \
	ide-autotools-builder.c \
	ide-autotools-builder.h \
	ide-autotools-build-system.c \
	ide-autotools-build-system.h \
	ide-autotools-build-task.c \
	ide-autotools-build-task.h \
	ide-autotools-project-miner.c \
	ide-autotools-project-miner.h \
	ide-makecache.c \
	ide-makecache.h \
	ide-makecache-target.c \
	ide-makecache-target.h \
	$(NULL)

libautotools_plugin_la_CFLAGS = \
	$(LIBIDE_CFLAGS) \
	-I$(top_srcdir)/contrib/egg \
	-I$(top_srcdir)/libide \
	$(NULL)

libautotools_plugin_la_LDFLAGS = \
	-avoid-version \
	-module \
	$(NULL)

include $(top_srcdir)/plugins/Makefile.plugin

endif

-include $(top_srcdir)/git.mk

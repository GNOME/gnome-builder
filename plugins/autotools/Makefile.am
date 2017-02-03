if ENABLE_AUTOTOOLS_PLUGIN

plugindir = $(libdir)/gnome-builder/plugins
plugin_LTLIBRARIES = libautotools-plugin.la
dist_plugin_DATA = autotools.plugin

libautotools_plugin_la_SOURCES = \
	autotools-plugin.c \
	ide-autotools-application-addin.c \
	ide-autotools-application-addin.h \
	ide-autotools-autogen-stage.c \
	ide-autotools-autogen-stage.h \
	ide-autotools-build-system.c \
	ide-autotools-build-system.h \
	ide-autotools-build-target.c \
	ide-autotools-build-target.h \
	ide-autotools-makecache-stage.c \
	ide-autotools-makecache-stage.h \
	ide-autotools-pipeline-addin.c \
	ide-autotools-pipeline-addin.h \
	ide-autotools-project-miner.c \
	ide-autotools-project-miner.h \
	ide-makecache.c \
	ide-makecache.h \
	ide-makecache-target.c \
	ide-makecache-target.h \
	$(NULL)

libautotools_plugin_la_CFLAGS = $(PLUGIN_CFLAGS)
libautotools_plugin_la_LDFLAGS = $(PLUGIN_LDFLAGS)

include $(top_srcdir)/plugins/Makefile.plugin

endif

-include $(top_srcdir)/git.mk

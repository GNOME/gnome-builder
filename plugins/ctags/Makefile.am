if ENABLE_CTAGS_PLUGIN

plugindir = $(libdir)/gnome-builder/plugins
plugin_LTLIBRARIES = libctags-plugin.la
dist_plugin_DATA = ctags.plugin

libctags_plugin_la_SOURCES = \
	ide-ctags-builder.c \
	ide-ctags-builder.h \
	ide-ctags-completion-item.c \
	ide-ctags-completion-item.h \
	ide-ctags-completion-provider.c \
	ide-ctags-completion-provider.h \
	ide-ctags-completion-provider-private.h \
	ide-ctags-highlighter.c \
	ide-ctags-highlighter.h \
	ide-ctags-index.c \
	ide-ctags-index.h \
	ide-ctags-service.c \
	ide-ctags-service.h \
	ide-ctags-symbol-resolver.c \
	ide-ctags-symbol-resolver.h \
	ide-ctags-util.c \
	ide-ctags-util.h \
	ctags-plugin.c \
	$(NULL)

libctags_plugin_la_CFLAGS = \
	$(LIBIDE_CFLAGS) \
	-I$(top_srcdir)/contrib/egg \
	-I$(top_srcdir)/libide \
	-I$(top_srcdir)/libide/util \
	-I$(top_srcdir)/src/util \
	$(NULL)

libctags_plugin_la_LDFLAGS = \
	-avoid-version \
	-module \
	$(NULL)

include $(top_srcdir)/plugins/Makefile.plugin

endif

-include $(top_srcdir)/git.mk

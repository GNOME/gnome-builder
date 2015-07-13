plugindir = $(libdir)/gnome-builder/plugins
plugin_LTLIBRARIES = libctags-plugin.la
plugin_DATA = ctags.plugin

libctags_plugin_la_SOURCES = \
	ide-ctags-builder.c \
	ide-ctags-builder.h \
	ide-ctags-completion-item.c \
	ide-ctags-completion-item.h \
	ide-ctags-completion-provider.c \
	ide-ctags-completion-provider.h \
	ide-ctags-highlighter.c \
	ide-ctags-highlighter.h \
	ide-ctags-index.c \
	ide-ctags-index.h \
	ide-ctags-service.c \
	ide-ctags-service.h \
	ctags-plugin.c \
	$(NULL)

libctags_plugin_la_CFLAGS = \
	$(BUILDER_CFLAGS) \
	-I$(top_srcdir)/contrib/egg \
	-I$(top_srcdir)/libide \
	-I$(top_srcdir)/libide/util \
	$(NULL)

libctags_plugin_la_LDFLAGS = \
	-avoid-version \
	-module \
	$(NULL)

-include $(top_srcdir)/git.mk

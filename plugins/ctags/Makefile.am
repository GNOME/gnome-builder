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
	ide-ctags-symbol-node.c \
	ide-ctags-symbol-node.h \
	ide-ctags-symbol-resolver.c \
	ide-ctags-symbol-resolver.h \
	ide-ctags-symbol-tree.c \
	ide-ctags-symbol-tree.h \
	ide-ctags-util.c \
	ide-ctags-util.h \
	ctags-plugin.c \
	$(NULL)

libctags_plugin_la_CFLAGS = $(PLUGIN_CFLAGS)
libctags_plugin_la_LDFLAGS = $(PLUGIN_LDFLAGS)

include $(top_srcdir)/plugins/Makefile.plugin

endif

-include $(top_srcdir)/git.mk

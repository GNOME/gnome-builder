EXTRA_DIST = $(plugin_DATA)

plugindir = $(libdir)/gnome-builder/plugins
plugin_LTLIBRARIES = libclang-plugin.la
plugin_DATA = clang.plugin

libclang_plugin_la_SOURCES = \
	ide-clang-completion-item.c \
	ide-clang-completion-item.h \
	ide-clang-completion-provider.c \
	ide-clang-completion-provider.h \
	ide-clang-diagnostic-provider.c \
	ide-clang-diagnostic-provider.h \
	ide-clang-highlighter.c \
	ide-clang-highlighter.h \
	ide-clang-private.h \
	ide-clang-service.c \
	ide-clang-service.h \
	ide-clang-symbol-node.c \
	ide-clang-symbol-node.h \
	ide-clang-symbol-resolver.c \
	ide-clang-symbol-resolver.h \
	ide-clang-symbol-tree.c \
	ide-clang-symbol-tree.h \
	ide-clang-translation-unit.c \
	ide-clang-translation-unit.h \
	clang-plugin.c \
	$(NULL)

libclang_plugin_la_CFLAGS = \
	$(BUILDER_CFLAGS) \
	-I$(top_srcdir)/libide \
	-I$(top_srcdir)/contrib/egg \
	$(NULL)

libclang_plugin_la_LIBADD = \
	-lclang \
	$(NULL)

libclang_plugin_la_LDFLAGS = \
	$(CLANG_LDFLAGS) \
	$(OPTIMIZE_LDFLAGS) \
	-avoid-version \
	-module \
	$(NULL)

-include $(top_srcdir)/git.mk

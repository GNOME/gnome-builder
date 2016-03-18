if ENABLE_HTML_COMPLETION_PLUGIN

EXTRA_DIST = $(plugin_DATA)

plugindir = $(libdir)/gnome-builder/plugins
plugin_LTLIBRARIES = libhtml-completion-plugin.la
dist_plugin_DATA = html-completion.plugin

libhtml_completion_plugin_la_SOURCES = \
	ide-html-completion-provider.c \
	ide-html-completion-provider.h \
	$(NULL)

libhtml_completion_plugin_la_CFLAGS = $(PLUGIN_CFLAGS)
libhtml_completion_plugin_la_LDFLAGS = $(PLUGIN_LDFLAGS)

include $(top_srcdir)/plugins/Makefile.plugin

endif

-include $(top_srcdir)/git.mk

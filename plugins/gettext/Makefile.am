if ENABLE_GETTEXT_PLUGIN

EXTRA_DIST = $(plugin_DATA)

plugindir = $(libdir)/gnome-builder/plugins
plugin_LTLIBRARIES = libgettext-plugin.la
dist_plugin_DATA = gettext.plugin

libgettext_plugin_la_SOURCES =            \
	ide-gettext-diagnostic-provider.c \
	ide-gettext-diagnostic-provider.h \
	gettext-plugin.c                  \
	$(NULL)

libgettext_plugin_la_CFLAGS = $(PLUGIN_CFLAGS) $(GETTEXT_CFLAGS)
libgettext_plugin_la_LDFLAGS = $(PLUGIN_LDFLAGS)

include $(top_srcdir)/plugins/Makefile.plugin

endif

-include $(top_srcdir)/git.mk

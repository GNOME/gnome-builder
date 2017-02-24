if ENABLE_QUICK_HIGHLIGHT_PLUGIN

EXTRA_DIST = $(plugin_DATA)

plugindir = $(libdir)/gnome-builder/plugins
plugin_LTLIBRARIES = libquick-highlight-plugin.la
dist_plugin_DATA = quick-highlight.plugin

libquick_highlight_plugin_la_SOURCES =    \
	gbp-quick-highlight-plugin.c      \
	gbp-quick-highlight-view-addin.c  \
	gbp-quick-highlight-view-addin.h  \
	gbp-quick-highlight-preferences.c \
	gbp-quick-highlight-preferences.h \
	$(NULL)

libquick_highlight_plugin_la_CFLAGS = $(PLUGIN_CFLAGS)
libquick_highlight_plugin_la_LDFLAGS = $(PLUGIN_LDFLAGS)

include $(top_srcdir)/plugins/Makefile.plugin

endif

-include $(top_srcdir)/git.mk

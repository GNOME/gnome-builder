if ENABLE_GETTEXT_PLUGIN

EXTRA_DIST = $(plugin_DATA)

plugindir = $(libdir)/gnome-builder/plugins
plugin_LTLIBRARIES = libgettext-plugin.la
dist_plugin_DATA = gettext.plugin

libgettext_plugin_la_SOURCES = \
	ide-gettext-diagnostic-provider.c \
	ide-gettext-diagnostic-provider.h \
	gettext-plugin.c \
	$(NULL)

libgettext_plugin_la_CFLAGS = \
	$(LIBIDE_CFLAGS) \
	$(GETTEXT_CFLAGS) \
	-I$(top_srcdir)/libide \
	-I$(top_srcdir)/contrib/egg \
	$(NULL)

libgettext_plugin_la_LIBADD = \
	$(NULL)

libgettext_plugin_la_LDFLAGS = \
	$(GETTEXT_LDFLAGS) \
	$(OPTIMIZE_LDFLAGS) \
	-avoid-version \
	-module \
	$(NULL)

include $(top_srcdir)/plugins/Makefile.plugin

endif

-include $(top_srcdir)/git.mk

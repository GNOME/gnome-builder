if ENABLE_XDG_APP_PLUGIN

DISTCLEANFILES =
BUILT_SOURCES =
CLEANFILES =
EXTRA_DIST = $(plugin_DATA)

plugindir = $(libdir)/gnome-builder/plugins
plugin_LTLIBRARIES = libxdg-app-plugin.la
dist_plugin_DATA = xdg-app.plugin

libxdg_app_plugin_la_SOURCES = \
	gbp-xdg-runtime-provider.c \
	gbp-xdg-runtime-provider.h \
	gbp-xdg-runtime.c \
	gbp-xdg-runtime.h \
	gbp-xdg-plugin.c \
	$(NULL)

libxdg_app_plugin_la_CFLAGS = \
	$(LIBIDE_CFLAGS) \
	$(XDG_APP_CFLAGS) \
	$(OPTIMIZE_CFLAGS) \
	-I$(top_srcdir)/libide \
	$(NULL)

libxdg_app_plugin_la_LIBADD = $(XDG_APP_LIBS)

libxdg_app_plugin_la_LDFLAGS = \
	$(OPTIMIZE_LDFLAGS) \
	-avoid-version \
	-module \
	-export-regex peas_register_types \
	$(NULL)

include $(top_srcdir)/plugins/Makefile.plugin

endif

-include $(top_srcdir)/git.mk

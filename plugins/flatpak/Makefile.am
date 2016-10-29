if ENABLE_FLATPAK_PLUGIN

DISTCLEANFILES =
BUILT_SOURCES =
CLEANFILES =
EXTRA_DIST = $(plugin_DATA)

plugindir = $(libdir)/gnome-builder/plugins
plugin_LTLIBRARIES = libflatpak-plugin.la
dist_plugin_DATA = flatpak.plugin

libflatpak_plugin_la_SOURCES = \
	gbp-flatpak-runtime-provider.c \
	gbp-flatpak-runtime-provider.h \
	gbp-flatpak-runtime.c \
	gbp-flatpak-runtime.h \
	gbp-flatpak-subprocess-launcher.c \
	gbp-flatpak-subprocess-launcher.h \
	gbp-flatpak-plugin.c \
	$(NULL)

libflatpak_plugin_la_CFLAGS = $(PLUGIN_CFLAGS) $(FLATPAK_CFLAGS)
libflatpak_plugin_la_LIBADD = $(FLATPAK_LIBS)
libflatpak_plugin_la_LDFLAGS = $(PLUGIN_LDFLAGS)

include $(top_srcdir)/plugins/Makefile.plugin

endif

-include $(top_srcdir)/git.mk

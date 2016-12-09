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
	gbp-flatpak-runner.c \
	gbp-flatpak-runner.h \
	gbp-flatpak-application-addin.c \
	gbp-flatpak-application-addin.h \
	gbp-flatpak-clone-widget.c \
	gbp-flatpak-clone-widget.h \
	gbp-flatpak-genesis-addin.c \
	gbp-flatpak-genesis-addin.h \
	$(NULL)

nodist_libflatpak_plugin_la_SOURCES = \
	gbp-flatpak-resources.c \
	gbp-flatpak-resources.h

libflatpak_plugin_la_CFLAGS = $(PLUGIN_CFLAGS) $(FLATPAK_CFLAGS) $(GIT_CFLAGS)
libflatpak_plugin_la_LIBADD = $(FLATPAK_LIBS) $(GIT_LIBS)
libflatpak_plugin_la_LDFLAGS = $(PLUGIN_LDFLAGS)

glib_resources_c = gbp-flatpak-resources.c
glib_resources_h = gbp-flatpak-resources.h
glib_resources_xml = gbp-flatpak-resources.gresource.xml
glib_resources_namespace = gbp_flatpak
include $(top_srcdir)/build/autotools/Makefile.am.gresources

include $(top_srcdir)/plugins/Makefile.plugin

endif

-include $(top_srcdir)/git.mk

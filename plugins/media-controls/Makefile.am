if ENABLE_MEDIA_CONTROLS_PLUGIN

EXTRA_DIST = $(plugin_DATA)

plugindir = $(libdir)/gnome-builder/plugins
plugin_LTLIBRARIES = libmedia-controls-plugin.la
dist_plugin_DATA = media-controls.plugin

libmedia_controls_plugin_la_SOURCES = \
    media-controls.vala

libmedia_controls_plugin_la_VALAFLAGS = \
	--vapidir $(top_builddir)/libide \
    --pkg gtk+-3.0 \
    --pkg gio-2.0 \
    --pkg libide-1.0 \
    --pkg libpeas-1.0

libmedia_controls_plugin_la_CFLAGS = \
	-DG_LOG_DOMAIN=\"media-controls-plugin\" \
	$(LIBIDE_CFLAGS) \
	$(VALA_CFLAGS) \
	-I$(top_srcdir)/libide \
	-I$(top_srcdir)/contrib/egg \
    -w

libmedia_controls_plugin_la_LDFLAGS = \
	$(OPTIMIZE_LDFLAGS) \
	$(VALA_LIBS) \
    -avoid-version \
    -module

include $(top_srcdir)/plugins/Makefile.plugin

endif

-include $(top_srcdir)/git.mk

if ENABLE_GNOME_CODE_ASSISTANCE_PLUGIN

EXTRA_DIST = $(plugin_DATA)

plugindir = $(libdir)/gnome-builder/plugins
plugin_LTLIBRARIES = libgnome-code-assistance-plugin.la
dist_plugin_DATA = gnome-code-assistance.plugin

libgnome_code_assistance_plugin_la_SOURCES = \
	gca-diagnostics.c \
	gca-diagnostics.h \
	gca-service.c \
	gca-service.h \
	gca-structs.c \
	gca-structs.h \
	gca-plugin.c \
	ide-gca-diagnostic-provider.c \
	ide-gca-diagnostic-provider.h \
	ide-gca-service.c \
	ide-gca-service.h \
	$(NULL)

libgnome_code_assistance_plugin_la_CFLAGS = \
	$(LIBIDE_CFLAGS) \
	-I$(top_srcdir)/libide \
	$(NULL)

libgnome_code_assistance_plugin_la_LDFLAGS = \
	$(OPTIMIZE_LDFLAGS) \
	-avoid-version \
	-module \
	$(NULL)

include $(top_srcdir)/plugins/Makefile.plugin

endif

-include $(top_srcdir)/git.mk

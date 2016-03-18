if ENABLE_GNOME_CODE_ASSISTANCE_PLUGIN

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
	ide-gca-preferences-addin.c \
	ide-gca-preferences-addin.h \
	ide-gca-service.c \
	ide-gca-service.h \
	$(NULL)

libgnome_code_assistance_plugin_la_CFLAGS = $(PLUGIN_CFLAGS)
libgnome_code_assistance_plugin_la_LDFLAGS = $(PLUGIN_LDFLAGS)

gsettings_SCHEMAS = org.gnome.builder.gnome-code-assistance.gschema.xml

.PRECIOUS: $(gsettings_SCHEMAS)

@GSETTINGS_RULES@

EXTRA_DIST = $(gsettings_SCHEMAS) $(plugin_DATA)

include $(top_srcdir)/plugins/Makefile.plugin

endif

-include $(top_srcdir)/git.mk

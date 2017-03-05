if ENABLE_ESLINT_PLUGIN


plugindir = $(libdir)/gnome-builder/plugins
dist_plugin_DATA = eslint.plugin

moduledir = $(libdir)/gnome-builder/plugins/eslint_plugin
dist_module_DATA = eslint_plugin/__init__.py

gsettings_SCHEMAS = org.gnome.builder.plugins.eslint.gschema.xml

.PRECIOUS: $(gsettings_SCHEMAS)

@GSETTINGS_RULES@

EXTRA_DIST = $(gsettings_SCHEMAS) $(plugin_DATA)

endif

-include $(top_srcdir)/git.mk

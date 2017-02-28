if ENABLE_RUSTUP_PLUGIN


plugindir = $(libdir)/gnome-builder/plugins
dist_plugin_DATA = rustup.plugin

moduledir = $(libdir)/gnome-builder/plugins/rustup_plugin
dist_module_DATA = rustup_plugin/__init__.py

resourcedir = $(datadir)/gnome-builder/plugins/
nobase_resource_DATA = rustup_plugin/resources/rustup.sh

EXTRA_DIST = $(nobase_resource_DATA)

endif

-include $(top_srcdir)/git.mk

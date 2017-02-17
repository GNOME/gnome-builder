if ENABLE_RUSTUP_PLUGIN

EXTRA_DIST = $(plugin_DATA)

plugindir = $(libdir)/gnome-builder/plugins
dist_plugin_DATA = rustup.plugin

moduledir = $(libdir)/gnome-builder/plugins/rustup_plugin
dist_module_DATA = rustup_plugin/__init__.py

resourcedir = $(datadir)/gnome-builder/plugins/
nobase_resource_DATA = \
	rustup_plugin/resources/rustup.sh \
	$(NULL)

endif

include $(top_srcdir)/plugins/Makefile.plugin

-include $(top_srcdir)/git.mk

if ENABLE_CONTRIBUTING_PLUGIN

EXTRA_DIST = $(plugin_DATA)

plugindir = $(libdir)/gnome-builder/plugins
dist_plugin_DATA = contributing.plugin

moduledir = $(libdir)/gnome-builder/plugins/contributing_plugin
dist_module_DATA = \
	contributing_plugin/__init__.py \
	contributing_plugin/helper.py

endif

-include $(top_srcdir)/git.mk

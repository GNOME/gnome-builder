if ENABLE_HTML_PREVIEW_PLUGIN

EXTRA_DIST = $(plugin_DATA)

plugindir = $(libdir)/gnome-builder/plugins
dist_plugin_DATA = html-preview.plugin

moduledir = $(libdir)/gnome-builder/plugins/html_preview_plugin
dist_module_DATA = html_preview_plugin/__init__.py

resourcedir = $(datadir)/gnome-builder/plugins/html_preview_plugin
dist_resource_DATA = \
	html_preview_plugin/markdown.css \
	html_preview_plugin/markdown-view.js \
	html_preview_plugin/marked.js

resourcegtkdir = $(datadir)/gnome-builder/plugins/html_preview_plugin/gtk
dist_resourcegtk_DATA = html_preview_plugin/gtk/menus.ui

endif

-include $(top_srcdir)/git.mk

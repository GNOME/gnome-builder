if ENABLE_HTML_PREVIEW_PLUGIN

EXTRA_DIST = $(plugin_DATA)

plugindir = $(libdir)/gnome-builder/plugins
dist_plugin_DATA = html-preview.plugin

moduledir = $(libdir)/gnome-builder/plugins/html_preview_plugin
dist_module_DATA = \
	html_preview_plugin/__init__.py \
	html_preview_plugin/markdown.css \
	html_preview_plugin/markdown-view.js \
	html_preview_plugin/marked.js \
	$(NULL)

endif

GITIGNOREFILES = html_preview_plugin/__pycache__

-include $(top_srcdir)/git.mk

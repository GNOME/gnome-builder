EXTRA_DIST = $(plugin_DATA)

plugindir = $(libdir)/gnome-builder/plugins
dist_plugin_DATA = \
	python-gi-imports-completion.plugin \
	python_gi_imports_completion.py

GITIGNOREFILES = __pycache__

-include $(top_srcdir)/git.mk

include $(top_srcdir)/plugins/Makefile.plugin

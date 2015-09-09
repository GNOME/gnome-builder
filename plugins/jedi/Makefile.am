if ENABLE_JEDI_PLUGIN

EXTRA_DIST = $(plugin_DATA)

plugindir = $(libdir)/gnome-builder/plugins
dist_plugin_DATA = \
	jedi.plugin \
	jedi_plugin.py

GITIGNOREFILES = __pycache__

endif

-include $(top_srcdir)/git.mk

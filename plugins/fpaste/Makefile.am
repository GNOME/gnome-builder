if ENABLE_FPASTE_PLUGIN

EXTRA_DIST = $(plugin_DATA)

plugindir = $(libdir)/gnome-builder/plugins
dist_plugin_DATA = fpaste.plugin

moduledir = $(libdir)/gnome-builder/plugins/fpaste_plugin
dist_module_DATA = fpaste_plugin/__init__.py

resourcegtkdir = $(datadir)/gnome-builder/plugins/fpaste_plugin/gtk
dist_resourcegtk_DATA = fpaste_plugin/gtk/menus.ui

endif

GITIGNOREFILES = fpaste_plugin/__pycache__

-include $(top_srcdir)/git.mk


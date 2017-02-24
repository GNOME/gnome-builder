if ENABLE_MESON_TEMPLATES

plugindir = $(libdir)/gnome-builder/plugins
dist_plugin_DATA = meson-templates.plugin

moduledir = $(libdir)/gnome-builder/plugins/meson_templates
dist_module_DATA = meson_templates/__init__.py

resourcedir = $(datadir)/gnome-builder/plugins/
nobase_resource_DATA =                            \
	meson_templates/resources/meson.build     \
	meson_templates/resources/src/meson.build \
	meson_templates/resources/src/main.c      \
	meson_templates/resources/po/meson.build  \
	meson_templates/resources/po/LINGUAS      \
	meson_templates/resources/po/POTFILES

EXTRA_DIST = $(nobase_resource_DATA)

endif

-include $(top_srcdir)/git.mk

if get_option('with_meson_templates')

meson_template_datadir = join_paths(plugindatadir, 'meson_templates')
meson_template_dir = join_paths(plugindir, 'meson_templates')

install_data('meson-templates.plugin', install_dir: plugindir)
install_data('meson_templates/__init__.py', install_dir: meson_template_dir)
install_subdir('meson_templates/resources', install_dir: meson_template_datadir)

endif

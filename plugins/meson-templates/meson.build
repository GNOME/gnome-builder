if get_option('with_meson_templates')

meson_template_datadir = join_paths(plugindatadir, 'meson_templates')
meson_template_dir = join_paths(plugindir, 'meson_templates')

install_data('meson_templates/__init__.py', install_dir: meson_template_dir)
install_subdir('meson_templates/resources', install_dir: meson_template_datadir)

configure_file(
          input: 'meson-templates.plugin',
         output: 'meson-templates.plugin',
  configuration: configuration_data(),
        install: true,
    install_dir: plugindir,
)

endif

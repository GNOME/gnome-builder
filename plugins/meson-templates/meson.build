if get_option('with_meson_templates')

devhelp_resources = gnome.compile_resources(
  'meson_templates',
  'meson-templates.gresource.xml',
  gresource_bundle: true,
           install: true,
       install_dir: plugindir,
)

meson_template_dir = join_paths(plugindir, 'meson_templates')

install_data('meson_templates/__init__.py', install_dir: meson_template_dir)

configure_file(
          input: 'meson-templates.plugin',
         output: 'meson-templates.plugin',
  configuration: configuration_data(),
        install: true,
    install_dir: plugindir,
)

endif

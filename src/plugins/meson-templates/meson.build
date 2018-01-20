if get_option('with_meson_templates')

meson_templates_resources = gnome.compile_resources(
  'meson_templates',
  'meson-templates.gresource.xml',
  gresource_bundle: true,
           install: true,
       install_dir: plugindir,
)

install_data('meson_templates.py', install_dir: plugindir)

configure_file(
          input: 'meson-templates.plugin',
         output: 'meson-templates.plugin',
  configuration: configuration_data(),
        install: true,
    install_dir: plugindir,
)

endif

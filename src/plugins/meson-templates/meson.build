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
  configuration: config_h,
        install: true,
    install_dir: plugindir,
)

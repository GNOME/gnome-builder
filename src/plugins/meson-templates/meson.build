flatpak_json_data = configuration_data()
flatpak_json_data.set('gnome_sdk_version', get_option('gnome_sdk_version'))
flatpak_json = configure_file(
  input: 'resources/flatpak.json.in',
  output: 'flatpak.json',
  configuration: flatpak_json_data)

meson_templates_resources = gnome.compile_resources(
  'meson_templates',
  'meson-templates.gresource.xml',
  gresource_bundle: true,
           install: true,
       install_dir: plugindir,
      dependencies: [flatpak_json],
)

install_data('meson_templates.py', install_dir: plugindir)

configure_file(
          input: 'meson-templates.plugin',
         output: 'meson-templates.plugin',
  configuration: config_h,
        install: true,
    install_dir: plugindir,
)

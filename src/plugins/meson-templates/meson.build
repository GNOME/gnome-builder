if get_option('plugin_meson_templates')

plugins_sources += files([
  'meson-templates-plugin.c',
  'gbp-meson-template.c',
  'gbp-meson-template-provider.c',
])

flatpak_json_data = configuration_data()
flatpak_json_data.set('gnome_sdk_version', get_option('gnome_sdk_version'))
flatpak_json_data.set('freedesktop_sdk_version', get_option('freedesktop_sdk_version'))
flatpak_json = configure_file(
  input: 'resources/flatpak.json.in',
  output: 'flatpak.json',
  configuration: flatpak_json_data)

plugin_meson_templates_resources = gnome.compile_resources(
  'gbp-meson-templates',
  'meson-templates.gresource.xml',
        c_name: 'gbp_meson_templates',
  dependencies: [flatpak_json],
)

plugins_sources += plugin_meson_templates_resources

endif

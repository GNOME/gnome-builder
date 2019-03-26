if get_option('plugin_deviced')

if not get_option('plugin_flatpak')
  error('-Dplugin_flatpak=true is required to enable deviced')
endif

plugins_sources += files([
  'deviced-plugin.c',
  'gbp-deviced-deploy-strategy.c',
  'gbp-deviced-device.c',
  'gbp-deviced-device-provider.c',
])

plugin_deviced_resources = gnome.compile_resources(
  'deviced-resources',
  'deviced.gresource.xml',
  c_name: 'gbp_deviced',
)

plugins_deps += [
  dependency('libdeviced', version: '>=3.27.4'),
]

plugins_sources += plugin_deviced_resources

endif

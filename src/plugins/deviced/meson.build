if get_option('with_deviced')

deviced_resources = gnome.compile_resources(
  'deviced-resources',
  'deviced.gresource.xml',
  c_name: 'gbp_deviced',
)

deviced_sources = [
  'gbp-deviced-plugin.c',
  'gbp-deviced-deploy-strategy.c',
  'gbp-deviced-device.c',
  'gbp-deviced-device-provider.c',
]

gnome_builder_plugins_deps += [
  dependency('libdeviced', version: '>=3.27.4'),
]

gnome_builder_plugins_sources += files(deviced_sources)
gnome_builder_plugins_sources += deviced_resources[0]

endif

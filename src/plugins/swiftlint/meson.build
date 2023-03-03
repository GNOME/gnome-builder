if get_option('plugin_swiftlint')

plugins_sources += files([
  'swiftlint-plugin.c',
  'gbp-swiftlint-diagnostic-provider.c',
])

plugin_swiftlint_resources = gnome.compile_resources(
  'swiftlint-resources',
  'swiftlint.gresource.xml',
  c_name: 'gbp_swiftlint',
)

plugins_sources += plugin_swiftlint_resources

endif

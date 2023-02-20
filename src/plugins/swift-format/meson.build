if get_option('plugin_swiftformat')

plugins_sources += files([
  'swiftformat-plugin.c',
  'gbp-swiftformat-formatter.c',
])

plugin_swiftformat_resources = gnome.compile_resources(
  'swiftformat-resources',
  'swiftformat.gresource.xml',
  c_name: 'gbp_swiftformat',
)

plugins_sources += plugin_swiftformat_resources

endif

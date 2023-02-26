if get_option('plugin_swift')

plugins_sources += files([
  'swift-plugin.c',
  'gbp-swift-build-system-discovery.c',
  'gbp-swift-build-system.c',
  'gbp-swift-dependency-updater.c',
  'gbp-swift-pipeline-addin.c',
  'gbp-swift-run-command-provider.c',
])

plugin_swift_resources = gnome.compile_resources(
  'swift-resources',
  'swift.gresource.xml',
  c_name: 'gbp_swift',
)

plugins_sources += plugin_swift_resources

endif

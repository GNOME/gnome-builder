if get_option('plugin_npm')

plugins_sources += files([
  'npm-plugin.c',
  'gbp-npm-build-system-discovery.c',
  'gbp-npm-build-system.c',
  'gbp-npm-pipeline-addin.c',
  'gbp-npm-run-command-provider.c',
])

plugin_npm_resources = gnome.compile_resources(
  'npm-resources',
  'npm.gresource.xml',
  c_name: 'gbp_npm',
)

plugins_sources += plugin_npm_resources

endif

if get_option('plugin_gradle')

plugins_sources += files([
  'gradle-plugin.c',
  'gbp-gradle-build-system-discovery.c',
  'gbp-gradle-build-system.c',
  'gbp-gradle-pipeline-addin.c',
  'gbp-gradle-run-command-provider.c',
])

plugin_gradle_resources = gnome.compile_resources(
  'gradle-resources',
  'gradle.gresource.xml',
  c_name: 'gbp_gradle',
)

plugins_sources += plugin_gradle_resources

endif

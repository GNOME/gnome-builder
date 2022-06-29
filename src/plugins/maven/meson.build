if get_option('plugin_maven')

plugins_sources += files([
  'maven-plugin.c',
  'gbp-maven-build-system-discovery.c',
  'gbp-maven-build-system.c',
  'gbp-maven-pipeline-addin.c',
  'gbp-maven-run-command-provider.c',
])

plugin_maven_resources = gnome.compile_resources(
  'maven-resources',
  'maven.gresource.xml',
  c_name: 'gbp_maven',
)

plugins_sources += plugin_maven_resources

endif

plugins_sources += files([
  'buildconfig-plugin.c',
  'ide-buildconfig-configuration.c',
  'ide-buildconfig-configuration-provider.c',
  'ide-buildconfig-pipeline-addin.c',
])

plugin_buildconfig_resources = gnome.compile_resources(
  'buildconfig-resources',
  'buildconfig.gresource.xml',
  c_name: 'gbp_buildconfig',
)

plugins_sources += plugin_buildconfig_resources[0]

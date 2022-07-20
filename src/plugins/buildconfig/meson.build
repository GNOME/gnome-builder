plugins_sources += files([
  'buildconfig-plugin.c',
  'ide-buildconfig-config.c',
  'ide-buildconfig-config-provider.c',
  'ide-buildconfig-pipeline-addin.c',
  'gbp-buildconfig-run-command-provider.c',
])

plugin_buildconfig_resources = gnome.compile_resources(
  'buildconfig-resources',
  'buildconfig.gresource.xml',
  c_name: 'gbp_buildconfig',
)

plugins_sources += plugin_buildconfig_resources

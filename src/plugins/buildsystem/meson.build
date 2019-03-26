plugins_sources += files([
  'buildsystem-plugin.c',
  'gbp-buildsystem-workbench-addin.c',
])

plugin_buildsystem_resources = gnome.compile_resources(
  'buildsystem-resources',
  'buildsystem.gresource.xml',
  c_name: 'gbp_buildsystem',
)

plugins_sources += plugin_buildsystem_resources

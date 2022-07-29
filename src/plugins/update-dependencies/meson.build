plugins_sources += files([
  'update-dependencies-plugin.c',
  'gbp-update-dependencies-workbench-addin.c',
])

plugin_update_dependencies_resources = gnome.compile_resources(
  'gbp-update-dependencies-resources',
  'update-dependencies.gresource.xml',
  c_name: 'gbp_update_dependencies',
)

plugins_sources += plugin_update_dependencies_resources

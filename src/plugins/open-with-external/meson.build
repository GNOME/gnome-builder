if libportal_dep.found()

plugins_sources += files([
  'open-with-external-plugin.c',
  'gbp-owe-workbench-addin.c',
])

plugin_open_with_external_resources = gnome.compile_resources(
  'open-with-external-resources',
  'open-with-external.gresource.xml',
  c_name: 'gbp_owe',
)

plugins_deps += [libportal_dep]
plugins_sources += plugin_open_with_external_resources

endif

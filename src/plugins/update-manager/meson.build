if get_option('plugin_update_manager')

plugins_deps += [
  dependency('libportal', version: '>= 0.3'),
]

plugins_sources += files([
  'update-manager-plugin.c',
  'gbp-update-manager-app-addin.c',
])

plugin_update_manager_resources = gnome.compile_resources(
  'gbp-update-manager-resources',
  'update-manager.gresource.xml',
  c_name: 'gbp_update_manager',
)

plugins_sources += plugin_update_manager_resources

endif

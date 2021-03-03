if get_option('plugin_update_manager')
if host_machine.system() == 'linux'

if not libportal_dep.found()
  error('plugin_update_manager requires access to libportal')
endif

plugins_deps += [
  libportal_dep,
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
endif

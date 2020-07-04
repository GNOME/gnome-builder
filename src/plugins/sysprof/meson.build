if get_option('plugin_sysprof')

plugins_deps += [
  dependency('sysprof-4', version: '>= 3.37.1'),
  dependency('sysprof-ui-4', version: '>= 3.37.1'),
]

plugins_sources += files([
  'sysprof-plugin.c',
  'gbp-sysprof-surface.c',
  'gbp-sysprof-surface.h',
  'gbp-sysprof-workspace-addin.c',
  'gbp-sysprof-workspace-addin.h',
])

plugin_sysprof_resources = gnome.compile_resources(
  'sysprof-resources',
  'sysprof.gresource.xml',
  c_name: 'gbp_sysprof',
)

plugins_sources += plugin_sysprof_resources

endif

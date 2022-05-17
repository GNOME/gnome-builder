if get_option('plugin_sysprof')

plugins_deps += [
  dependency('sysprof-4', version: '>= 3.37.1'),
  dependency('sysprof-ui-5', version: '>= 3.45.0'),
]

plugins_sources += files([
  'sysprof-plugin.c',
])

plugin_sysprof_resources = gnome.compile_resources(
  'sysprof-resources',
  'sysprof.gresource.xml',
  c_name: 'gbp_sysprof',
)

plugins_sources += plugin_sysprof_resources

endif

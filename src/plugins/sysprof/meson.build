if get_option('with_sysprof')

sysprof_resources = gnome.compile_resources(
  'gbp-sysprof-resources',
  'sysprof.gresource.xml',
  c_name: 'gbp_sysprof',
)

sysprof_sources = [
  'gbp-sysprof-plugin.c',
  'gbp-sysprof-perspective.c',
  'gbp-sysprof-perspective.h',
  'gbp-sysprof-workbench-addin.c',
  'gbp-sysprof-workbench-addin.h',
]

gnome_builder_plugins_deps += [
  dependency('sysprof-2', version: '>= 3.27.91'),
  dependency('sysprof-ui-2', version: '>= 3.27.91'),
]

gnome_builder_plugins_sources += files(sysprof_sources)
gnome_builder_plugins_sources += sysprof_resources[0]

endif

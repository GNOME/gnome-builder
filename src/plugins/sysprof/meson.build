if get_option('plugin_sysprof')

libsysprof_dep = dependency('sysprof-4', version: '>= 3.45.0')
libsysprof_ui_dep = dependency('sysprof-ui-5', version: '>= 3.45.0')

plugins_deps += [
  libsysprof_dep,
  libsysprof_ui_dep,
]

plugins_sources += files([
  'sysprof-plugin.c',
  'gbp-sysprof-page.c',
  'gbp-sysprof-tool.c',
  'gbp-sysprof-workbench-addin.c',
  'gbp-sysprof-workspace-addin.c',
])

plugin_sysprof_resources = gnome.compile_resources(
  'sysprof-resources',
  'sysprof.gresource.xml',
  c_name: 'gbp_sysprof',
)

plugins_sources += plugin_sysprof_resources

install_data(['org.gnome.builder.sysprof.gschema.xml'], install_dir: schema_dir)

gnome_builder_sysprof_sources = ['gnome-builder-sysprof.c']
gnome_builder_sysprof_deps = [libsysprof_dep]
gnome_builder_sysprof = executable('gnome-builder-sysprof', gnome_builder_sysprof_sources,
           install: true,
       install_dir: get_option('libexecdir'),
      dependencies: gnome_builder_sysprof_deps,
)

endif

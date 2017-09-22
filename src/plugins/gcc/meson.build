if get_option('with_gcc')

gcc_resources = gnome.compile_resources(
  'gcc-resources',
  'gcc.gresource.xml',
  c_name: 'gbp_gcc',
)

gcc_sources = [
  'gbp-gcc-pipeline-addin.c',
  'gbp-gcc-pipeline-addin.h',
  'gbp-gcc-plugin.c',
]

gnome_builder_plugins_sources += files(gcc_sources)
gnome_builder_plugins_sources += gcc_resources[0]

endif

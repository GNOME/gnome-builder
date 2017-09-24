if get_option('with_cmake')

cmake_resources = gnome.compile_resources(
  'cmake-resources',
  'cmake.gresource.xml',
  c_name: 'gbp_cmake',
)

gnome_builder_plugins_sources += cmake_resources[0]

endif

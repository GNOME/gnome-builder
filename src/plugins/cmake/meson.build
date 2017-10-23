if get_option('with_cmake')

cmake_resources = gnome.compile_resources(
  'gbp-cmake-resources',
  'cmake.gresource.xml',
  c_name: 'gbp_cmake',
)

cmake_sources = [
  'cmake-plugin.c',
  'gbp-cmake-build-system.c',
  'gbp-cmake-build-system.h',
  'gbp-cmake-build-target.c',
  'gbp-cmake-build-target.h',
  'gbp-cmake-pipeline-addin.c',
  'gbp-cmake-pipeline-addin.h',
]

gnome_builder_plugins_sources += files(cmake_sources)
gnome_builder_plugins_sources += cmake_resources[0]

endif

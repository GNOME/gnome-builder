if get_option('plugin_cmake')

plugins_sources += files([
  'cmake-plugin.c',
  'gbp-cmake-build-stage-cross-file.c',
  'gbp-cmake-build-system.c',
  'gbp-cmake-build-system-discovery.c',
  'gbp-cmake-build-target.c',
  'gbp-cmake-build-target-provider.c',
  'gbp-cmake-pipeline-addin.c',
  'gbp-cmake-run-command-provider.c',
  'gbp-cmake-toolchain.c',
  'gbp-cmake-toolchain-provider.c',
  'gbp-cmake-build-stage-codemodel.c',
])

plugin_cmake_resources = gnome.compile_resources(
  'gbp-cmake-resources',
  'cmake.gresource.xml',
  c_name: 'gbp_cmake',
)

plugins_sources += plugin_cmake_resources

endif

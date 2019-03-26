plugins_sources += files([
  'gbp-gcc-pipeline-addin.c',
  'gbp-gcc-pipeline-addin.h',
  'gbp-gcc-toolchain-provider.c',
  'gbp-gcc-toolchain-provider.h',
  'gcc-plugin.c',
])

plugin_gcc_resources = gnome.compile_resources(
  'gcc-resources',
  'gcc.gresource.xml',
  c_name: 'gbp_gcc',
)

plugins_sources += plugin_gcc_resources

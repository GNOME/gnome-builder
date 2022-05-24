if get_option('plugin_jhbuild')

plugins_sources += files([
  'jhbuild-plugin.c',
  'gbp-jhbuild-runtime.c',
  'gbp-jhbuild-runtime-provider.c',
])

plugin_jhbuild_resources = gnome.compile_resources(
  'jhbuild-resources',
  'jhbuild.gresource.xml',
  c_name: 'gbp_jhbuild',
)

plugins_sources += plugin_jhbuild_resources

endif

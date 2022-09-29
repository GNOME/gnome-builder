if get_option('plugin_dub')

plugins_sources += files([
  'dub-plugin.c',
  'gbp-dub-build-system.c',
])

plugin_dub_resources = gnome.compile_resources(
  'dub-resources',
  'dub.gresource.xml',
  c_name: 'gbp_dub',
)

plugins_sources += plugin_dub_resources

endif

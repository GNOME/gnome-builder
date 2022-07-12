if get_option('plugin_rubocop')

plugins_sources += files([
  'rubocop-plugin.c',
  'gbp-rubocop-diagnostic-provider.c',
])

plugin_rubocop_resources = gnome.compile_resources(
  'rubocop-resources',
  'rubocop.gresource.xml',
  c_name: 'gbp_rubocop',
)

plugins_sources += plugin_rubocop_resources

endif

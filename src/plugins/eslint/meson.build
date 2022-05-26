if get_option('plugin_eslint')

plugins_sources += files([
  'eslint-plugin.c',
  'gbp-eslint-diagnostic-provider.c',
])

plugin_eslint_resources = gnome.compile_resources(
  'eslint-resources',
  'eslint.gresource.xml',
  c_name: 'gbp_eslint',
)

plugins_sources += plugin_eslint_resources

endif

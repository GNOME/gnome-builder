if get_option('plugin_stylelint')

plugins_sources += files([
  'stylelint-plugin.c',
  'gbp-stylelint-diagnostic-provider.c',
])

plugin_stylelint_resources = gnome.compile_resources(
  'stylelint-resources',
  'stylelint.gresource.xml',
  c_name: 'gbp_stylelint',
)

plugins_sources += plugin_stylelint_resources

endif

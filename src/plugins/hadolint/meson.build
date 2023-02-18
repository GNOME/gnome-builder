if get_option('plugin_hadolint')

plugins_sources += files([
  'hadolint-plugin.c',
  'gbp-hadolint-diagnostic-provider.c',
])

plugin_hadolint_resources = gnome.compile_resources(
  'hadolint-resources',
  'hadolint.gresource.xml',
  c_name: 'gbp_hadolint',
)

plugins_sources += plugin_hadolint_resources

endif

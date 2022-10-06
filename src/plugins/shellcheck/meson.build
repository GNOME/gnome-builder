if get_option('plugin_shellcheck')

plugins_sources += files([
  'shellcheck-plugin.c',
  'gbp-shellcheck-diagnostic-provider.c',
])

plugin_shellcheck_resources = gnome.compile_resources(
  'shellcheck-resources',
  'shellcheck.gresource.xml',
  c_name: 'gbp_shellcheck',
)

plugins_sources += plugin_shellcheck_resources

endif
